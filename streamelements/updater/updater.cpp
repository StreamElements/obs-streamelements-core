#include "updater.hpp"

#include "obs-streamelements-config.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QProcess>
#include <QMainWindow>
#include <QTimer>
#include <QApplication>
#include <QThread>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QList>
#include <QStatusBar>
#include <QMessageBox>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <thread>
#include <mutex>
#include <fstream>
#include <regex>
#include <ctime>

#include "networkdialog.hpp"
#include "confirmpendingupdatedialog.hpp"
#include "../StreamElementsUtils.hpp"
#include "../Version.hpp"

#ifdef APPLE
#include <sys/types.h>
#include <unistd.h>
#endif

#define GLOBAL_ENV_CONFIG_FILE_NAME "obs-studio/streamelements-env.ini"

/* UI elements */
static std::shared_ptr<NetworkDialog> s_NetworkDialog = nullptr;
static std::shared_ptr<ConfirmPendingUpdateDialog> s_ConfirmPendingUpdateDialog = nullptr;

static const char *streamelements_check_for_updates_signal_decl =
	"void streamelements_request_check_for_updates(bool allow_downgrade, bool force_install, bool allow_use_last_response)";
static const char *streamelements_check_for_updates_signal_id =
	"streamelements_request_check_for_updates";

static const char *streamelements_check_for_updates_silent_signal_decl =
	"void streamelements_request_check_for_updates_silent(bool allow_downgrade, bool force_install, bool allow_use_last_response)";
static const char *streamelements_check_for_updates_silent_signal_id =
	"streamelements_request_check_for_updates_silent";

static os_event_t *streamelements_check_for_updates_event;
static bool streamelements_updater_is_running = false;

///
// Get registry key path for configuration values
//
// @param productName	name of the product to get registry path for
// @return resulting string
//
static std::string GetEnvironmentRegKeyPath(const char *productName = nullptr)
{
	std::string REG_KEY_PATH = "SOFTWARE\\StreamElements";
	if (productName && productName[0]) {
		REG_KEY_PATH += "\\";
		REG_KEY_PATH += productName;
	}

	return REG_KEY_PATH;
}

inline static void ShowStatusBarMessage(std::string msg, bool showMsgBox,
					int timeout = 3000)
{
	if (!streamelements_updater_is_running)
		return;

	blog(LOG_INFO, "obs-streamelements: ShowStatusBarMessage: %s",
	     msg.c_str());

	struct local_context {
		std::string text;
		bool showMsgBox;
		int timeout;
	};

	local_context *context = new local_context();
	context->text = msg;
	;
	context->showMsgBox = showMsgBox;
	context->timeout = timeout;

	QtPostTask(
		[context]() {
			if (streamelements_updater_is_running) {
				QMainWindow *mainWindow =
					(QMainWindow *)obs_frontend_get_main_window();
				mainWindow->statusBar()->showMessage(
					("SE.Live: " + context->text).c_str(),
					context->timeout);

				if (context->showMsgBox) {
					QMessageBox msgBox;
					msgBox.setWindowTitle(
						"StreamElements: The Ultimate Streamer Platform"),
						msgBox.setText(
							QString(context->text.c_str()));
					msgBox.exec();
				}
			}

			delete context;
	});
}

///
// Read configuration string from registry
//
// @param regValueName	value name
// @param productName	product name to read environment for
// @param defaultValue	default value to return
// @return resulting string
//
static std::string ReadEnvironmentConfigString(const char *regValueName,
					       const char *productName,
					       std::string defaultValue = "")
{
	std::string result = defaultValue;

#ifdef _WIN32
	std::string REG_KEY_PATH = GetEnvironmentRegKeyPath(productName);

	DWORD bufLen = 16384;
	char *buffer = new char[bufLen];

	LSTATUS lResult = RegGetValueA(HKEY_LOCAL_MACHINE, REG_KEY_PATH.c_str(),
				       regValueName,
				       RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY,
				       NULL, buffer, &bufLen);

	if (lResult != ERROR_SUCCESS) {
		lResult = RegGetValueA(HKEY_LOCAL_MACHINE, REG_KEY_PATH.c_str(),
				       regValueName,
				       RRF_RT_REG_SZ | RRF_SUBKEY_WOW6432KEY,
				       NULL, buffer, &bufLen);
	}

	if (ERROR_SUCCESS == lResult) {
		result = buffer;
	}

	delete[] buffer;
#else
    config_t *config;

    char *filePath = os_get_config_path_ptr(GLOBAL_ENV_CONFIG_FILE_NAME);
    config_open(&config, filePath, CONFIG_OPEN_ALWAYS);
    bfree(filePath);

    const char* str = config_get_string(config, productName ? productName : "Global", regValueName);

    if (str) {
        result = str;
    }

    config_close(config);
#endif
    
	return result;
}

///
// Replace substring
//
// @param input		input text
// @param what		text to replace
// @param with		text to replace with
// @return resulting string
//
static std::string replace_substr(std::string input, const char *what,
				   const char *with)
{
	std::string result = input;

	size_t what_len = strlen(what);

	// While we can find @what, replace it with @with
	for (size_t index = result.find(what, 0); index != std::string::npos;
	     index = result.find(what, 0)) {
		result = result.replace(index, what_len, with);
	}

	return result;
}

///
// Transform update manifest URL
//
// This method will add a cachebusting query string argument
//
// @param input input update manifest URL
// @return transformed update manifest url
//
static std::string transform_update_manifest_url(std::string input)
{
	std::string result = input;

	if (result.find("?", 0) == std::string::npos) {
		result += "?";
	} else {
		result += "&";
	}

	result += "_nocache=";

	std::time_t t = std::time(nullptr);
	char buf[64];
	std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", std::gmtime(&t));

	result += buf;

	return result;
}

///
// Check that OBS is not streaming/recording.
// If it does - ask the user to stop.
//
// If user refuses: return false
//
bool ensure_no_output(bool silent)
{
	if (!obs_frontend_streaming_active() && !obs_frontend_recording_active()) {
		return true;
	}

	QtExecSync([=]() {
		ShowStatusBarMessage(
			"SE.Live update is available, but streaming and/or recording are active.", !silent);
	});

	return false;
}

///
// Get obs-browser-SE plugin version number by loading the plugin
// module and calling streamelements_get_version_number() -> int64_t
//
// @return installed obs-browser-SE version number or 0 on failure
//
static int64_t get_installed_streamelements_core_plugin_version_number()
{
	blog(LOG_INFO,
			"obs-streamelements-core: updater: detected obs-streamelements-core version: %llu",
			(unsigned long long)STREAMELEMENTS_PLUGIN_VERSION);

	return STREAMELEMENTS_PLUGIN_VERSION;
}

static int64_t get_installed_plugin_version_number() {
	return get_installed_streamelements_core_plugin_version_number();
}

///
// Reset "don't ask me again" state
//
static void reset_auto_use_last_response()
{
	config_t *config = NULL;

	// Read config file
	char *configFilePath = obs_module_get_config_path(
		obs_current_module(), STREAMELEMENTS_UPDATE_CONFIG_FILENAME);

	config_open(&config, configFilePath, CONFIG_OPEN_ALWAYS);

	bfree(configFilePath);

	config_set_bool(config, "update-prompt", "auto_use_last_response",
			false);

	config_save_safe(config, "tmp", "bak");

	config_close(config);
	config = NULL;
}

///
// Asynchronously download and install update package
//
// @param package_url		update package EXE url
// @param package_arguments	update package EXE command-line args
//
static void download_update_package_async(std::string package_url,
					  std::string package_arguments,
					  void (*callback)())
{
	if (!streamelements_updater_is_running) {
		callback();

		return;
	}

	// We'll use this to pass data to download result handler callback
	struct local_context {
		char *packageFilePath;
		std::string package_arguments;
		std::string package_url;
		void (*callback)();
	};

	local_context *context = new local_context();

	context->callback = callback;

#ifdef __APPLE__
    // Build local package file path
    char pidbuf[32];
    sprintf(pidbuf, "%d", getpid());
    context->packageFilePath = (char*)bmalloc(1024);
    sprintf(context->packageFilePath, "/tmp/obs.%d.%s", getpid(), STREAMELEMENTS_UPDATE_PACKAGE_FILENAME);
#else
	// Build local package file path
	context->packageFilePath = obs_module_get_config_path(
		obs_current_module(), STREAMELEMENTS_UPDATE_PACKAGE_FILENAME);
#endif
    
	// Save args to local context
	context->package_url = package_url;

	context->package_arguments = package_arguments;

	// Clean up previous package file if exists
	if (os_file_exists(context->packageFilePath))
		os_unlink(context->packageFilePath);

	ShowStatusBarMessage("Downloading software update...", false);

	// Initiate async download of update package EXE file
	s_NetworkDialog->DownloadFileAsync(
		context->packageFilePath, context->package_url.c_str(), true,
		[](bool download_success, void *param) {
			// Download result

			local_context *context = (local_context *)param;

			// Check for successful download
			if (download_success) {
				ShowStatusBarMessage(
					"Software update package downloaded, starting update process...",
					false);

#ifdef __APPLE__
                std::string command = "/usr/bin/open \"";
                command += context->packageFilePath;
                command += "\"";
#else
				// Build update package command line
				std::string command = "\"";
				command += context->packageFilePath;
				command += "\"";

				if (context->package_arguments.size() > 0) {
					command += " ";
					command += context->package_arguments;
				}
#endif

                blog(LOG_INFO, "obs-streamelements-core: updater: ready to start update process: %s", command.c_str());

				QStringList tokens = QProcess::splitCommand(QString(command.c_str()));
				command = tokens[0].toStdString();
				tokens.erase(tokens.begin());

                // Start update package process
				QProcess proc;
				if (proc.startDetached(
					    QString(command.c_str()), tokens)) {
					// Successfully started update process
                    blog(LOG_INFO, "obs-streamelements-core: updater: started update process: %s", command.c_str());

					// Quit OBS
					// QApplication::quit();
					QMetaObject::invokeMethod(
						(QMainWindow*) obs_frontend_get_main_window(),
						"close",
						Qt::QueuedConnection);
				} else {
					// Failed starting update process
#ifdef _WIN32
					wchar_t *wmsg = NULL;
					wchar_t *wtitle = NULL;
					os_utf8_to_wcs_ptr(
						Message_UpdateFailed_Title, 0,
						&wmsg);
					os_utf8_to_wcs_ptr(
						Message_UpdateFailed_Text, 0,
						&wtitle);

                    QMainWindow *obs_main_window = (QMainWindow *)
                        obs_frontend_get_main_window();

					::MessageBoxW(
						(HWND)obs_main_window->winId(),
						wmsg, wtitle,
						MB_ICONEXCLAMATION);

					bfree(wtitle);
					bfree(wmsg);
#else
                    blog(LOG_ERROR, "obs-streamelements-core: updater: failed starting update process");
                    
                    QtExecSync([]() {
                        QMainWindow *obs_main_window = (QMainWindow *)
                            obs_frontend_get_main_window();

                        QMessageBox::critical(obs_main_window, Message_UpdateFailed_Title, Message_UpdateFailed_Text);
                    });
#endif
                }

				context->callback();
			} else {
				ShowStatusBarMessage(
					"Failed downloading software update package.",
					true);

				reset_auto_use_last_response();

				context->callback();
			}

			// Free packageFilePath & context memory
			bfree(context->packageFilePath);

			delete context;
		},
		context, Message_DownloadingUpdatePackage);
}

///
// Prompt the user for update or re-use previous answer if the user
// checked "don't ask me again" the last time they responded to the
// update dialog.
//
// @return	true to update, false to skip
//
static bool prompt_for_update(const bool allowUseLastResponse,
			      const uint64_t avail_version_number,
			      std::string release_notes)
{
	if (!streamelements_updater_is_running)
		return false;

	bool result = false;

	config_t *config = NULL;

	// Read config file
	char *configFilePath = obs_module_get_config_path(
		obs_current_module(), STREAMELEMENTS_UPDATE_CONFIG_FILENAME);

	config_open(&config, configFilePath, CONFIG_OPEN_ALWAYS);

	bfree(configFilePath);

	// Set defaults
	config_set_default_bool(config, "update-prompt", "last_response", true);

	config_set_default_bool(config, "update-prompt",
				"auto_use_last_response", false);

	// Remove legacy values
	config_remove_value(config, "update-prompt", "last_response"); // replaced by "skip_version"
	config_remove_value(
		config, "update-prompt", "auto_use_last_response"); // replaced by "skip_version"

	if (allowUseLastResponse) {
		uint64_t skip_version = config_get_uint(config, "update-prompt",
							"skip_version");

		if (avail_version_number == skip_version)
			return false;
	}

	// Prompt the user with available update
	s_ConfirmPendingUpdateDialog->SetReleaseNotes(release_notes);

	int confirmPendingUpdateReturnVal =
		s_ConfirmPendingUpdateDialog->ExecDialog();

	result = confirmPendingUpdateReturnVal == QDialog::Accepted;

	if (s_ConfirmPendingUpdateDialog->IsSkipVersionClicked()) {
		// Save skipped version for next time
		config_set_uint(config, "update-prompt", "skip_version",
				avail_version_number);
	} else {
		// Remove skipped version for next time
		config_remove_value(config, "update-prompt", "skip_version");
	}

	config_save_safe(config, "tmp", "bak");

	config_close(config);
	config = NULL;

	return result;
}

///
// Asynchronously download update package manifest and check if
// an update is pending.
// If so, proceed to prompting for update and downloading/installing
// the pending update package.
//
static void check_for_updates_async(bool allowUseLastResponse, bool silent,
				    bool allowDowngrade,
				    bool forceInstall, void (*callback)())
{
	ShowStatusBarMessage("Checking for updates...", false);

	struct local_context {
		char *manifestFilePath;
		void (*callback)();
		bool allowUseLastResponse = true;
		bool silent = false;
		bool allowDowngrade = false;
		bool forceInstall = false;
	};

	local_context *context = new local_context();

	context->callback = callback;
	context->allowUseLastResponse = allowUseLastResponse;
	context->silent = silent;
	context->allowDowngrade = allowDowngrade;
	context->forceInstall = forceInstall;

	// Build update manifest file path
	context->manifestFilePath = obs_module_get_config_path(
		obs_current_module(), STREAMELEMENTS_UPDATE_MANIFEST_FILENAME);

    char *config_folder_path =
        obs_module_get_config_path(obs_current_module(), "");

    blog(LOG_INFO, "obs-streamelements-core: updater: context->manifestFilePath: %s", context->manifestFilePath);
    blog(LOG_INFO, "obs-streamelements-core: updater: config_folder_path: %s", config_folder_path);

    os_mkdirs(config_folder_path);

    bfree(config_folder_path);
    
	config_t *probe_manifest;
	if (CONFIG_SUCCESS == config_open(&probe_manifest,
					  context->manifestFilePath,
					  CONFIG_OPEN_ALWAYS)) {
		config_close(probe_manifest);
		probe_manifest = nullptr;
	}

	// Delete previous manifest if exists
	if (os_file_exists(context->manifestFilePath))
		os_unlink(context->manifestFilePath);

	/*
	std::string productQuality =
		ReadEnvironmentConfigString(
			"Quality",
			STREAMELEMENTS_PRODUCT_NAME,
			STREAMELEMENTS_DEFAULT_UPDATE_MANIFEST_URL);
	*/

	std::string updateManifestUrl = ReadEnvironmentConfigString(
		"ManifestUrl", STREAMELEMENTS_PRODUCT_NAME,
		STREAMELEMENTS_DEFAULT_UPDATE_MANIFEST_URL);

	updateManifestUrl = transform_update_manifest_url(updateManifestUrl);

	// Initiate async manifest download
	s_NetworkDialog->DownloadFileAsync(
		context->manifestFilePath, updateManifestUrl.c_str(), false,
		[](bool download_success, void *param) -> void {
			// Download completed or failed

			local_context *context = (local_context *)param;

			config_t *manifest = NULL;

			if (download_success &&
			    streamelements_updater_is_running) {
				// Open manifest as INI file
				if (CONFIG_SUCCESS ==
				    config_open(&manifest,
						context->manifestFilePath,
						CONFIG_OPEN_EXISTING)) {
					// Get available version number string
					const char *version_number_str =
						config_get_string(
							manifest, "obs-browser",
							"version_number");

					bool force_install = config_get_bool(
						manifest, "obs-browser",
							"force_install") ||
						context->forceInstall;

					bool allow_downgrade = config_get_bool(
						manifest, "obs-browser",
							"allow_downgrade") ||
						context->allowDowngrade;

					bool disable_auto_use_last_response =
						config_get_bool(
							manifest, "obs-browser",
							"disable_auto_use_last_response");

					if (version_number_str) {
						// Get installed version number
						int64_t installed_version_number =
							get_installed_plugin_version_number();

						// Get available version number
						int64_t avail_version_number =
							(int64_t)atoll(
								version_number_str);

						bool update_available = false;

						if (avail_version_number >
						    installed_version_number) {
							update_available = true;
						} else if (
							avail_version_number !=
							installed_version_number) {
							update_available =
								allow_downgrade;
						}

						if (update_available) {
							ShowStatusBarMessage(
								"Software update available.",
								false);

							std::string package_url =
								"";

						// Update available, read package info
#ifdef _WIN64
							if (config_has_user_value(
								    manifest,
								    "obs-browser",
								    "package_url_64")) {
								package_url = config_get_string(
									manifest,
									"obs-browser",
									"package_url_64");
							}
#else
							if (config_has_user_value(
								    manifest,
								    "obs-browser",
								    "package_url_32")) {
								package_url = config_get_string(
									manifest,
									"obs-browser",
									"package_url_32");
							}
#endif
							else {
                                if (config_has_user_value(
                                        manifest,
                                        "obs-browser",
                                        "package_url")) {
                                    package_url = config_get_string(
                                        manifest,
                                        "obs-browser",
                                        "package_url");
                                } else {
                                    package_url = "";
                                }
							}

							package_url = replace_substr(
								package_url,
								"${ARCH_OS}",
#ifdef __APPLE__
                                "macos"
#else
                                "windows"
#endif
                            );

                            package_url = replace_substr(
								package_url,
								"${ARCH_BITS}",
								sizeof(void*) == 4 ? "32" : "64");

                            std::string package_arguments = "";
                            if (config_has_user_value(
                                    manifest,
                                    "obs-browser",
                                    "package_arguments_g2")) {
                                package_arguments =
                                    config_get_string(
                                        manifest,
                                        "obs-browser",
                                        "package_arguments_g2");
                            }
                            
							// Get absolute path to our OBS module, we'll
							// use it to build the destination folder path
							char *abs_module_dl_path = os_get_abs_path_ptr(
								obs_get_module_binary_path(
									obs_current_module()));

							std::string module_dl_path =
								abs_module_dl_path;

							bfree(abs_module_dl_path);

							// Replace '/' with '\\'
							module_dl_path =
								replace_substr(
									module_dl_path,
									"/",
									"\\");

							// Remove last 3 path components
							for (int i = 0; i < 3;
							     ++i) {
								module_dl_path =
									module_dl_path
										.substr(0,
											module_dl_path
												.find_last_of(
													'\\'));
							}

							// Replace '${OBS_INSTALL_DIR}' with module_dl_path
							package_arguments = replace_substr(
								package_arguments,
								"${OBS_INSTALL_DIR}",
								module_dl_path
									.c_str());

							// Extract release notes
							std::string
								release_notes;

							{
								const std::string
									RELEASE_NOTES_BEGIN =
										"[[[BEGIN_RELEASE_NOTES]]]";
								const std::string
									RELEASE_NOTES_END =
										"[[[END_RELEASE_NOTES]]]";

								bool reading_release_notes =
									false;

								std::ifstream file(
									context->manifestFilePath,
									std::ifstream::
										in);
								for (std::string
									     line;
								     std::getline(
									     file,
									     line);) {
									// Trim line end
									line.erase(
										line.find_last_not_of(
											" \n\r\t") +
										1);

									if (!reading_release_notes) {
										if (line ==
										    RELEASE_NOTES_BEGIN) {
											reading_release_notes =
												true;
										}
									} else {
										if (line ==
										    RELEASE_NOTES_END) {
											reading_release_notes =
												false;
										} else {
											release_notes +=
												line;
											release_notes +=
												"\n";
										}
									}
								}
							}

							if (package_url.size()) {
								if (!ensure_no_output(context->silent)) {
									blog(LOG_INFO, "obs-streamelements-core: updater: can not update while streaming and/or recording are active");

									context->callback();

									return;
								}

								bool do_update =
									force_install;

								if (!do_update) {
									bool allowUseLastResponse =
										context->allowUseLastResponse;

									if (disable_auto_use_last_response) {
										allowUseLastResponse =
											false;
									}

									// Prompt for available update & download install package if
									// user accepted the prompt
									do_update = prompt_for_update(
										allowUseLastResponse,
										avail_version_number,
										release_notes);
								}

								if (do_update) {
									download_update_package_async(
										package_url,
										package_arguments,
										context->callback);
								} else {
									context->callback();
								}
							} else {
								ShowStatusBarMessage(
									"Invalid software update manifest.",
									false);
							}
						} else {
							ShowStatusBarMessage(
								"No software update available.",
								!context->allowUseLastResponse &&
									!context->silent);

							context->callback();
						}
					} else {
						ShowStatusBarMessage(
							"Update package manifest missing update package version.",
							!context->allowUseLastResponse &&
								!context->silent);

						context->callback();
					}

					config_close(manifest);
					manifest = NULL;
				} else {
					ShowStatusBarMessage(
						"Invalid update package manifest received.",
						!context->allowUseLastResponse &&
							!context->silent);

					context->callback();
				}
			} else {
				ShowStatusBarMessage(
					"Failed connecting to the updates server.",
					!context->allowUseLastResponse &&
						!context->silent);

				context->callback();
			}

			// Free manifest file path & context memory
			bfree(context->manifestFilePath);
			delete context;
		},
		context, Message_DownloadingUpdateManifest);
}

static void check_for_updates(bool allowUseLastResponse, bool silent, bool allowDowngrade, bool forceInstall)
{
	if (0 == os_event_try(streamelements_check_for_updates_event)) {
		check_for_updates_async(
			allowUseLastResponse, silent, allowDowngrade,
			forceInstall, []() {
			os_event_signal(streamelements_check_for_updates_event);
		});
	}
}

static void streamelements_check_for_updates_signal_callback(void *,
							     calldata_t *cd)
{
	bool allowDowngrade = false;
	bool forceInstall = false;
	bool allowUseLastResponse = false;

	calldata_get_bool(cd, "allow_downgrade", &allowDowngrade);
	calldata_get_bool(cd, "force_install", &forceInstall);
	calldata_get_bool(cd, "allow_use_last_response", &allowUseLastResponse);

	check_for_updates(allowUseLastResponse, false, allowDowngrade,
			  forceInstall);
}

static void
streamelements_check_for_updates_silent_signal_callback(void *, calldata_t *cd)
{
	bool allowDowngrade = false;
	bool forceInstall = false;
	bool allowUseLastResponse = true;

	calldata_get_bool(cd, "allow_downgrade", &allowDowngrade);
	calldata_get_bool(cd, "force_install", &forceInstall);
	calldata_get_bool(cd, "allow_use_last_response", &allowUseLastResponse);

	check_for_updates(allowUseLastResponse, true, allowDowngrade,
			  forceInstall);
}

///
// Called by OBS upon module load
//
// @return	true on success, otherwise false
bool streamelements_updater_init(void)
{
	blog(LOG_INFO, "obs-streamelements-core: updater: initializing");
	os_event_init(&streamelements_check_for_updates_event,
		      OS_EVENT_TYPE_AUTO);
	os_event_signal(streamelements_check_for_updates_event);

	QtExecSync([]() {
		// Set up UI elements
		QMainWindow *obs_main_window =
			(QMainWindow *)obs_frontend_get_main_window();

		// obs_frontend_push_ui_translation(obs_module_get_string);

		// Download UI
		s_NetworkDialog = std::make_shared<NetworkDialog>(obs_main_window);

		// Update available prompt UI
		s_ConfirmPendingUpdateDialog = std::make_shared<ConfirmPendingUpdateDialog>(obs_main_window);

		// obs_frontend_pop_ui_translation();
	});

	streamelements_updater_is_running = true;

	blog(LOG_INFO,
	     "obs-streamelements-core: updater: checking for updates");
	check_for_updates(true, false, false, false);

	blog(LOG_INFO,
	     "obs-streamelements-core: updater: creating signal handlers");
	signal_handler_add(obs_get_signal_handler(),
			   streamelements_check_for_updates_signal_decl);
	signal_handler_add(obs_get_signal_handler(),
			   streamelements_check_for_updates_silent_signal_decl);

	blog(LOG_INFO,
	     "obs-streamelements-core: updater: connecting signal handlers");
	signal_handler_connect(obs_get_signal_handler(),
			       streamelements_check_for_updates_signal_id,
			       streamelements_check_for_updates_signal_callback,
			       nullptr);

	signal_handler_connect(
		obs_get_signal_handler(),
		streamelements_check_for_updates_silent_signal_id,
		streamelements_check_for_updates_silent_signal_callback,
		nullptr);

	blog(LOG_INFO, "obs-streamelements-core: updater: initialized");
	return true;
}

void streamelements_updater_shutdown(void)
{
	streamelements_updater_is_running = false;

	signal_handler_disconnect(
		obs_get_signal_handler(),
		streamelements_check_for_updates_silent_signal_id,
		streamelements_check_for_updates_silent_signal_callback,
		nullptr);

	signal_handler_disconnect(
		obs_get_signal_handler(),
		streamelements_check_for_updates_signal_id,
		streamelements_check_for_updates_signal_callback, nullptr);

	os_event_wait(streamelements_check_for_updates_event);

	os_event_destroy(streamelements_check_for_updates_event);
	streamelements_check_for_updates_event = nullptr;

	s_NetworkDialog = nullptr;
	s_ConfirmPendingUpdateDialog = nullptr;
}
