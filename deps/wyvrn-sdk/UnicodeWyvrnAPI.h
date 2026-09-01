#pragma once

#include "WyvrnSDKTypes.h"

#pragma region API typedefs
/*
	Direct access to low level API.
*/
typedef RZRESULT	(*PLUGIN_CORE_INIT_SDK)(WyvrnSDK::APPINFOTYPE* AppInfo);
/*
	Direct access to low level API.
*/
typedef RZRESULT	(*PLUGIN_CORE_SET_EVENT_NAME)(const wchar_t* name);
/*
	Direct access to low level API.
*/
typedef RZRESULT	(*PLUGIN_CORE_UNINIT)();

#pragma endregion

#define WYVRNSDK_DECLARE_METHOD(Signature, FieldName) static Signature FieldName;

namespace WyvrnSDK
{
	namespace Implementation
	{
		class UnicodeWyvrnAPI
		{
		private:
			static bool _sIsInitializedAPI;
			static bool _sInvalidSignature;
			static HMODULE _sLibrary;

		public:

#pragma region API declare prototypes
			/*
				Direct access to low level API.
			*/
			WYVRNSDK_DECLARE_METHOD(PLUGIN_CORE_INIT_SDK, CoreInitSDK);
			/*
				Direct access to low level API.
			*/
			WYVRNSDK_DECLARE_METHOD(PLUGIN_CORE_SET_EVENT_NAME, CoreSetEventName);
			/*
				Direct access to low level API.
			*/
			WYVRNSDK_DECLARE_METHOD(PLUGIN_CORE_UNINIT, CoreUnInit);

#pragma endregion

			static int InitAPI();
			static int UninitAPI();
			static bool GetIsInitializedAPI();
		};
	}
}
