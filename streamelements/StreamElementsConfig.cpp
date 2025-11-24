#include "StreamElementsConfig.hpp"
#include "Version.hpp"
#include <obs-frontend-api.h>
#include <obs.h>

StreamElementsConfig* StreamElementsConfig::s_instance = nullptr;
bool StreamElementsConfig::s_destroyed = false;
std::shared_mutex StreamElementsConfig::s_mutex;

StreamElementsConfig::StreamElementsConfig()
	: m_config(nullptr), m_obsUserConfig(obs_frontend_get_user_config())
{
}

StreamElementsConfig::~StreamElementsConfig()
{
	SaveConfig();

	if (m_config) {
		config_close(SETRACE_DECREF(m_config));

		m_config = nullptr;
	}

	if (m_obsUserConfig) {
		m_obsUserConfig = nullptr;
	}
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
		SETRACE_ADDREF(m_config);
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

std::string StreamElementsConfig::GetScopedConfigStorageRootPath()
{
	char *configDir = obs_module_config_path("");
	configDir[strlen(configDir) - 1] = 0; // remove last char
	std::string root = configDir;
	bfree(configDir);

	root += "/scoped_config_storage";

#ifdef _WIN32
	char* path = os_get_abs_path_ptr(root.c_str());
	root = path;
	bfree(path);
#endif

	return root;
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

	std::string root = GetScopedConfigStorageRootPath();
	root += "/";
	root += scope;
	root += "/";
	root += container;

#ifdef _WIN32
	char *abs = os_get_abs_path_ptr(root.c_str());
	result = abs;
	bfree(abs);
#else
	result = root;
#endif

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

	os_mkdirs(root.c_str());

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

	os_mkdirs(root.c_str());

	std::string path = root + "/" + filename;

	os_quick_write_utf8_file(path.c_str(), content.c_str(), content.size(),
				 true);

	return true;
}

bool StreamElementsConfig::RemoveScopedFile(std::string scope,
						std::string container,
						std::string filename)
{
	if (!isSecureFilename(filename))
		return false;

	std::string root;
	if (!GetScopedTextFileFolderPath(scope, container, root))
		return false;

	os_mkdirs(root.c_str());

	std::string path = root + "/" + filename;

	if (!os_file_exists(path.c_str()))
		return false;

	return (os_unlink(path.c_str()) == 0);
}

bool StreamElementsConfig::ReadScopedFilesList(std::string scope,
					       std::string container,
					       std::string pattern,
					       std::vector<std::string> &result)
{
	std::string root;
	if (!GetScopedTextFileFolderPath(scope, container, root))
		return false;

	os_mkdirs(root.c_str());

	std::string path = root + "/" + pattern;

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

	return true;
}

void StreamElementsConfig::ReadScopedJsonFile(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	output->SetNull();

	if (input->GetType() != VTYPE_DICTIONARY)
		return;

	auto d = input->GetDictionary();

	if (!d->HasKey("scope") || !d->HasKey("container") ||
	    !d->HasKey("item"))
		return;

	if (d->GetType("scope") != VTYPE_STRING ||
	    d->GetType("container") != VTYPE_STRING ||
	    d->GetType("item") != VTYPE_STRING)
		return;

	std::string scope = d->GetString("scope");
	std::string container = d->GetString("container");
	std::string file = d->GetString("item").ToString() + ".json";

	std::string resultText;
	if (!ReadScopedTextFile(scope, container, file, resultText))
		return;

	auto r = CefDictionaryValue::Create();

	auto result = CefParseJSON(resultText, JSON_PARSER_ALLOW_TRAILING_COMMAS);

	if (!result.get())
		return;

	if (!result->IsValid())
		return;

	r->SetString("scope", scope);
	r->SetString("container", container);
	r->SetString("item",
		     file.substr(0, file.size() - 5)); // remove ".json" suffix
	r->SetValue("content", result);

	output->SetDictionary(r);
}

void StreamElementsConfig::WriteScopedJsonFile(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	output->SetNull();

	if (input->GetType() != VTYPE_DICTIONARY)
		return;

	auto d = input->GetDictionary();

	if (!d->HasKey("scope") || !d->HasKey("container") ||
	    !d->HasKey("item") || !d->HasKey("content"))
		return;

	if (d->GetType("scope") != VTYPE_STRING ||
	    d->GetType("container") != VTYPE_STRING ||
	    d->GetType("item") != VTYPE_STRING)
		return;

	std::string scope = d->GetString("scope");
	std::string container = d->GetString("container");
	std::string file = d->GetString("item").ToString() + ".json";
	std::string content = CefWriteJSON(d->GetValue("content"), JSON_WRITER_PRETTY_PRINT).ToString();

	if (!WriteScopedTextFile(scope, container, file, content))
		return;

	auto r = CefDictionaryValue::Create();

	r->SetString("scope", scope);
	r->SetString("container", container);
	r->SetString("item",
		     file.substr(0, file.size() - 5)); // remove ".json" suffix
	r->SetValue("content", d->GetValue("content"));

	output->SetDictionary(r);
}

void StreamElementsConfig::ReadScopedJsonFilesList(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	output->SetNull();

	if (input->GetType() != VTYPE_DICTIONARY)
		return;

	auto d = input->GetDictionary();

	if (!d->HasKey("scope") || !d->HasKey("container"))
		return;

	if (d->GetType("scope") != VTYPE_STRING ||
	    d->GetType("container") != VTYPE_STRING)
		return;

	std::string scope = d->GetString("scope");
	std::string container = d->GetString("container");

	std::vector<std::string> result;
	if (!ReadScopedFilesList(scope, container, "*.json", result))
		return;

	auto r = CefListValue::Create();

	for (auto path : result) {
		auto f = CefDictionaryValue::Create();

		std::string file =
			path.substr(path.find_last_of("/\\") + 1);

		f->SetString("item", file.substr(0, file.size() - 5)); // remove ".json" suffix
		f->SetString("scope", scope);
		f->SetString("container", container);
		f->SetInt("contentLength", os_get_file_size(path.c_str()));

		r->SetDictionary(r->GetSize(), f);
	}

	output->SetList(r);
}

void StreamElementsConfig::RemoveScopedJsonFile(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	output->SetNull();

	if (input->GetType() != VTYPE_DICTIONARY)
		return;

	auto d = input->GetDictionary();

	if (!d->HasKey("scope") || !d->HasKey("container") ||
	    !d->HasKey("item"))
		return;

	if (d->GetType("scope") != VTYPE_STRING ||
	    d->GetType("container") != VTYPE_STRING ||
	    d->GetType("item") != VTYPE_STRING)
		return;

	std::string scope = d->GetString("scope");
	std::string container = d->GetString("container");
	std::string file = d->GetString("item").ToString() + ".json";

	if (!RemoveScopedFile(scope, container, file))
		return;

	auto r = CefDictionaryValue::Create();

	r->SetString("scope", scope);
	r->SetString("container", container);
	r->SetString("item",
		     file.substr(0, file.size() - 5)); // remove ".json" suffix

	output->SetDictionary(r);
}
