#include "StreamElementsReportIssueDialog.hpp"
#include "StreamElementsProgressDialog.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsNetworkDialog.hpp"
#include "StreamElementsConfig.hpp"
#include "Version.hpp"
#include "ui_StreamElementsReportIssueDialog.h"

#include "deps/zip/zip.h"

#include <sys/stat.h>

#include <obs.h>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include "cef-headers.hpp"

#ifdef WIN32
#include <windows.h>
#endif

#include <thread>
#include <iostream>
#include <filesystem>
#include <stdio.h>
#include <fcntl.h>
#ifdef WIN32
#include <io.h>
#else
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <time.h>
#endif

#include <QMessageBox>
#include <QScreen>
#include <QPixmap>
#include <QWindow>
#include <QMainWindow>
#include <QByteArray>
#include <QBuffer>
#include <QDesktopWidget>

#include <string>
#include <regex>

#ifndef BYTE
	typedef unsigned char BYTE;
#endif

static std::string get_newest_file(const char *location)
{
	char *basePathPtr = os_get_config_path_ptr(location);
	std::string logDir(basePathPtr);
	bfree(basePathPtr);

	std::string newestLog;
	time_t newest_ts = 0;
	struct os_dirent *entry;

	unsigned int maxLogs = (unsigned int)config_get_uint(
		obs_frontend_get_global_config(), "General", "MaxLogs");

	os_dir_t *dir = os_opendir(logDir.c_str());
	if (dir) {
		unsigned int count = 0;

		while ((entry = os_readdir(dir)) != NULL) {
			if (entry->directory || *entry->d_name == '.')
				continue;

			std::string filePath =
				logDir + "/" + std::string(entry->d_name);
			struct stat st;
			if (0 == os_stat(filePath.c_str(), &st)) {
				time_t ts = st.st_ctime;

				if (ts) {
					if (ts > newest_ts) {
						newestLog = filePath;
						newest_ts = ts;
					}

					count++;
				}
			}
		}

		os_closedir(dir);
	}

	return newestLog;
}

#pragma optimize("", off)
static double GetCpuCoreBenchmark(const uint64_t CPU_BENCH_TOTAL, uint64_t& cpu_bench_delta)
{
	uint64_t cpu_bench_begin = os_gettime_ns();
	//const uint64_t CPU_BENCH_TOTAL = 10000000;
	uint64_t cpu_bench_accumulator = 2L;

	for (uint64_t bench = 0; bench < CPU_BENCH_TOTAL; ++bench) {
		cpu_bench_accumulator *= cpu_bench_accumulator;
	}

	uint64_t cpu_bench_end = os_gettime_ns();
	cpu_bench_delta = cpu_bench_end - cpu_bench_begin;
	double cpu_benchmark = (double)CPU_BENCH_TOTAL / (double)cpu_bench_delta;

	return cpu_benchmark * (double)100.0;
}
#pragma optimize("", on)

StreamElementsReportIssueDialog::StreamElementsReportIssueDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::StreamElementsReportIssueDialog)
{
    ui->setupUi(this);

    ui->txtIssue->setTabChangesFocus(true);
    ui->txtIssue->setWordWrapMode(QTextOption::WordWrap);

    // Disable context help button
    this->setWindowFlags((
	    (windowFlags() | Qt::CustomizeWindowHint)
	    & ~Qt::WindowContextHelpButtonHint
	    ));
}

StreamElementsReportIssueDialog::~StreamElementsReportIssueDialog()
{
    delete ui;
}

void StreamElementsReportIssueDialog::update()
{
	QDialog::update();

	ui->cmdOK->setEnabled(ui->txtIssue->toPlainText().trimmed().size());
}

void StreamElementsReportIssueDialog::accept()
{
	std::string tempBufPath;

	ui->txtIssue->setEnabled(false);
	ui->cmdOK->setEnabled(false);

	obs_frontend_push_ui_translation(obs_module_get_string);

	StreamElementsProgressDialog dialog(this);

	obs_frontend_pop_ui_translation();

	dialog.setEnableCancel(false);

	std::thread thread([&]() {
		bool collect_all = ui->checkCollectLogsAndSettings->isChecked();
		std::string descriptionText = wstring_to_utf8(
			ui->txtIssue->toPlainText().trimmed().toStdWString());

		const size_t BUF_LEN = 2048;
#ifdef WIN32
		wchar_t pathBuffer[BUF_LEN];

		if (!::GetTempPathW(BUF_LEN, pathBuffer)) {
			QMessageBox::warning(
				this,
				obs_module_text("StreamElements.ReportIssue.Error.Generic.Title"),
				obs_module_text("StreamElements.ReportIssue.Error.GetTempPath.Text"),
				QMessageBox::Ok);

			return;
		}

		std::wstring wtempBufPath(pathBuffer);

		if (0 == ::GetTempFileNameW(wtempBufPath.c_str(), L"obs-live-error-report-data", 0, pathBuffer)) {
			QMessageBox::warning(
				this,
				obs_module_text("StreamElements.ReportIssue.Error.Generic.Title"),
				obs_module_text("StreamElements.ReportIssue.Error.GetTempFileName.Text"),
				QMessageBox::Ok);

			return;
		}

		wtempBufPath = pathBuffer;
		wtempBufPath += L".zip";

		tempBufPath = wstring_to_utf8(wtempBufPath);
#else
		char pid_buf[32];
		sprintf(pid_buf, "%d", getpid());

		tempBufPath = "/tmp/";
		tempBufPath += pid_buf;
		tempBufPath += ".zip";
#endif

		char programDataPathBuf[BUF_LEN];
		int ret = os_get_config_path(programDataPathBuf, BUF_LEN, "obs-studio");

		if (ret <= 0) {
			QMessageBox::warning(
				this,
				obs_module_text("StreamElements.ReportIssue.Error.Generic.Title"),
				obs_module_text("StreamElements.ReportIssue.Error.GetObsDataPath.Text"),
				QMessageBox::Ok);

			return;
		}

		std::wstring obsDataPath = QString(programDataPathBuf).toStdWString();

		zip_t* zip = zip_open(tempBufPath.c_str(), 9, 'w');

		auto addBufferToZip = [&](BYTE* buf, size_t bufLen, std::wstring zipPath)
		{
			zip_entry_open(zip, wstring_to_utf8(zipPath).c_str());

			zip_entry_write(zip, buf, bufLen);

			zip_entry_close(zip);
		};

		auto addLinesBufferToZip = [&](std::vector<std::string>& lines, std::wstring zipPath)
		{
			zip_entry_open(zip, wstring_to_utf8(zipPath).c_str());

			for (auto line : lines) {
				zip_entry_write(zip, line.c_str(), line.size());
				zip_entry_write(zip, "\r\n", 2);
			}

			zip_entry_close(zip);
		};

		auto addCefValueToZip = [&](CefRefPtr<CefValue>& input, std::wstring zipPath)
		{
			std::string buf =
				wstring_to_utf8(
					CefWriteJSON(
						input,
						JSON_WRITER_PRETTY_PRINT)
					.ToWString());

			zip_entry_open(zip, wstring_to_utf8(zipPath).c_str());

			zip_entry_write(zip, buf.c_str(), buf.size());

			zip_entry_close(zip);
		};

		auto addFileToZip = [&](std::wstring localPath, std::wstring zipPath)
		{
#ifdef WIN32
			int fd = _wsopen(
				localPath.c_str(),
				O_RDONLY | O_BINARY,
				SH_DENYNO,
				0 /*_S_IREAD | _S_IWRITE*/);
#else
			int fd = ::open(wstring_to_utf8(localPath).c_str(),
					 O_RDONLY);
#endif
			if (-1 != fd) {
				size_t BUF_LEN = 32768;

				BYTE* buf = new BYTE[BUF_LEN];

				zip_entry_open(zip, wstring_to_utf8(zipPath).c_str());

				int read = ::read(fd, buf, BUF_LEN);
				while (read > 0) {
					if (0 != zip_entry_write(zip, buf, read)) {
						break;
					}

					read = ::read(fd, buf, BUF_LEN);
				}

				zip_entry_close(zip);

				delete[] buf;

				::close(fd);
			}
			else {
				blog(LOG_ERROR,
					"obs-browser: report issue: failed opening file for reading: %s",
					wstring_to_utf8(zipPath).c_str());
			}
		};

		auto addWindowCaptureToZip = [&](std::wstring zipPath)
		{
#ifdef WIN32
			QMainWindow *mainWindow =
				StreamElementsGlobalStateManager::GetInstance()
					->mainWindow();

			QScreen *screen = QGuiApplication::primaryScreen();
			if (const QWindow *window =
				    mainWindow->windowHandle()) {
				screen = window->screen();
			}

			QPixmap pixmap =
				screen->grabWindow(mainWindow->winId());

			// This won't grab CEF windows' content on Win32
			// QPixmap pixmap = mainWindow->grab();
#else
			QDesktopWidget desktop;
			QScreen *screen = QGuiApplication::screens().at(
				desktop.screenNumber(this));
			QRect screenRect = screen->geometry();
			QPixmap pixmap = screen->grabWindow(0, screenRect.x(),
				screenRect.y(), screenRect.width(), screenRect.height());
#endif

			QByteArray bytes;
			QBuffer buffer(&bytes);
			buffer.open(QIODevice::WriteOnly);
			pixmap.save(&buffer, "BMP");

			zip_entry_open(zip, wstring_to_utf8(zipPath).c_str());

			zip_entry_write(zip, buffer.data().constData(),
					buffer.size());

			zip_entry_close(zip);

			return true;
		};

		std::string package_manifest = "generator=report_issue\nversion=4\n";
		addBufferToZip((BYTE*)package_manifest.c_str(), package_manifest.size(), L"manifest.ini");

		// Add user-provided description
		addBufferToZip((BYTE*)descriptionText.c_str(), descriptionText.size(), L"description.txt");

		// Add window capture
		addWindowCaptureToZip(L"obs-main-window.bmp");

		std::map<std::wstring, std::wstring> local_to_zip_files_map;

		if (collect_all) {
			std::vector<std::wstring> blacklist = {
                L"plugin_config/obs-streamelements/obs-streamelements-update.exe",
                L"plugin_config/obs-streamelements/obs-streamelements-update.pkg",
                L"plugin_config/obs-streamelements/obs-streamelements-update.dmg",
				L"plugin_config/obs-browser/cache/",
				L"plugin_config/obs-browser/blob_storage/",
				L"plugin_config/obs-browser/code cache/",
				L"plugin_config/obs-browser/gpucache/",
				L"plugin_config/obs-browser/visited links/",
				L"plugin_config/obs-browser/transportsecurity/",
				L"plugin_config/obs-browser/videodecodestats/",
				L"plugin_config/obs-browser/session storage/",
				L"plugin_config/obs-browser/service worker/",
				L"plugin_config/obs-browser/pepper data/",
				L"plugin_config/obs-browser/indexeddb/",
				L"plugin_config/obs-browser/file system/",
				L"plugin_config/obs-browser/databases/",
				L"plugin_config/obs-browser/obs-browser-streamelements.ini.bak",
				L"plugin_config/obs-browser/cef.",
				L"plugin_config/obs-browser/obs_profile_cookies/",
				L"updates/",
				L"profiler_data/",
				L"obslive_restored_files/",
				L"plugin_config/obs-browser/streamelements_restored_files/",
				L"crashes/"
			};

			// Collect all files
			for (auto &i : std::filesystem::
				     recursive_directory_iterator(
					     programDataPathBuf)) {
				if (!std::filesystem::is_directory(
					    i.path())) {
					std::wstring local_path = i.path().wstring();
					std::wstring zip_path = local_path.substr(obsDataPath.size() + 1);

					std::wstring zip_path_lcase = zip_path;
					std::transform(zip_path_lcase.begin(), zip_path_lcase.end(), zip_path_lcase.begin(), ::towlower);
					std::transform(zip_path_lcase.begin(), zip_path_lcase.end(), zip_path_lcase.begin(), [](wchar_t ch) {
						return ch == L'\\' ? L'/' : ch;
					});

					bool accept = true;
					for (auto item : blacklist) {
						if (zip_path_lcase.size() >= item.size()) {
							if (zip_path_lcase.substr(0, item.size()) == item) {
								accept = false;

								break;
							}
						}
					}

					if (accept) {
						local_to_zip_files_map[local_path] = L"obs-studio\\" + zip_path;
					}
				}
			}

			std::string lastCrashLog =
				get_newest_file("obs-studio/crashes");

			if (lastCrashLog.size()) {
				local_to_zip_files_map[utf8_to_wstring(lastCrashLog)] =
					std::wstring(L"obs-studio\\crashes\\crash.log");
			}
		}
		else {
			// Collect only own files
			local_to_zip_files_map[obsDataPath + L"\\plugin_config\\obs-streamelements\\obs-streamelements.ini"] = L"obs-studio\\plugin_config\\obs-streamelements\\obs-streamelements.ini";
			local_to_zip_files_map[obsDataPath + L"\\plugin_config\\obs-browser\\obs-browser-streamelements.ini"] = L"obs-studio\\plugin_config\\obs-browser\\obs-browser-streamelements.ini";
			local_to_zip_files_map[obsDataPath + L"\\global.ini"] = L"obs-studio\\global.ini";
		}
        
#ifdef __APPLE__
        // Add apple crash logs

        {
            const time_t MAX_SECONDS_OLD = 60 * 60 * 24 * 3;

            struct timespec now;
            clock_gettime(CLOCK_REALTIME, &now);

            struct passwd* pw = getpwuid(getuid());

            std::string logsPath = pw->pw_dir;
            logsPath += "/Library/Logs/DiagnosticReports";

            DIR* dir = opendir(logsPath.c_str());

            if (dir) {
                while (struct dirent* ent = readdir(dir)) {
                    if (ent->d_type == DT_REG && ent->d_name[0] != '.') {
                        std::string fileName = ent->d_name;

                        std::smatch match;

                        if (!std::regex_search(fileName, match,
                                    std::regex("^(.+?)\\.(crash|diag)$")))
                            continue;

                        std::string filePath = logsPath + "/" + fileName;

                        struct stat entstat;

                        if (lstat(filePath.c_str(), &entstat) != 0) continue;

                        time_t delta = now.tv_sec - entstat.st_mtimespec.tv_sec;

                        if (delta < MAX_SECONDS_OLD) {
                            std::string zipFilePath = "crashes/" + fileName;
                            
                            local_to_zip_files_map[utf8_to_wstring(filePath)] = utf8_to_wstring(zipFilePath);
                        }
                    }
                }

                closedir(dir);
            }
        }
#endif

		dialog.setMessage(obs_module_text("StreamElements.ReportIssue.Progress.Message.CollectingFiles"));

		size_t count = 0;
		size_t total = local_to_zip_files_map.size();

		for (auto item : local_to_zip_files_map) {
			if (dialog.cancelled()) {
				break;
			}

			++count;

			addFileToZip(item.first, item.second);

			dialog.setProgress(0, (int)total, (int)count);
		}

		double cpu_benchmark = 0;
		const uint64_t CPU_BENCH_TOTAL = 10000000;
		uint64_t cpu_bench_delta = 0;

		if (!dialog.cancelled()) {
			dialog.setMessage(obs_module_text(
				"StreamElements.ReportIssue.Progress.Message.CollectingCpuBenchmark"));
			qApp->sendPostedEvents();

			cpu_benchmark = GetCpuCoreBenchmark(
				CPU_BENCH_TOTAL, cpu_bench_delta);
		}

		if (!dialog.cancelled()) {
			dialog.setMessage(obs_module_text(
				"StreamElements.ReportIssue.Progress.Message.CollectingSysInfo"));
			qApp->sendPostedEvents();

			{
				CefRefPtr<CefValue> basicInfo =
					CefValue::Create();
				CefRefPtr<CefDictionaryValue> d =
					CefDictionaryValue::Create();
				basicInfo->SetDictionary(d);

				std::string bench;
				bench += (cpu_benchmark);
				d->SetString("obsVersion",
					     obs_get_version_string());
				d->SetString("cefVersion",
					     GetCefVersionString());
				d->SetString("cefApiHash",
					     GetCefPlatformApiHash());
#ifdef WIN32
				d->SetString("platform", "windows");
#elif defined(__APPLE__)
				d->SetString("platform", "macos");
#elif defined(__linux__)
				d->SetString("platform", "linux");
#endif

                if (sizeof(void*) == 8) {
                    d->SetString("platformArch", "64bit");
                } else {
                    d->SetString("platformArch", "32bit");
                }

                d->SetString(
					"streamelementsPluginVersion",
					GetStreamElementsPluginVersionString());
				d->SetDouble("cpuCoreBenchmarkScore",
					     (double)cpu_benchmark);
				d->SetDouble("cpuCoreBenchmarkOpsCount",
					     (double)CPU_BENCH_TOTAL);
				d->SetDouble("cpuCoreBenchmarkNanoseconds",
					     (double)cpu_bench_delta);

                d->SetString("machineUniqueId",
					     GetComputerSystemUniqueId());

				addCefValueToZip(basicInfo,
						 L"system\\basic.json");
			}

			{
				CefRefPtr<CefValue> sysHardwareInfo =
					CefValue::Create();

				SerializeSystemHardwareProperties(
					sysHardwareInfo);

				addCefValueToZip(sysHardwareInfo,
						 L"system\\hardware.json");
			}

			{
				CefRefPtr<CefValue> sysMemoryInfo =
					CefValue::Create();

				SerializeSystemMemoryUsage(sysMemoryInfo);

				addCefValueToZip(sysMemoryInfo,
						 L"system\\memory.json");
			}

			{
				// Histogram CPU & memory usage (past hour, 1 minute intervals)

				auto cpuUsageHistory =
					StreamElementsGlobalStateManager::GetInstance()
						->GetPerformanceHistoryTracker()
						->getCpuUsageSnapshot();

				auto memoryUsageHistory =
					StreamElementsGlobalStateManager::GetInstance()
						->GetPerformanceHistoryTracker()
						->getMemoryUsageSnapshot();

				char lineBuf[512];

				{
					std::vector<std::string> lines;

					lines.push_back(
						"totalSeconds,busySeconds,idleSeconds");
					for (auto item : cpuUsageHistory) {
						sprintf(lineBuf,
							"%1.2Lf,%1.2Lf,%1.2Lf",
							item.totalSeconds,
							item.busySeconds,
							item.idleSeconds);

						lines.push_back(lineBuf);
					}

					addLinesBufferToZip(
						lines,
						L"system\\usage_history_cpu.csv");
				}

				{
					std::vector<std::string> lines;

					lines.push_back(
						"totalSeconds,memoryUsedPercentage");

					size_t index = 0;
					for (auto item : memoryUsageHistory) {
						if (index <
						    cpuUsageHistory.size()) {
							auto totalSec =
								cpuUsageHistory[index]
									.totalSeconds;

#ifdef WIN32
							sprintf(lineBuf,
								"%1.2Lf,%d",
								totalSec,
								item.dwMemoryLoad // % Used
							);
#else
							sprintf(lineBuf,
								"%1.2Lf,%d",
								totalSec,
								item // % Used
							);
#endif
						} else {
#ifdef WIN32
							sprintf(lineBuf,
								"%1.2Lf,%d",
								0.0,
								item.dwMemoryLoad // % Used
							);
#else
							sprintf(lineBuf,
								"%1.2Lf,%d",
								0.0,
								item // % Used
							);
#endif
						}

						lines.push_back(lineBuf);

						++index;
					}

					addLinesBufferToZip(
						lines,
						L"system\\usage_history_memory.csv");
				}
			}
		}

		zip_close(zip);

		if (!dialog.cancelled()) {
			StreamElementsGlobalStateManager::GetInstance()->GetAnalyticsEventsManager()->trackEvent(
				"Issue Report",
				json11::Json::object{ { "issueDescription", descriptionText.c_str() } }
			);

			QMetaObject::invokeMethod(&dialog, "accept", Qt::QueuedConnection);
		}
	});

	if (dialog.exec() == QDialog::Accepted)
	{
		// HTTP upload

		bool retry = true;

		while (retry) {
			obs_frontend_push_ui_translation(obs_module_get_string);

			StreamElementsNetworkDialog netDialog(
				StreamElementsGlobalStateManager::GetInstance()->mainWindow());

			obs_frontend_pop_ui_translation();

			bool success = netDialog.UploadFile(
				tempBufPath.c_str(),
				StreamElementsConfig::GetInstance()->GetUrlReportIssue().c_str(),
				"package",
				obs_module_text("StreamElements.ReportIssue.Upload.Message"));

			if (success) {
				retry = false;

				QMessageBox::information(
					&dialog,
					obs_module_text("StreamElements.ReportIssue.ThankYou.Title"),
					obs_module_text("StreamElements.ReportIssue.ThankYou.Text"),
					QMessageBox::Ok);
			}
			else {
				retry = QMessageBox::Yes == QMessageBox::question(
					&dialog,
					obs_module_text("StreamElements.ReportIssue.Upload.Retry.Title"),
					obs_module_text("StreamElements.ReportIssue.Upload.Retry.Text"),
					QMessageBox::Yes | QMessageBox::No);
			}
		}

		os_unlink(tempBufPath.c_str());
	}


	if (thread.joinable()) {
		thread.join();
	}

	QDialog::accept();
}
