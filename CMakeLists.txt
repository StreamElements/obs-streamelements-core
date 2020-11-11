cmake_minimum_required(VERSION 2.8.12)
project(obs-browser)

if (WIN32)
	set(ENABLE_BUGSPLAT TRUE)
	set(ENABLE_ANGELSCRIPT TRUE)
else()
	set(ENABLE_BUGSPLAT FALSE)
	set(ENABLE_ANGELSCRIPT FALSE)
endif(WIN32)

set(CMAKE_PREFIX_PATH "${QTDIR}")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps")

find_package(CEF QUIET)
find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Libcurl REQUIRED)
if (ENABLE_BUGSPLAT)
	find_package(BugSplat REQUIRED)
endif()
if (ENABLE_ANGELSCRIPT)
	find_package(AngelScript REQUIRED)
endif()

include_directories(SYSTEM
	${Qt5Core_INCLUDES}
	${Qt5Widgets_INCLUDES}
	${LIBCURL_INCLUDE_DIRS}
)

if (ENABLE_BUGSPLAT)
	include_directories(SYSTEM
		${BUGSPLAT_INCLUDE_DIR}
	)
endif()

if (ENABLE_ANGELSCRIPT)
	include_directories(SYSTEM
		${ANGELSCRIPT_INCLUDE_DIR}
		${ANGELSCRIPT_ADDON_INCLUDE_DIR}
	)
endif()

#qt5_wrap_ui(NetworkDialog "networkdialog.ui")
qt5_add_resources(obs-browser-streamelements_QRC_SOURCES "streamelements/streamelements.qrc")

if(NOT CEF_FOUND)
	message(STATUS "CEF Not found -- obs-browser plugin disabled.")
	return()
endif()

if (NOT BUGSPLAT_FOUND AND ENABLE_BUGSPLAT)
	message(STATUS "BugSplat Not found -- obs-browser plugin disabled.")
	return()
endif()

if (NOT ANGELSCRIPT_FOUND AND ENABLE_ANGELSCRIPT)
	message(STATUS "AngelScript Not found -- obs-browser plugin disabled.")
	return()
endif()

option(EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED "Enable shared texture support for the browser plugin (Win32)" ON)
option(BROWSER_PANEL_SUPPORT_ENABLED "Enables Qt web browser panel support" ON)

if(NOT APPLE)
	option(USE_QT_LOOP "Runs CEF on the main UI thread alongside Qt instead of in its own thread" OFF)
else()
	set(USE_QT_LOOP TRUE)
endif()

if(APPLE)
	option(BROWSER_DEPLOY "Marks full deployment of browser plugin" OFF)
	if(BROWSER_DEPLOY)
		add_definitions(-DBROWSER_DEPLOY)
	endif()
endif()

if(UNIX AND NOT APPLE)
	find_package(X11 REQUIRED)
	if(NOT X11_FOUND)
		message(STATUS "X11 Not found -- obs-browser plugin disabled.")
		return()
	endif()
	include_directories("${X11_INCLUDE_DIR}")
endif()


configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/browser-config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/browser-config.h")

include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${CEF_ROOT_DIR}")

if (APPLE)
	add_custom_command(
		OUTPUT
			${CMAKE_CURRENT_SOURCE_DIR}/streamelements/mach_exc.h
			${CMAKE_CURRENT_SOURCE_DIR}/streamelements/mach_excServer.c
			${CMAKE_CURRENT_SOURCE_DIR}/streamelements/mach_excUser.c
		MAIN_DEPENDENCY /Library/Developer/CommandLineTools/SDKs/MacOSX10.15.sdk/usr/include/mach/mach_exc.defs
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/streamelements
		COMMAND mig -v /Library/Developer/CommandLineTools/SDKs/MacOSX10.15.sdk/usr/include/mach/mach_exc.defs
	)
endif(APPLE)

# ----------------------------------------------------------------------------

if (ENABLE_BUGSPLAT)
	set(obs-browser_LIBRARIES
		libobs
		obs-frontend-api
		Qt5::Core
		Qt5::Widgets
		w32-pthreads
		${LIBCURL_LIBRARIES}
		${BUGSPLAT_LIBRARIES}
		)
else()
	set(obs-browser_LIBRARIES
		libobs
		obs-frontend-api
		Qt5::Core
		Qt5::Widgets
		#w32-pthreads
		${LIBCURL_LIBRARIES}
		)
endif()

list(APPEND obs-browser_LIBRARIES
	${CEF_LIBRARIES})

if (APPLE)
	list(APPEND obs-browser_LIBRARIES
		${CEF_LIBRARIES}
		"-framework IOKit"
		"-framework Foundation"
		"-framework System"
        "-framework SystemConfiguration"
	)
endif(APPLE)

if(BROWSER_PANEL_SUPPORT_ENABLED OR USE_QT_LOOP)
	if(DEFINED QTDIR${_lib_suffix})
		list(APPEND CMAKE_PREFIX_PATH "${QTDIR${_lib_suffix}}")
	elseif(DEFINED QTDIR)
		list(APPEND CMAKE_PREFIX_PATH "${QTDIR}")
	elseif(DEFINED ENV{QTDIR${_lib_suffix}})
		list(APPEND CMAKE_PREFIX_PATH "$ENV{QTDIR${_lib_suffix}}")
	elseif(DEFINED ENV{QTDIR})
		list(APPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
	endif()

	set(CMAKE_AUTOMOC TRUE)
	find_package(Qt5Widgets REQUIRED)
	list(APPEND obs-browser_LIBRARIES
		Qt5::Widgets
		)
endif()

if(MSVC)
	string(REPLACE "/MD" "/MT"
		"CMAKE_C_FLAGS"
		"${CMAKE_C_FLAGS}")

	string(REPLACE "/MD" "/MT"
		"CMAKE_CXX_FLAGS"
		"${CMAKE_CXX_FLAGS}")

	string(TOUPPER "${CMAKE_CONFIGURATION_TYPES}" UPPER_CONFIG_TYPES)
	foreach(CONFIG_TYPE ${UPPER_CONFIG_TYPES})
		string(REPLACE "/MD" "/MT"
			"CMAKE_C_FLAGS_${CONFIG_TYPE}"
			"${CMAKE_C_FLAGS_${CONFIG_TYPE}}")

		string(REPLACE "/MD" "/MT"
			"CMAKE_CXX_FLAGS_${CONFIG_TYPE}"
			"${CMAKE_CXX_FLAGS_${CONFIG_TYPE}}")
	endforeach()
	list(APPEND obs-browser_LIBRARIES
		d3d11
		dxgi
		)
endif()

set(obs-browser_SOURCES
	obs-browser-source.cpp
	obs-browser-source-audio.cpp
	obs-browser-plugin.cpp
	browser-scheme.cpp
	browser-client.cpp
	browser-app.cpp
	deps/json11/json11.cpp
	deps/base64/base64.cpp
	deps/wide-string.cpp
	streamelements/Version.cpp
	streamelements/StreamElementsAsyncTaskQueue.cpp
	streamelements/StreamElementsCefClient.cpp
	streamelements/StreamElementsBrowserWidget.cpp
	streamelements/StreamElementsBrowserWidgetManager.cpp
	streamelements/StreamElementsBandwidthTestClient.cpp
	streamelements/StreamElementsObsBandwidthTestClient.cpp
	streamelements/StreamElementsWidgetManager.cpp
	streamelements/StreamElementsObsAppMonitor.cpp
	streamelements/StreamElementsApiMessageHandler.cpp
	streamelements/StreamElementsConfig.cpp
	streamelements/StreamElementsGlobalStateManager.cpp
	streamelements/StreamElementsMenuManager.cpp
	streamelements/StreamElementsBandwidthTestManager.cpp
	streamelements/StreamElementsOutputSettingsManager.cpp
	streamelements/StreamElementsWorkerManager.cpp
	streamelements/StreamElementsBrowserDialog.cpp
	streamelements/StreamElementsUtils.cpp
	streamelements/StreamElementsHotkeyManager.cpp
	streamelements/StreamElementsReportIssueDialog.cpp
	streamelements/StreamElementsProgressDialog.cpp
	streamelements/StreamElementsPerformanceHistoryTracker.cpp
	streamelements/StreamElementsNetworkDialog.cpp
	streamelements/StreamElementsAnalyticsEventsManager.cpp
	streamelements/StreamElementsMessageBus.cpp
	streamelements/StreamElementsBrowserSourceApiMessageHandler.cpp
	streamelements/StreamElementsControllerServer.cpp
	streamelements/StreamElementsObsSceneManager.cpp
	streamelements/StreamElementsLocalWebFilesServer.cpp
	streamelements/StreamElementsExternalSceneDataProviderSlobsClient.cpp
	streamelements/StreamElementsHttpClient.cpp
	streamelements/StreamElementsNativeOBSControlsManager.cpp
	streamelements/StreamElementsCookieManager.cpp
	streamelements/StreamElementsProfilesManager.cpp
	streamelements/StreamElementsBackupManager.cpp
	streamelements/StreamElementsCleanupManager.cpp
	streamelements/StreamElementsPreviewManager.cpp
	streamelements/StreamElementsSceneItemsMonitor.cpp
	streamelements/StreamElementsDeferredExecutive.cpp
	streamelements/StreamElementsRemoteIconLoader.cpp
	streamelements/StreamElementsScenesListWidgetManager.cpp
	streamelements/StreamElementsPleaseWaitWindow.cpp
	streamelements/deps/StackWalker/StackWalker.cpp
	streamelements/deps/zip/zip.c
	streamelements/deps/server/NamedPipesServer.cpp
	streamelements/deps/server/NamedPipesServerClientHandler.cpp
	streamelements/deps/sqlite/sqlite3.c
	)

if (APPLE)
    list(APPEND obs-browser_SOURCES
        streamelements/StreamElementsUtils.mm
        streamelements/StreamElementsCrashHandler.mm
		streamelements/mach_excServer.c
		streamelements/mach_excUser.c
    )
else()
    list(APPEND obs-browser_SOURCES
        streamelements/StreamElementsCrashHandler.cpp
    )
endif(APPLE)

set(obs-browser_HEADERS
	obs-browser-source.hpp
	browser-scheme.hpp
	browser-client.hpp
	browser-app.hpp
	browser-version.h
	deps/json11/json11.hpp
	deps/base64/base64.hpp
	deps/wide-string.hpp
	cef-headers.hpp
	streamelements/Version.hpp
	streamelements/Version.generated.hpp
	streamelements/StreamElementsUtils.hpp
	streamelements/StreamElementsAsyncTaskQueue.hpp
	streamelements/StreamElementsCefClient.hpp
	streamelements/StreamElementsBrowserWidget.hpp
	streamelements/StreamElementsBrowserWidgetManager.hpp
	streamelements/StreamElementsBandwidthTestClient.hpp
	streamelements/StreamElementsObsBandwidthTestClient.hpp
	streamelements/StreamElementsWidgetManager.hpp
	streamelements/StreamElementsObsAppMonitor.hpp
	streamelements/StreamElementsBrowserMessageHandler.hpp
	streamelements/StreamElementsApiMessageHandler.hpp
	streamelements/StreamElementsConfig.hpp
	streamelements/StreamElementsGlobalStateManager.hpp
	streamelements/StreamElementsMenuManager.hpp
	streamelements/StreamElementsBandwidthTestManager.hpp
	streamelements/StreamElementsOutputSettingsManager.hpp
	streamelements/StreamElementsWorkerManager.cpp
	streamelements/StreamElementsBrowserDialog.hpp
	streamelements/StreamElementsHotkeyManager.hpp
	streamelements/StreamElementsReportIssueDialog.hpp
	streamelements/StreamElementsProgressDialog.hpp
	streamelements/StreamElementsPerformanceHistoryTracker.hpp
	streamelements/StreamElementsNetworkDialog.hpp
	streamelements/StreamElementsAnalyticsEventsManager.hpp
	streamelements/StreamElementsCrashHandler.hpp
	streamelements/StreamElementsMessageBus.hpp
	streamelements/StreamElementsBrowserSourceApiMessageHandler.hpp
	streamelements/StreamElementsControllerServer.hpp
	streamelements/StreamElementsObsSceneManager.hpp
	streamelements/StreamElementsFileSystemMapper.hpp
	streamelements/StreamElementsLocalWebFilesServer.hpp
	streamelements/StreamElementsExternalSceneDataProviderManager.hpp
	streamelements/StreamElementsExternalSceneDataProviderSlobsClient.hpp
	streamelements/StreamElementsExternalSceneDataProvider.hpp
	streamelements/StreamElementsHttpClient.hpp
	streamelements/StreamElementsNativeOBSControlsManager.hpp
	streamelements/StreamElementsCookieManager.hpp
	streamelements/StreamElementsProfilesManager.hpp
	streamelements/StreamElementsBackupManager.hpp
	streamelements/StreamElementsCleanupManager.hpp
	streamelements/StreamElementsPreviewManager.hpp
	streamelements/StreamElementsSceneItemsMonitor.hpp
	streamelements/StreamElementsDeferredExecutive.hpp
	streamelements/StreamElementsRemoteIconLoader.hpp
	streamelements/StreamElementsScenesListWidgetManager.hpp
	streamelements/StreamElementsPleaseWaitWindow.hpp
	streamelements/deps/StackWalker/StackWalker.h
	streamelements/deps/zip/zip.h
	streamelements/deps/zip/miniz.h
	streamelements/deps/moodycamel/concurrentqueue.h
	streamelements/deps/moodycamel/blockingconcurrentqueue.h
	streamelements/deps/server/NamedPipesServer.hpp
	streamelements/deps/server/NamedPipesServerClientHandler.hpp
	streamelements/deps/sqlite/sqlite3.h
	streamelements/deps/sqlite/sqlite3ext.h
	)

if (APPLE)
    list(APPEND obs-browser_HEADERS
		streamelements/mach_exc.h
    )
endif(APPLE)

# only allow browser panels on win32 for now -- other operating systems
# need more testing
if(WIN32 AND BROWSER_PANEL_SUPPORT_ENABLED)
	list(APPEND obs-browser_SOURCES
		panel/browser-panel.cpp
		panel/browser-panel-client.cpp
		)
	list(APPEND obs-browser_HEADERS
		panel/browser-panel.hpp
		panel/browser-panel-client.hpp
		panel/browser-panel-internal.hpp
		)
endif()

add_library(obs-browser MODULE
	${obs-browser_SOURCES}
	${obs-browser_HEADERS}
	${obs-browser-streamelements_QRC_SOURCES}
	)

target_link_libraries(obs-browser
	${obs-browser_LIBRARIES}
	)
if(USE_QT_LOOP)
	target_compile_definitions(obs-browser PRIVATE USE_QT_LOOP)
endif()

set_target_properties(obs-browser PROPERTIES FOLDER "plugins/obs-browser")

if (APPLE)
	set_target_properties(obs-browser PROPERTIES
		COMPILE_FLAGS "-mmacosx-version-min=10.15 -target x86_64-apple-macos10.15")
endif(APPLE)

# ----------------------------------------------------------------------------

set(obs-browser-page_SOURCES
	obs-browser-page/obs-browser-page-main.cpp
	browser-app.cpp
	deps/json11/json11.cpp
	)
set(obs-browser-page_HEADERS
	obs-browser-page/obs-browser-page-main.cpp
	browser-app.hpp
	deps/json11/json11.hpp
	cef-headers.hpp
	)

add_executable(obs-browser-page
	${obs-browser-page_SOURCES}
	${obs-browser-page_HEADERS}
	)
target_link_libraries(obs-browser-page
	${CEF_LIBRARIES}
	)
set_target_properties(obs-browser-page PROPERTIES FOLDER "plugins/obs-browser")

if (APPLE)
	set_target_properties(obs-browser-page PROPERTIES
		COMPILE_FLAGS "-mmacosx-version-min=10.15 -target x86_64-apple-macos10.15")
endif(APPLE)

if (WIN32)
	set_target_properties(obs-browser-page PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
endif(WIN32)

if (UNIX AND NOT APPLE)
    set_target_properties(obs-browser PROPERTIES INSTALL_RPATH "$ORIGIN/")
    set_target_properties(obs-browser-page PROPERTIES INSTALL_RPATH "$ORIGIN/")
endif()

# ----------------------------------------------------------------------------

if(ENABLE_ANGELSCRIPT)
	set(obs-browser-streamelements-restore-script-host_SOURCES
		streamelements/restore-script-host/main.rc
		streamelements/restore-script-host/main.cpp
		streamelements/restore-script-host/ScriptEngine.cpp
		${ANGELSCRIPT_ADDON_INCLUDE_DIR}/scriptstdstring/scriptstdstring.cpp
		#${ANGELSCRIPT_ADDON_INCLUDE_DIR}/scriptstdstring/scriptstdstring_utils.cpp
		${ANGELSCRIPT_ADDON_INCLUDE_DIR}/scriptbuilder/scriptbuilder.cpp
		)
	set(obs-browser-streamelements-restore-script-host_HEADERS
		streamelements/restore-script-host/main.hpp
		streamelements/restore-script-host/ScriptEngine.hpp
		)

	set(obs-browser-streamelements-restore-script-host_LIBRARIES
		${ANGELSCRIPT_LIBRARIES}
		#libobs
		#Qt5::Core
		#Qt5::Widgets
		#w32-pthreads
		#${LIBCURL_LIBRARIES}
		)

	add_executable(obs-browser-streamelements-restore-script-host
		${obs-browser-streamelements-restore-script-host_SOURCES}
		${obs-browser-streamelements-restore-script-host_HEADERS}
		)

	target_link_libraries(obs-browser-streamelements-restore-script-host
		${obs-browser-streamelements-restore-script-host_LIBRARIES}
		)

	if (APPLE)
		set_target_properties(obs-browser-streamelements-restore-script-host PROPERTIES
			COMPILE_FLAGS "-mmacosx-version-min=10.15 -target x86_64-apple-macos10.15")
	endif(APPLE)

	if (WIN32)
		set_target_properties(obs-browser-streamelements-restore-script-host PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
	endif(WIN32)

	if (APPLE AND XCODE)
		set_xcode_property(obs-browser-streamelements-restore-script-host CLANG_CXX_LIBRARY "libc++")
	endif(APPLE AND XCODE)
endif()

# ----------------------------------------------------------------------------

if (WIN32)
	math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")
	add_custom_command(TARGET obs-browser POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CEF_ROOT_DIR}/Resources"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
	)

	target_sources(obs-browser-page
		PRIVATE obs-browser-page.manifest
	)

	add_custom_command(TARGET obs-browser POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CEF_ROOT_DIR}/Release/chrome_elf.dll"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CEF_ROOT_DIR}/Release/libcef.dll"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CEF_ROOT_DIR}/Release/libEGL.dll"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CEF_ROOT_DIR}/Release/libGLESv2.dll"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CEF_ROOT_DIR}/Release/natives_blob.bin"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CEF_ROOT_DIR}/Release/snapshot_blob.bin"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CEF_ROOT_DIR}/Release/v8_context_snapshot.bin"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CMAKE_CURRENT_SOURCE_DIR}/streamelements/qt.conf"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/bin/${BITS}bit/"
	)

	if(ENABLE_BUGSPLAT)
		# Copy BugSplat binary files
		foreach( bugsplat_bin_file ${BUGSPLAT_BIN_FILES})
			add_custom_command(TARGET obs-browser POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy
				${bugsplat_bin_file}
				"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/bin/${BITS}bit/"
			)
		endforeach( bugsplat_bin_file )
	endif()
endif()

if(APPLE AND NOT BROWSER_DEPLOY)
	add_custom_command(TARGET obs-browser POST_BUILD
		COMMAND "install_name_tool" -change
		"@rpath/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework"
		"${CEF_LIBRARY}/Chromium Embedded Framework"
		"${CMAKE_BUILD_TYPE}/obs-browser.so")
	add_custom_command(TARGET obs-browser-page POST_BUILD
		COMMAND "install_name_tool" -change
		"@rpath/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework"
		"${CEF_LIBRARY}/Chromium Embedded Framework"
		"${CMAKE_BUILD_TYPE}/obs-browser-page")

	add_custom_command(TARGET obs-browser POST_BUILD
		COMMAND "/bin/mkdir" -p
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/Frameworks"
	)

	add_custom_command(TARGET obs-browser POST_BUILD
		COMMAND "/bin/cp" -R
		"${CEF_ROOT_DIR}/Release/Chromium Embedded Framework.framework"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/Frameworks/Chromium Embedded Framework.framework"
	)
endif()

if (UNIX AND NOT APPLE)
	install(DIRECTORY "${CEF_ROOT_DIR}/Resources/" DESTINATION "${OBS_PLUGIN_DESTINATION}")
	install(DIRECTORY "${CEF_ROOT_DIR}/Release/" DESTINATION "${OBS_PLUGIN_DESTINATION}")
endif()

install_obs_plugin_with_data(obs-browser data)
install_obs_plugin(obs-browser-page)

if (ENABLE_ANGELSCRIPT)
	install_obs_plugin(obs-browser-streamelements-restore-script-host)
endif()
