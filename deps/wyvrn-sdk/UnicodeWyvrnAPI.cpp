#include "UnicodeWyvrnAPI.h"
#include "WyvrnErrors.h"
#include "WyvrnLogger.h"
#include "VerifyLibrarySignature.h"
#include <iostream>
#include <tchar.h>


//#define RAZER_CHROMATIC_DEBUGGING true
#if defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE
#define RAZER_CHROMATIC_DLL	L"RzChromatic64.dll"
#else
#ifdef _WIN64
#define RAZER_CHROMATIC_DLL	L"RzChromatic64.dll"
#else
#define RAZER_CHROMATIC_DLL	L"RzChromatic.dll"
#endif


#endif


using namespace WyvrnSDK;
using namespace WyvrnSDK::Implementation;
using namespace std;

HMODULE UnicodeWyvrnAPI::_sLibrary = nullptr;
bool UnicodeWyvrnAPI::_sInvalidSignature = false;
bool UnicodeWyvrnAPI::_sIsInitializedAPI = false;

#define WYVRNSDK_DECLARE_METHOD_IMPL(Signature, FieldName) Signature UnicodeWyvrnAPI::FieldName = nullptr;

#pragma region API declare assignments

WYVRNSDK_DECLARE_METHOD_IMPL(PLUGIN_CORE_INIT_SDK, CoreInitSDK);
WYVRNSDK_DECLARE_METHOD_IMPL(PLUGIN_CORE_SET_EVENT_NAME, CoreSetEventName);
WYVRNSDK_DECLARE_METHOD_IMPL(PLUGIN_CORE_UNINIT, CoreUnInit);
#pragma endregion

#define WYVRNSDK_VALIDATE_METHOD(Signature, FieldName) FieldName = (Signature) GetProcAddress(library, "Plugin" #FieldName); \
if (FieldName == nullptr) \
{ \
	cerr << "Failed to find method: " << ("Plugin" #FieldName) << endl; \
    return -1; \
}

int UnicodeWyvrnAPI::InitAPI()
{
	// abort load if an invalid signature was detected
	if (_sInvalidSignature)
	{
		return RZRESULT_DLL_INVALID_SIGNATURE;
	}

	if (_sIsInitializedAPI)
	{
		return 0;
	}

#ifdef RAZER_CHROMATIC_DEBUGGING

	// looking for the DLL in the current module path

	wchar_t filename[MAX_PATH]; //this is a char buffer
	GetModuleFileNameW(NULL, filename, sizeof(filename));

	std::wstring path;
	const size_t last_slash_idx = std::wstring(filename).rfind('\\');
	if (std::string::npos != last_slash_idx)
	{
		path = std::wstring(filename).substr(0, last_slash_idx);
	}

	path += L"\\";
	path += RAZER_CHROMATIC_DLL;

#else

	wstring path = RAZER_CHROMATIC_DLL;

	// 2. The system directory.Use the GetSystemDirectory function to get the path of this directory.

	wchar_t pathTemp[MAX_PATH];
	if (GetSystemDirectoryW(pathTemp, sizeof(pathTemp)))
	{
		path = pathTemp;

		if (path.length() > 0 && path.compare(path.length() - 1, 1, L"\\") != 0) //not endsWith slash
		{
			path += L"\\";
		}
		path += RAZER_CHROMATIC_DLL;
	}

#endif

	// check the library file version
	if (!VerifyLibrarySignature::IsFileVersionSameOrNewer(path.c_str(), 2, 0, 2, 0))
	{
		WyvrnLogger::fprintf(stderr, "Detected old version of Chromatic Library!\r\n");
		return RZRESULT_DLL_NOT_FOUND;
	}

#ifndef NO_CHECK_WYVRNSDK_LIBRARY_SIGNATURE
	// verify the library has a valid signature
	_sInvalidSignature = !VerifyLibrarySignature::VerifyModule(path);
#endif

	if (_sInvalidSignature)
	{
		WyvrnLogger::fprintf(stderr, "Chromatic Library has an invalid signature!\r\n");
		return RZRESULT_DLL_INVALID_SIGNATURE;
	}

	HMODULE library = LoadLibraryW(path.c_str());
	if (library == NULL)
	{
		WyvrnLogger::fprintf(stderr, "Failed to load Chromatic Library!\r\n");
		return RZRESULT_DLL_NOT_FOUND;
	}

	_sLibrary = library;

	#pragma region API validation
	WYVRNSDK_VALIDATE_METHOD(PLUGIN_CORE_INIT_SDK, CoreInitSDK);
	WYVRNSDK_VALIDATE_METHOD(PLUGIN_CORE_SET_EVENT_NAME, CoreSetEventName);
	WYVRNSDK_VALIDATE_METHOD(PLUGIN_CORE_UNINIT, CoreUnInit);
	#pragma endregion

	//WyvrnLogger::printf(stdout, "Validated all DLL methods [success]\r\n");
	_sIsInitializedAPI = true;
	return 0;
}

bool UnicodeWyvrnAPI::GetIsInitializedAPI()
{
	return _sIsInitializedAPI;
}

#undef WYVRNSDK_DECLARE_METHOD_CLEAR
#define WYVRNSDK_DECLARE_METHOD_CLEAR(FieldName) UnicodeWyvrnAPI::FieldName = nullptr;

int UnicodeWyvrnAPI::UninitAPI()
{
	if (nullptr != _sLibrary)
	{
		FreeLibrary(_sLibrary);
		_sLibrary = nullptr;
	}

#pragma region Free API Methods

	WYVRNSDK_DECLARE_METHOD_CLEAR(CoreInitSDK);
	WYVRNSDK_DECLARE_METHOD_CLEAR(CoreSetEventName);
	WYVRNSDK_DECLARE_METHOD_CLEAR(CoreUnInit);

#pragma endregion

	_sIsInitializedAPI = false;
	
	return 0;
}
