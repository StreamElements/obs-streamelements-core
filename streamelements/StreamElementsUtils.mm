#include "StreamElementsUtils.hpp"
#include "cef-headers.hpp"

#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <AppKit/AppKit.h>

#include <objc/objc.h>
#include <Carbon/Carbon.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/hid/IOHIDManager.h>
#include <SystemConfiguration/SystemConfiguration.h>

#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include <string>
#include <algorithm>

#if CHROME_VERSION_BUILD >= 3729
#include <include/cef_api_hash.h>
#endif

static std::string GetUserName()
{
    char username[_POSIX_LOGIN_NAME_MAX];
    getlogin_r(username, _POSIX_LOGIN_NAME_MAX);
    username[_POSIX_LOGIN_NAME_MAX - 1] = 0;
    
    return username;
}

static bool is_in_bundle()
{
    NSRunningApplication *app = [NSRunningApplication currentApplication];
    return [app bundleIdentifier] != nil;
}

static std::string str_sysctlbyname(const char* name)
{
    std::string result = "";
    
    size_t size;

    int ret = sysctlbyname(name, NULL, &size, NULL, 0);
    if (ret == 0) {
        char* buffer = new char[size];
        
        ret = sysctlbyname(name, buffer, &size, NULL, 0);
        
        if (ret == 0) {
            result = buffer;
        }
        
        delete[] buffer;
    }
    
    return result;
}

static int64_t int64_sysctlbyname(const char* name)
{
    int64_t result = -1;
    size_t size = sizeof(result);
    
    int ret = sysctlbyname(name, &result, &size, NULL, 0);
    if (ret != 0) {
        result = -1;
    }
    
    return result;
}

std::string GetComputerSystemUniqueId()
{
    // Get MacOS serial number
    // https://stackoverflow.com/questions/5868567/unique-identifier-of-a-mac
    // -framework IOKit -framework Foundation
    
    io_service_t platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOPlatformExpertDevice"));

    if (platformExpert) {
        CFStringRef cfsSerial = (CFStringRef)IORegistryEntryCreateCFProperty(platformExpert, CFSTR(kIOPlatformSerialNumberKey), kCFAllocatorDefault, 0);

        IOObjectRelease(platformExpert);

        if (cfsSerial) {
            NSString* nsSerial = [NSString stringWithString:(NSString*)cfsSerial];

            CFRelease(cfsSerial);

            if (nsSerial) {
                std::string username = GetUserName();
                std::string hash = CreateSHA256Digest(username);
                std::transform(hash.begin(), hash.end(), hash.begin(), ::toupper);

                std::string result = "ALSU/"; // AppLe Serial & Username-hash
                result += [nsSerial UTF8String];
                result += "-";
                result += hash;

                return result;
            } else {
                return "ERR_NO_NS_SERIAL";
            }
        } else {
            return "ERR_NO_CFS_SERIAL";
        }
    } else {
        return "ERR_NO_PLATFORM_EXPERT";
    }
}

std::string CreateCryptoSecureRandomNumberString()
{
    srandomdev();
    
    char buf[64];
    
    snprintf(buf, sizeof(buf), "%lX%lX%lX%lX", random(), random(), random(), random());

    return buf;
}

void SerializeSystemHardwareProperties(CefRefPtr<CefValue> &output)
{
    output->SetNull();
    
    CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
    output->SetDictionary(d);

    d->SetString("platform", "macos");
    
    char bitness[16];
    sprintf(bitness, "%dbit", sizeof(void*) * 8);
    
    d->SetString("cpuArch", bitness);
    d->SetInt("cpuCount", os_get_physical_cores());
    d->SetInt("cpuLevel", int64_sysctlbyname("machdep.cpu.stepping"));
    d->SetInt("logicalCpuCount", os_get_logical_cores());
    
    // cpuHardware:
    {
        CefRefPtr<CefListValue> cpuList = CefListValue::Create();

        CefRefPtr<CefDictionaryValue> p =
            CefDictionaryValue::Create();

        // TODO: TBD: Handle multiple CPUs
        
        p->SetString("name", str_sysctlbyname("machdep.cpu.brand_string"));
        p->SetString("vendor", str_sysctlbyname("machdep.cpu.vendor"));
        p->SetInt("speedMHz", int64_sysctlbyname("hw.cpufrequency") / 1000000);
        p->SetString("identifier", str_sysctlbyname("machdep.cpu.brand_string"));

        cpuList->SetDictionary(cpuList->GetSize(), p);
        
        d->SetList("cpuHardware", cpuList);
    }

    // BIOS:
    {
        // Not implemented
        d->SetDictionary("BIOS", CefDictionaryValue::Create());
    }

    // OS:
    {
        Class NSProcessInfo = objc_getClass("NSProcessInfo");
        typedef id (*func)(id, SEL);
        func processInfo = (func)objc_msgSend;

        id pi = processInfo((id)NSProcessInfo, sel_registerName("processInfo"));
        SEL UTF8StringSel = sel_registerName("UTF8String");

        // OS
        d->SetString("os", "Mac OS X");

        // OS Name
        {
            typedef int (*os_func)(id, SEL);
            os_func operatingSystem = (os_func)objc_msgSend;
            unsigned long os_id = (unsigned long)operatingSystem(
                pi, sel_registerName("operatingSystem"));

            typedef id (*os_name_func)(id, SEL);
            os_name_func operatingSystemName = (os_name_func)objc_msgSend;
            id os = operatingSystemName(pi,
                            sel_registerName("operatingSystemName"));
            typedef const char *(*utf8_func)(id, SEL);
            utf8_func UTF8String = (utf8_func)objc_msgSend;
            const char *name = UTF8String(os, UTF8StringSel);

            if (os_id == 5 /*NSMACHOperatingSystem*/) {
                d->SetString("osName", name);
            } else {
                d->SetString("osName", name ? name : "Unknown");
            }
        }
        
        // OS Version
        {
            typedef id (*version_func)(id, SEL);
            version_func operatingSystemVersionString = (version_func)objc_msgSend;
            id vs = operatingSystemVersionString(
                pi, sel_registerName("operatingSystemVersionString"));
            typedef const char *(*utf8_func)(id, SEL);
            utf8_func UTF8String = (utf8_func)objc_msgSend;
            const char *version = UTF8String(vs, UTF8StringSel);

            d->SetString("osVersion", version ? version : "Unknown");
        }
        
        // Kernel version
        d->SetString("osKernelVersion", str_sysctlbyname("kern.osrelease"));
    }
}

//
// Set global CURL options, including system proxy settings
//
// http://mirror.informatimago.com/next/developer.apple.com/qa/qa2001/qa1234.html
//
void SetGlobalCURLOptions(CURL *curl, const char *url)
{
    std::string proxy =
        GetCommandLineOptionValue("streamelements-http-proxy");

    if (!proxy.size()) {
        char host[512];

        Boolean             result;
        CFDictionaryRef     proxyDict;
        CFNumberRef         enableNum;
        int                 enable;
        CFStringRef         hostStr;
        CFNumberRef         portNum;
        int                 portInt;

        // Get the dictionary.
        
        proxyDict = SCDynamicStoreCopyProxies(NULL);
        result = (proxyDict != NULL);

        // Get the enable flag.  This isn't a CFBoolean, but a CFNumber.
        
        if (result) {
            enableNum = (CFNumberRef) CFDictionaryGetValue(proxyDict,
                    kSCPropNetProxiesHTTPSEnable);

            result = (enableNum != NULL)
                    && (CFGetTypeID(enableNum) == CFNumberGetTypeID());
        }
        if (result) {
            result = CFNumberGetValue(enableNum, kCFNumberIntType,
                        &enable) && (enable != 0);
        }
        
        // Get the proxy host.  DNS names must be in ASCII.  If you
        // put a non-ASCII character  in the "Secure Web Proxy"
        // field in the Network preferences panel, the CFStringGetCString
        // function will fail and this function will return false.
        
        if (result) {
            hostStr = (CFStringRef) CFDictionaryGetValue(proxyDict,
                        kSCPropNetProxiesHTTPSProxy);

            result = (hostStr != NULL)
                && (CFGetTypeID(hostStr) == CFStringGetTypeID());
        }
        if (result) {
            result = CFStringGetCString(hostStr, host,
                (CFIndex) sizeof(host), kCFStringEncodingASCII);
        }
        
        // Get the proxy port.
        
        if (result) {
            portNum = (CFNumberRef) CFDictionaryGetValue(proxyDict,
                    kSCPropNetProxiesHTTPSPort);

            result = (portNum != NULL)
                && (CFGetTypeID(portNum) == CFNumberGetTypeID());
        }
        if (result) {
            result = CFNumberGetValue(portNum, kCFNumberIntType, &portInt);
        }

        // Clean up.
        
        if (proxyDict != NULL) {
            CFRelease(proxyDict);
        }
        
        if (result) {
            char buf[520];
            snprintf(buf, sizeof(buf), "%s:%d", host, portInt);
            proxy = buf;
        }
    }
    
    if (proxy.size()) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
    }
}
