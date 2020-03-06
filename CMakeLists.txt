cmake_minimum_required(VERSION 2.8.12)
project(obs-browser)

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
find_package(BugSplat REQUIRED)
find_package(AngelScript REQUIRED)

include_directories(SYSTEM
	${Qt5Core_INCLUDES}
	${Qt5Widgets_INCLUDES}
	${LIBCURL_INCLUDE_DIRS}
	${BUGSPLAT_INCLUDE_DIR}
	${ANGELSCRIPT_INCLUDE_DIR}
	${ANGELSCRIPT_ADDON_INCLUDE_DIR}
)

#qt5_wrap_ui(NetworkDialog "networkdialog.ui")
qt5_add_resources(obs-browser-streamelements_QRC_SOURCES "streamelements/streamelements.qrc")

if(NOT CEF_FOUND)
	message(STATUS "CEF Not found -- obs-browser plugin disabled.")
	return()
endif()

if (NOT BUGSPLAT_FOUND)
	message(STATUS "BugSplat Not found -- obs-browser plugin disabled.")
	return()
endif()

if (NOT ANGELSCRIPT_FOUND)
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

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/browser-config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/browser-config.h")

include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${CEF_ROOT_DIR}")

# ----------------------------------------------------------------------------

set(obs-browser_LIBRARIES
	libobs
	obs-frontend-api
	Qt5::Core
	Qt5::Widgets
	w32-pthreads
	${LIBCURL_LIBRARIES}
	${BUGSPLAT_LIBRARIES}
	)

list(APPEND obs-browser_LIBRARIES
	${CEF_LIBRARIES})

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
	streamelements/StreamElementsCrashHandler.cpp
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
	streamelements/deps/StackWalker/StackWalker.cpp
	streamelements/deps/zip/zip.c
	streamelements/deps/server/NamedPipesServer.cpp
	streamelements/deps/server/NamedPipesServerClientHandler.cpp
	streamelements/deps/sqlite/sqlite3.c
	)
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

if (APPLE)
	set_target_properties(obs-browser-page PROPERTIES
		COMPILE_FLAGS "-mmacosx-version-min=10.8")
endif(APPLE)

if (WIN32)
	set_target_properties(obs-browser-page PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
endif(WIN32)

if (UNIX AND NOT APPLE)
    set_target_properties(obs-browser PROPERTIES INSTALL_RPATH "$ORIGIN/")
    set_target_properties(obs-browser-page PROPERTIES INSTALL_RPATH "$ORIGIN/")
endif()

# ----------------------------------------------------------------------------

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
		COMPILE_FLAGS "-mmacosx-version-min=10.8")
endif(APPLE)

if (WIN32)
	set_target_properties(obs-browser-streamelements-restore-script-host PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
endif(WIN32)

if (APPLE AND XCODE)
	set_xcode_property(obs-browser-streamelements-restore-script-host CLANG_CXX_LIBRARY "libc++")
endif(APPLE AND XCODE)

# ----------------------------------------------------------------------------

if (WIN32)
	math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")
	add_custom_command(TARGET obs-browser POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CEF_ROOT_DIR}/Resources"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
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

	# Copy BugSplat binary files
	foreach( bugsplat_bin_file ${BUGSPLAT_BIN_FILES})
		add_custom_command(TARGET obs-browser POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy
			${bugsplat_bin_file}
			"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/bin/${BITS}bit/"
		)
	endforeach( bugsplat_bin_file )
endif()

if(APPLE AND NOT BROWSER_DEPLOY)
	add_custom_command(TARGET obs-browser POST_BUILD
		COMMAND "install_name_tool" -change
		"@rpath/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework"
		"${CEF_LIBRARY}/Chromium Embedded Framework"
		"obs-browser.so")
	add_custom_command(TARGET obs-browser-page POST_BUILD
		COMMAND "install_name_tool" -change
		"@rpath/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework"
		"${CEF_LIBRARY}/Chromium Embedded Framework"
		"obs-browser-page")
endif()

if(APPLE AND NOT BROWSER_DEPLOY)
	add_custom_command(TARGET obs-browser POST_BUILD
		COMMAND "install_name_tool" -change
		"@rpath/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework"
		"${CEF_LIBRARY}/Chromium Embedded Framework"
		"obs-browser.so")
	add_custom_command(TARGET obs-browser-page POST_BUILD
		COMMAND "install_name_tool" -change
		"@rpath/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework"
		"${CEF_LIBRARY}/Chromium Embedded Framework"
		"obs-browser-page")
endif()

if(APPLE AND NOT BROWSER_DEPLOY)
	add_custom_command(TARGET obs-browser POST_BUILD
		COMMAND "install_name_tool" -change
		"@rpath/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework"
		"${CEF_LIBRARY}/Chromium Embedded Framework"
		"obs-browser.so")
	add_custom_command(TARGET obs-browser-page POST_BUILD
		COMMAND "install_name_tool" -change
		"@rpath/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework"
		"${CEF_LIBRARY}/Chromium Embedded Framework"
		"obs-browser-page")
endif()

if (UNIX AND NOT APPLE)
	install(DIRECTORY "${CEF_ROOT_DIR}/Resources/" DESTINATION "${OBS_PLUGIN_DESTINATION}")
	install(DIRECTORY "${CEF_ROOT_DIR}/Release/" DESTINATION "${OBS_PLUGIN_DESTINATION}")
endif()

if(APPLE AND NOT BROWSER_DEPLOY)
	add_custom_command(TARGET obs-browser POST_BUILD
		COMMAND "install_name_tool" -change
		"@rpath/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework"
		"${CEF_LIBRARY}/Chromium Embedded Framework"
		"obs-browser.so")
	add_custom_command(TARGET obs-browser-page POST_BUILD
		COMMAND "install_name_tool" -change
		"@rpath/Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework"
		"${CEF_LIBRARY}/Chromium Embedded Framework"
		"obs-browser-page")
endif()

install_obs_plugin_with_data(obs-browser data)
install_obs_plugin(obs-browser-page)
install_obs_plugin(obs-browser-streamelements-restore-script-host)
