#pragma once

#include "WyvrnSDKTypes.h"

namespace WyvrnSDK
{
	/* Setup log mechanism */
	typedef void (*DebugLogPtr)(const wchar_t*);
	void LogDebug(const wchar_t *text, ...);
	void LogError(const wchar_t *text, ...);
	/* End of setup log mechanism */

	class WyvrnAPI
	{
	public:

                
#pragma region API declare prototypes
		/*
			Direct access to low level API.
		*/
		static RZRESULT CoreInitSDK(WyvrnSDK::APPINFOTYPE* AppInfo);
		/*
			Direct access to low level API.
		*/
		static RZRESULT CoreSetEventName(const wchar_t* name);
		/*
			Direct access to low level API.
		*/
		static RZRESULT CoreUnInit();
#pragma endregion


		static int InitAPI();
		static int UninitAPI();
		static bool GetIsInitializedAPI();
		static bool _sIsInitializedAPI;
		static bool _sInitialized;
	};
}
