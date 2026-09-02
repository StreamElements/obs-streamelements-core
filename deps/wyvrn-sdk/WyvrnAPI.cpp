#include "WyvrnAPI.h"
#include "UnicodeWyvrnAPI.h"
#include "WyvrnErrors.h"
#include "WyvrnLogger.h"
#include "VerifyLibrarySignature.h"
#include <iostream>
#include <tchar.h>


using namespace std;
using namespace WyvrnSDK::Implementation;

namespace WyvrnSDK {

	bool WyvrnAPI::_sIsInitializedAPI = false;
	bool WyvrnAPI::_sInitialized = false;

	int WyvrnAPI::InitAPI()
	{
		if (_sIsInitializedAPI)
		{
			return RZRESULT_SUCCESS;
		}

		if (_sInitialized)
		{
			return RZRESULT_SUCCESS;
		}

		RZRESULT result = UnicodeWyvrnAPI::InitAPI();
		if (result == RZRESULT_SUCCESS)
		{
			_sIsInitializedAPI = true;
		}
		return result;
	}

	bool WyvrnAPI::GetIsInitializedAPI()
	{
		if (!_sIsInitializedAPI)
		{
			return false;
		}
		return UnicodeWyvrnAPI::GetIsInitializedAPI();
	}

	int WyvrnAPI::UninitAPI()
	{
		if (!_sIsInitializedAPI)
		{
			return RZRESULT_SUCCESS;
		}
		RZRESULT result = UnicodeWyvrnAPI::UninitAPI();
		if (result == RZRESULT_SUCCESS)
		{
			_sIsInitializedAPI = false;
		}
		return result;
	}

#pragma region API declare prototypes

	/*
		Direct access to low level API.
	*/
	RZRESULT WyvrnAPI::CoreInitSDK(WyvrnSDK::APPINFOTYPE* appInfo)
	{
		if (!_sIsInitializedAPI)
		{
			RZRESULT result = WyvrnAPI::InitAPI();
			if (result != RZRESULT_SUCCESS)
			{
				return result;
			}
			if (!_sIsInitializedAPI)
			{
				return static_cast<RZRESULT>(RZRESULT_FAILED);
			}
		}
		if (_sInitialized)
		{
			return RZRESULT_SUCCESS;
		}
		RZRESULT result = UnicodeWyvrnAPI::CoreInitSDK(appInfo);
		if (result == RZRESULT_SUCCESS)
		{
			_sInitialized = true;
		}
		return result;
	}
	/*
		Direct access to low level API.
	*/
	RZRESULT WyvrnAPI::CoreSetEventName(const wchar_t* name)
	{
		if (!_sIsInitializedAPI)
		{
			return -1;
		}
		if (!_sInitialized)
		{
			return -1;
		}
		return UnicodeWyvrnAPI::CoreSetEventName(name);
	}
	/*
		Direct access to low level API.
	*/
	RZRESULT WyvrnAPI::CoreUnInit()
	{
		if (!_sIsInitializedAPI)
		{
			return RZRESULT_SUCCESS;
		}
		if (!_sInitialized)
		{
			return RZRESULT_SUCCESS;
		}
		RZRESULT result = UnicodeWyvrnAPI::CoreUnInit();
		if (result == RZRESULT_SUCCESS)
		{
			_sInitialized = false;
		}
		return result;

	}
#pragma endregion

}
