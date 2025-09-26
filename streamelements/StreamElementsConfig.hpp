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
		return config_get_uint(
			StreamElementsConfig::GetInstance()->GetConfig(),
			"Header",
			"Version");
	}

	int GetStartupFlags()
	{
		return (int)config_get_uint(
			StreamElementsConfig::GetInstance()->GetConfig(),
			"Startup",
			"Flags");
	}

	void SetStartupFlags(int value)
	{
		config_set_uint(
			StreamElementsConfig::GetInstance()->GetConfig(),
			"Startup",
			"Flags",
			value);

		SaveConfig();

		StreamElementsMessageBus::GetInstance()->PublishSystemState();
	}

	std::string GetStartupState()
	{
		return config_get_string(
			StreamElementsConfig::GetInstance()->GetConfig(),
			"Startup",
			"State");
	}

	void SetStartupState(std::string value)
	{
		config_set_string(
			StreamElementsConfig::GetInstance()->GetConfig(),
			"Startup",
			"State",
			value.c_str());

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
		const char* value = config_get_string(
			StreamElementsConfig::GetInstance()->GetConfig(),
			"Startup", "AuxMenuItems");

		if (!!value)
			return value;
		else
			return "[]";
	}

	void SetAuxMenuItemsConfig(std::string value)
	{
		config_set_string(
			StreamElementsConfig::GetInstance()->GetConfig(),
			"Startup", "AuxMenuItems", value.c_str());

		SaveConfig();
	}

	bool GetShowBuiltInMenuItems()
	{
		const bool value = config_get_bool(
			StreamElementsConfig::GetInstance()->GetConfig(),
			"Startup", "ShowBuiltInMenuItems");

		return value;
	}

	void SetShowBuiltInMenuItems(bool value)
	{
		config_set_bool(
			StreamElementsConfig::GetInstance()->GetConfig(),
			"Startup", "ShowBuiltInMenuItems", value);

		SaveConfig();
	}

	std::string GetSceneItemsAuxActionsConfig()
	{
		const char *value = config_get_string(
			StreamElementsConfig::GetInstance()->GetConfig(),
			"Startup", "AuxSourcesActions");

		if (!!value)
			return value;
		else
			return "[]";
	}

	void SetSceneItemsAuxActionsConfig(std::string value)
	{
		config_set_string(
			StreamElementsConfig::GetInstance()->GetConfig(),
			"Startup", "AuxSourcesActions", value.c_str());

		SaveConfig();
	}

	std::string GetScenesAuxActionsConfig()
	{
		const char *value = config_get_string(
			StreamElementsConfig::GetInstance()->GetConfig(),
			"Startup", "AuxScenesActions");

		if (!!value)
			return value;
		else
			return "[]";
	}

	void SetScenesAuxActionsConfig(std::string value)
	{
		config_set_string(
			StreamElementsConfig::GetInstance()->GetConfig(),
			"Startup", "AuxScenesActions", value.c_str());

		SaveConfig();
	}

	bool IsOnBoardingMode() {
		return (GetStartupFlags() & STARTUP_FLAGS_ONBOARDING_MODE) != 0;
	}

	void GetSharedVideoCompositionIds(std::set<std::string>& set) {
		set.clear();

		auto value = config_get_string(
			StreamElementsConfig::GetInstance()->GetConfig(),
			"SharedVideoCompositions", "UUIDList");

		if (!value)
			return;

		std::string delimiter = ",";
		std::string s = value;

		size_t pos = 0;
		while ((pos = s.find(delimiter)) != std::string::npos) {
			auto token = s.substr(0, pos);

			set.insert(token);

			s.erase(0, pos + delimiter.length());
		}

		set.insert(s);
	}

	void SetSharedVideoCompositionIds(std::set<std::string>& set) {
		std::string s = "";

		for (auto key : set) {
			if (s.size() > 0)
				s += ",";

			s += key;
		}

		config_set_string(
			StreamElementsConfig::GetInstance()->GetConfig(),
			"SharedVideoCompositions", "UUIDList", s.c_str());

		SaveConfig();
	}

private:
	config_t* m_config = nullptr;
	config_t *m_obsUserConfig = nullptr;

private:
	static StreamElementsConfig* s_instance;
	static bool s_destroyed;
	static std::shared_mutex s_mutex;
};

