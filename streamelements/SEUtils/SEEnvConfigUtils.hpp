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

//
// A --key=value override from the process command line, which is where
// the product's configuration is read from before the registry: SEConfig
// reaches for both, one after the other.
//
// Declared here rather than beside its body in SEAppEnvUtils, which has
// to include <QWidget> for the window-stacking helpers. Reading argv is
// not a Qt concern -- QCoreApplication is just where the list happens to
// live -- and SEHttpUtils calls this for the proxy override, so that
// include was the one thing keeping the whole HTTP cluster from
// compiling without Qt. Same arrangement as
// CreateCryptoSecureRandomNumberString: the declaration sits with the
// domain that uses it, the body stays where the platform code is.
//
std::string GetCommandLineOptionValue(const std::string key);
