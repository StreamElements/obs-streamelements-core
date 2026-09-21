#pragma once

#include <obs-module.h>
#include <util/config-file.h>

#include <string>
#include <mutex>
#include <shared_mutex>
#include <set>

#include "StreamElementsUtils.hpp"
#include "StreamElementsMessageBus.hpp"

class StreamElementsConfig
{
private:
	const char* CONFIG_FILE_NAME = "obs-streamelements-core.ini";

public:
	static const uint64_t STARTUP_FLAGS_ONBOARDING_MODE = 0x0000000000000001ULL;
	static const uint64_t STARTUP_FLAGS_SIGNED_IN       = 0x0000000000000002ULL;

private:
	StreamElementsConfig();
	~StreamElementsConfig();

public:
	static StreamElementsConfig* GetInstance() {
		if (s_destroyed)
			return nullptr;

		{
			std::shared_lock<decltype(s_mutex)> lock(s_mutex);

			if (s_instance) {
				return s_instance;
			}
		}

		std::unique_lock<decltype(s_mutex)> lock(s_mutex);

		if (!s_instance) {
			s_instance = new StreamElementsConfig();
		}

		return s_instance;
	}

	static void Destroy()
	{
		std::unique_lock<decltype(s_mutex)> lock(s_mutex);

		if (s_instance) {
			delete s_instance;
			s_instance = nullptr;
		}

		s_destroyed = true;
	}

public:
	config_t* GetConfig();
	void SaveConfig();

	config_t* GetObsUserConfig() { return m_obsUserConfig; }

private:
	//
	// Every read and write below goes through these.
	//
	// GetConfig() can return null -- the profile directory can be
	// unwritable, or the path invalid -- and libobs dereferences a null
	// config_t at config->mutex without checking for it, which crashed OBS
	// on the start-up path (CORE-1900, SELIVE-8A). Passing GetConfig()
	// straight to a libobs config_* call is what made every accessor a
	// crash of its own.
	//
	uint64_t ReadUint(const char *section, const char *name,
			  uint64_t defaultValue = 0)
	{
		auto config = GetConfig();

		return config ? config_get_uint(config, section, name)
			      : defaultValue;
	}

	bool ReadBool(const char *section, const char *name,
		      bool defaultValue = false)
	{
		auto config = GetConfig();

		return config ? config_get_bool(config, section, name)
			      : defaultValue;
	}

	std::string ReadString(const char *section, const char *name,
			       const char *defaultValue = "")
	{
		auto config = GetConfig();

		const char *value =
			config ? config_get_string(config, section, name)
			       : nullptr;

		// config_get_string() returns null for a key that is not set,
		// and constructing a std::string from null is undefined.
		return value ? value : defaultValue;
	}

	void WriteUint(const char *section, const char *name, uint64_t value)
	{
		auto config = GetConfig();

		if (config)
			config_set_uint(config, section, name, value);
	}

	void WriteBool(const char *section, const char *name, bool value)
	{
		auto config = GetConfig();

		if (config)
			config_set_bool(config, section, name, value);
	}

	void WriteString(const char *section, const char *name,
			 const char *value)
	{
		auto config = GetConfig();

		if (config)
			config_set_string(config, section, name, value);
	}

public:
	std::string GetScopedConfigStorageRootPath();

	bool GetScopedTextFileFolderPath(std::string scope,
					 std::string container,
					 std::string &result);

	bool ReadScopedTextFile(std::string scope, std::string container,
				std::string filename, std::string &result);
	bool WriteScopedTextFile(std::string scope, std::string container,
				 std::string filename, std::string content);
	bool RemoveScopedFile(std::string scope, std::string container,
			      std::string filename);

	bool ReadScopedFilesList(std::string scope, std::string container,
				 std::string pattern,
				 std::vector<std::string> &result);

public:
	void ReadScopedJsonFile(CefRefPtr<CefValue> input,
				CefRefPtr<CefValue> &output);
	void WriteScopedJsonFile(CefRefPtr<CefValue> input,
				 CefRefPtr<CefValue> &output);
	void ReadScopedJsonFilesList(CefRefPtr<CefValue> input,
				     CefRefPtr<CefValue> &output);
	void RemoveScopedJsonFile(CefRefPtr<CefValue> input,
				  CefRefPtr<CefValue> &output);

public:
	int64_t GetStreamElementsPluginVersion()
	{
		return (int64_t)ReadUint("Header", "Version");
	}

	int GetStartupFlags()
	{
		return (int)ReadUint("Startup", "Flags",
				     STARTUP_FLAGS_ONBOARDING_MODE);
	}

	void SetStartupFlags(int value)
	{
		WriteUint("Startup", "Flags", value);

		SaveConfig();

		StreamElementsMessageBus::GetInstance()->PublishSystemState();
	}

	std::string GetStartupState() { return ReadString("Startup", "State"); }

	void SetStartupState(std::string value)
	{
		WriteString("Startup", "State", value.c_str());

		SaveConfig();
	}

	std::string GetUrlOnBoarding()
	{
		std::string result = GetCommandLineOptionValue("streamelements-onboarding-url");

		if (!result.size()) {
			result = ReadProductEnvironmentConfigurationString("OnboardingUrl");
		}

		if (!result.size()) {
			// result = "https://obs.streamelements.com/welcome"; // old URL, yoink repo in github
			result = "https://selive.streamelements.com/auth/login"; // obs-multistreaming repo in github
		}

		return result;
	}

	std::string GetUrlReportIssue()
	{
		std::string result = GetCommandLineOptionValue("streamelements-report-issue-url");

		if (!result.size()) {
			result = "https://obs-reports.streamelements.com/api/report-issue";
		}

		return result;
	}

	std::string GetHeapAnalyticsAppId();

	std::string GetAuxMenuItemsConfig()
	{
		return ReadString("Startup", "AuxMenuItems", "[]");
	}

	void SetAuxMenuItemsConfig(std::string value)
	{
		WriteString("Startup", "AuxMenuItems", value.c_str());

		SaveConfig();
	}

	bool GetShowBuiltInMenuItems()
	{
		return ReadBool("Startup", "ShowBuiltInMenuItems", true);
	}

	void SetShowBuiltInMenuItems(bool value)
	{
		WriteBool("Startup", "ShowBuiltInMenuItems", value);

		SaveConfig();
	}

	//
	// Contact details the user supplied on a previous crash report.
	//
	// Kept so the crash-time prompt can prefill them and the user does not
	// retype their details every time, and so that events can carry the user
	// even when a crash never reaches the prompt.
	//
	// These are written from the crash path, where the process is already
	// dying -- deliberately a single small ini write, and only after the
	// user has actually typed something.
	//
	std::string GetCrashReportUserName()
	{
		return ReadString("CrashReporting", "UserName");
	}

	void SetCrashReportUserName(std::string value)
	{
		WriteString("CrashReporting", "UserName", value.c_str());

		SaveConfig();
	}

	std::string GetCrashReportUserEmail()
	{
		return ReadString("CrashReporting", "UserEmail");
	}

	void SetCrashReportUserEmail(std::string value)
	{
		WriteString("CrashReporting", "UserEmail", value.c_str());

		SaveConfig();
	}

	std::string GetCrashReportUserDiscord()
	{
		return ReadString("CrashReporting", "UserDiscord");
	}

	void SetCrashReportUserDiscord(std::string value)
	{
		WriteString("CrashReporting", "UserDiscord", value.c_str());

		SaveConfig();
	}

	std::string GetSceneItemsAuxActionsConfig()
	{
		return ReadString("Startup", "AuxSourcesActions", "[]");
	}

	void SetSceneItemsAuxActionsConfig(std::string value)
	{
		WriteString("Startup", "AuxSourcesActions", value.c_str());

		SaveConfig();
	}

	std::string GetScenesAuxActionsConfig()
	{
		return ReadString("Startup", "AuxScenesActions", "[]");
	}

	void SetScenesAuxActionsConfig(std::string value)
	{
		WriteString("Startup", "AuxScenesActions", value.c_str());

		SaveConfig();
	}

	bool IsOnBoardingMode() {
		return (GetStartupFlags() & STARTUP_FLAGS_ONBOARDING_MODE) != 0;
	}

private:
	config_t* m_config = nullptr;
	// True when m_config is the in-memory fallback, which has no file.
	bool m_configIsMemoryOnly = false;
	config_t *m_obsUserConfig = nullptr;

private:
	static StreamElementsConfig* s_instance;
	static bool s_destroyed;
	static std::shared_mutex s_mutex;
};

