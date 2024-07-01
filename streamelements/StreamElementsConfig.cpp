#include "StreamElementsConfig.hpp"
#include "Version.hpp"

StreamElementsConfig* StreamElementsConfig::s_instance = nullptr;

StreamElementsConfig::StreamElementsConfig():
	m_config(nullptr)
{

}

StreamElementsConfig::~StreamElementsConfig()
{
	SaveConfig();
}


config_t* StreamElementsConfig::GetConfig()
{
	if (!m_config) {
		char *configDir = obs_module_config_path("");
		configDir[strlen(configDir) - 1] = 0; // remove last char
		os_mkdirs(configDir);
		bfree(configDir);

		char *configPath = obs_module_config_path(CONFIG_FILE_NAME);
		config_open(
			&m_config, configPath,
			CONFIG_OPEN_ALWAYS);
		bfree(configPath);

		config_set_default_uint(m_config, "Header", "Version", STREAMELEMENTS_PLUGIN_VERSION);
		config_set_default_uint(m_config, "Startup", "Flags", STARTUP_FLAGS_ONBOARDING_MODE);
		config_set_default_string(m_config, "Startup", "State", "");
		config_set_default_bool(m_config, "Startup",
					"ShowBuiltInMenuItems", true);
	}

	return m_config;
}

void StreamElementsConfig::SaveConfig()
{
	if (!m_config) return;

	config_set_uint(m_config, "Header", "Version", STREAMELEMENTS_PLUGIN_VERSION);

	config_save_safe(m_config, "tmp", "bak");
}

std::string StreamElementsConfig::GetHeapAnalyticsAppId()
{
	std::string result = "413792583";

#ifdef WIN32
	const char* REG_KEY_PATH = "SOFTWARE\\StreamElements";
	const char* REG_VALUE_NAME = "HeapAnalyticsAppId";

	DWORD bufLen = 16384;
	char* buffer = new char[bufLen];

	if (ERROR_SUCCESS != RegGetValueA(
		HKEY_LOCAL_MACHINE,
		REG_KEY_PATH,
		REG_VALUE_NAME,
		RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY,
		NULL,
		buffer,
		&bufLen)) {
		if (ERROR_SUCCESS == RegGetValueA(
			HKEY_LOCAL_MACHINE,
			REG_KEY_PATH,
			REG_VALUE_NAME,
			RRF_RT_REG_SZ | RRF_SUBKEY_WOW6432KEY,
			NULL,
			buffer,
			&bufLen)) {
			if (buffer[0]) {
				result = buffer;
			}
		}
	}
	else {
		if (buffer[0]) {
			result = buffer;
		}
	}

	delete[] buffer;
#endif

	return result;
}

static bool isSecureFilename(std::string filename) {
	if (filename == ".")
		return false;

	if (filename == "..")
		return false;

	if (strstr(filename.c_str(), "/"))
		return false;

	if (strstr(filename.c_str(), "\\"))
		return false;

	return true;
}

bool StreamElementsConfig::GetScopedTextFileFolderPath(
	std::string scope,
	std::string container,
	std::string &result)
{
	if (!isSecureFilename(scope))
		return false;

	if (!isSecureFilename(container))
		return false;

	char *configDir = obs_module_config_path("");
	configDir[strlen(configDir) - 1] = 0; // remove last char
	std::string root = configDir;
	bfree(configDir);

	root += "/scoped_config/";
	root += scope;
	root += "/";
	root += container;

	char *abs = os_get_abs_path_ptr(root.c_str());
	result = abs;
	bfree(abs);

	return true;
}

bool StreamElementsConfig::ReadScopedTextFile(std::string scope,
					      std::string container,
					      std::string filename,
					      std::string &result)
{
	if (!isSecureFilename(filename))
		return false;

	std::string root;
	if (!GetScopedTextFileFolderPath(scope, container, root))
		return false;

	if (os_mkdirs(root.c_str()) != 0)
		return false;

	std::string path = root + "/" + filename;

	if (!os_file_exists(path.c_str()))
		return false;

	char *data = os_quick_read_utf8_file(path.c_str());

	if (!data)
		return false;

	result = data;

	bfree(data);

	return true;
}

bool StreamElementsConfig::WriteScopedTextFile(std::string scope,
					       std::string container,
					       std::string filename,
					       std::string content)
{
	if (!isSecureFilename(filename))
		return false;

	std::string root;
	if (!GetScopedTextFileFolderPath(scope, container, root))
		return false;

	if (os_mkdirs(root.c_str()) != 0)
		return false;

	std::string path = root + "/" + filename;

	os_quick_write_utf8_file(path.c_str(), content.c_str(), content.size(),
				 true);

	return true;
}

bool StreamElementsConfig::RemoveScopedTextFile(std::string scope,
						std::string container,
						std::string filename)
{
	if (!isSecureFilename(filename))
		return false;

	std::string root;
	if (!GetScopedTextFileFolderPath(scope, container, root))
		return false;

	if (os_mkdirs(root.c_str()) != 0)
		return false;

	std::string path = root + "/" + filename;

	if (!os_file_exists(path.c_str()))
		return false;

	return (os_unlink(path.c_str()) == 0);
}

bool StreamElementsConfig::ReadScopedTextFilesList(
	std::string scope, std::string container,
	std::vector<std::string> &result)
{
	std::string root;
	if (!GetScopedTextFileFolderPath(scope, container, root))
		return false;

	if (os_mkdirs(root.c_str()) != 0)
		return false;

	std::string path = root + "/*";

	os_glob_t *glob;
	if (os_glob(path.c_str(), 0, &glob) != 0)
		return false;

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		struct os_globent ent = glob->gl_pathv[i];

		if (ent.directory)
			continue;

		QFile file(ent.path);

		result.push_back(file.fileName().toStdString());
	}

	os_globfree(glob);
}
