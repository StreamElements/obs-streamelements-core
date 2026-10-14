#pragma once

//
// Signed URLs for assets served out of the plug-in's own local filesystem
// HTTP server, so that a page can reference a local file without the path
// being forgeable.
//

#include <string>

std::string CreateSessionSignedAbsolutePathURL(std::wstring path);
bool VerifySessionSignedAbsolutePathURL(std::string url, std::string &path);
