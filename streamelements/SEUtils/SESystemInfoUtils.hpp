#pragma once

//
// What this machine is: cpu and memory counters, the hardware inventory, the
// CEF and plug-in version strings, and the per-machine identifier.
//
// Several of these are split across platforms -- the Windows bodies are here
// and the macOS ones are in SEUtils.mm, which is also true of
// CreateGloballyUniqueIdString and CreateCryptoSecureRandomNumberString,
// whose declarations live with the string and crypto domains but whose
// bodies belong to this one.
//

#include <cef-headers.hpp>

#include <string>

void SerializeSystemTimes(CefRefPtr<CefValue> &output);
void SerializeSystemMemoryUsage(CefRefPtr<CefValue> &output);
void SerializeSystemHardwareProperties(CefRefPtr<CefValue> &output);

std::string GetCefVersionString();
std::string GetCefPlatformApiHash();
std::string GetCefUniversalApiHash();
std::string GetSEPluginVersionString();
std::string GetSEApiVersionString();

std::string GetComputerSystemUniqueId();

double GetObsGlobalFramesPerSecond();
