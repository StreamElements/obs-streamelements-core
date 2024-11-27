#pragma once

#include <util/config-file.h>

#include <string>

#include "StreamElementsConfig.hpp"

class ConfigAccessibilityColor {
	std::string m_name;
	QColor m_defaultValue;

public:
	ConfigAccessibilityColor(std::string name, QColor defaultValue)
		: m_name(name), m_defaultValue(defaultValue)
	{
	}
	ConfigAccessibilityColor(ConfigAccessibilityColor &other) = delete;
	~ConfigAccessibilityColor() {}

	QColor get() {
		auto config = StreamElementsConfig::GetInstance();

		if (!config)
			return m_defaultValue;

		auto fe_config = config->GetObsGlobalConfig();

		if (!fe_config)
			return m_defaultValue;

		if (config_get_bool(fe_config,
				    "Accessibility",
				    "OverrideColors")) {
			return color_from_int(config_get_int(
				fe_config,
				"Accessibility", m_name.c_str()));
		} else {
			return m_defaultValue;
		}
	}

private:
	inline QColor color_from_int(long long val)
	{
		return QColor(val & 0xff, (val >> 8) & 0xff, (val >> 16) & 0xff,
			      (val >> 24) & 0xff);
	}
};

class FileTexture {
private:
	std::string m_path;
	std::mutex m_mutex;
	gs_texture_t *m_texture = nullptr;

public:
	FileTexture(std::string path): m_path(path) {}

	~FileTexture()
	{
		Destroy();
	}

	void Destroy()
	{
		if (m_texture) {
			gs_texture_destroy(m_texture);

			m_texture = nullptr;
		}
	}

	gs_texture_t *get()
	{
		if (m_texture)
			return m_texture;

		std::lock_guard<decltype(m_mutex)> lock(m_mutex);

		if (m_texture)
			return m_texture;

		auto textureFilePath = os_get_abs_path_ptr(m_path.c_str());
		m_texture = gs_texture_create_from_file(textureFilePath);
		bfree(textureFilePath);

		return m_texture;
	}
};
