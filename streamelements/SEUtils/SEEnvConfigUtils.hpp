#pragma once

//
// The product environment configuration: values under the StreamElements key
// in the Windows registry plus the matching OBS config entries, read and
// written as one thing.
//

#include <string>
#include <vector>

struct streamelements_env_update_request {
public:
	std::string product;
	std::string key;
	std::string value;
};

typedef std::vector<streamelements_env_update_request>
	streamelements_env_update_requests;

std::string ReadProductEnvironmentConfigurationString(const char *key);
bool WriteProductEnvironmentConfigurationString(const char *key,
						const char *value);
bool WriteProductEnvironmentConfigurationStrings(
	streamelements_env_update_requests requests);
bool WriteEnvironmentConfigStrings(streamelements_env_update_requests requests);
bool WriteEnvironmentConfigString(const char *regValueName,
				  const char *regValue,
				  const char *productName);

//
// Declared rather than left file-static, because GetComputerSystemUniqueId
// reads a value through it and that function has not moved yet -- its macOS
// half lives in SEUtils.mm.
//
std::string ReadEnvironmentConfigString(const char *regValueName,
					const char *productName);
