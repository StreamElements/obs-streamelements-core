cmake_minimum_required(VERSION 2.8.12)
project(obs-streamelements-core)

macro(find_qt)
  set(oneValueArgs VERSION)
  set(multiValueArgs COMPONENTS COMPONENTS_WIN COMPONENTS_MAC COMPONENTS_LINUX)
  cmake_parse_arguments(FIND_QT "" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  if(OS_WINDOWS)
    find_package(
      Qt${FIND_QT_VERSION}
      COMPONENTS ${FIND_QT_COMPONENTS} ${FIND_QT_COMPONENTS_WIN}
      REQUIRED)
  elseif(OS_MACOS)
    find_package(
      Qt${FIND_QT_VERSION}
      COMPONENTS ${FIND_QT_COMPONENTS} ${FIND_QT_COMPONENTS_MAC}
      REQUIRED)
  else()
    find_package(
      Qt${FIND_QT_VERSION}
      COMPONENTS ${FIND_QT_COMPONENTS} ${FIND_QT_COMPONENTS_LINUX}
      REQUIRED)
  endif()

  foreach(_COMPONENT IN LISTS FIND_QT_COMPONENTS FIND_QT_COMPONENTS_WIN
                              FIND_QT_COMPONENTS_MAC FIND_QT_COMPONENTS_LINUX)
    if(NOT TARGET Qt::${_COMPONENT} AND TARGET
                                        Qt${FIND_QT_VERSION}::${_COMPONENT})

      add_library(Qt::${_COMPONENT} INTERFACE IMPORTED)
      set_target_properties(
        Qt::${_COMPONENT} PROPERTIES INTERFACE_LINK_LIBRARIES
                                     "Qt${FIND_QT_VERSION}::${_COMPONENT}")
    endif()
  endforeach()
endmacro()

if(NOT QT_VERSION)
  set(QT_VERSION
      "5"
      CACHE STRING "OBS Qt version [5, 6]" FORCE)
  set_property(CACHE QT_VERSION PROPERTY STRINGS 5 6)
endif()

find_qt(VERSION ${QT_VERSION} COMPONENTS Core Widgets)

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
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/streamelements/deps")

find_package(CURL REQUIRED)

include_directories(SYSTEM
	${Qt5Core_INCLUDES}
	${Qt5Widgets_INCLUDES}
)

if (ENABLE_ANGELSCRIPT)
	include_directories(SYSTEM
		deps/angelscript/include
		deps/angelscript/add_on
	)
endif()

#qt5_wrap_ui(NetworkDialog "networkdialog.ui")
qt5_add_resources(obs-streamelements-core-streamelements_QRC_SOURCES "streamelements/streamelements.qrc")

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

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
	set(BUGSPLAT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/BugSplat")
	set(BUGSPLAT_INCLUDE_DIR "${BUGSPLAT_ROOT_DIR}/inc")

	include_directories(SYSTEM ${BUGSPLAT_INCLUDE_DIR})

	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		find_library(BUGSPLAT_LIBRARY
			NAMES BugSplat64.lib "BugSplat"
			PATHS ${BUGSPLAT_ROOT_DIR} ${BUGSPLAT_ROOT_DIR}/lib64)

		set(BUGSPLAT_BIN_DIR "${BUGSPLAT_ROOT_DIR}/bin64")
		set(BUGSPLAT_BIN_FILES
			${BUGSPLAT_BIN_DIR}/BsSndRpt64.exe
			${BUGSPLAT_BIN_DIR}/BugSplat64.dll
			${BUGSPLAT_BIN_DIR}/BugSplatHD64.exe
			${BUGSPLAT_BIN_DIR}/BugSplatRc64.dll
		)
	elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
		find_library(BUGSPLAT_LIBRARY
			NAMES BugSplat64.lib "BugSplat"
			PATHS ${BUGSPLAT_ROOT_DIR} ${BUGSPLAT_ROOT_DIR}/lib)

		set(BUGSPLAT_BIN_DIR "${BUGSPLAT_ROOT_DIR}/bin")
		set(BUGSPLAT_BIN_FILES
			${BUGSPLAT_BIN_DIR}/BsSndRpt.exe
			${BUGSPLAT_BIN_DIR}/BugSplat.dll
			${BUGSPLAT_BIN_DIR}/BugSplatHD.exe
			${BUGSPLAT_BIN_DIR}/BugSplatRc.dll
		)
	else()
		message(ERROR_FATAL "BugSplat: could not detect CPU architecture")
	endif()

	set(BUGSPLAT_LIBRARIES
			optimized ${BUGSPLAT_LIBRARY})

	set(obs-streamelements-core_LIBRARIES
		libobs
		obs-frontend-api
		Qt5::Core
		Qt5::Widgets
		w32-pthreads
		${BUGSPLAT_LIBRARIES}
	)
else()
	set(obs-streamelements-core_LIBRARIES
		libobs
		obs-frontend-api
		Qt5::Core
		Qt5::Widgets
		#w32-pthreads
		)
endif()

if (APPLE)
	list(APPEND obs-streamelements-core_LIBRARIES
		"-framework AppKit"
		"-framework IOKit"
		"-framework Foundation"
		"-framework System"
        "-framework SystemConfiguration"
	)

	find_library(COREFOUNDATION CoreFoundation)
	list(APPEND obs-streamelements-core_LIBRARIES
			${COREFOUNDATION})
endif()

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
list(APPEND obs-streamelements-core_LIBRARIES
	Qt5::Widgets
	)

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
endif()

set(obs-streamelements-core_SOURCES
	obs-streamelements-core-plugin.cpp
	deps/json11/json11.cpp
	deps/base64/base64.cpp
	deps/wide-string.cpp
	deps/cef-stub/cef_process_message.cpp
	deps/cef-stub/cef_string.cpp
	deps/cef-stub/cef_value.cpp
	deps/cef-stub/cef_value_binary.cpp
	deps/cef-stub/cef_value_dictionary.cpp
	deps/cef-stub/cef_value_json.cpp
	deps/cef-stub/cef_value_list.cpp
	streamelements/Version.cpp
	streamelements/StreamElementsAsyncTaskQueue.cpp
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
	streamelements/StreamElementsControllerServer.cpp
	streamelements/StreamElementsObsSceneManager.cpp
	streamelements/StreamElementsExternalSceneDataProviderSlobsClient.cpp
	streamelements/StreamElementsHttpClient.cpp
	streamelements/StreamElementsNativeOBSControlsManager.cpp
	streamelements/StreamElementsProfilesManager.cpp
	streamelements/StreamElementsBackupManager.cpp
	streamelements/StreamElementsCleanupManager.cpp
	streamelements/StreamElementsPreviewManager.cpp
	streamelements/StreamElementsSceneItemsMonitor.cpp
	streamelements/StreamElementsDeferredExecutive.cpp
	streamelements/StreamElementsRemoteIconLoader.cpp
	streamelements/StreamElementsScenesListWidgetManager.cpp
	streamelements/StreamElementsPleaseWaitWindow.cpp
	streamelements/StreamElementsHttpServerManager.cpp
	streamelements/StreamElementsWebsocketApiServer.cpp
	streamelements/StreamElementsLocalFilesystemHttpServer.cpp
	streamelements/deps/StackWalker/StackWalker.cpp
	streamelements/deps/zip/zip.c
	streamelements/deps/server/NamedPipesServer.cpp
	streamelements/deps/server/NamedPipesServerClientHandler.cpp
	streamelements/deps/server/HttpServer.cpp
	streamelements/deps/sqlite/sqlite3.c
	)

if (APPLE)
    list(APPEND obs-streamelements-core_SOURCES
        streamelements/StreamElementsUtils.mm
        streamelements/StreamElementsCrashHandler.mm
		streamelements/mach_excServer.c
		streamelements/mach_excUser.c
    )
else()
    list(APPEND obs-streamelements-core_SOURCES
        streamelements/StreamElementsCrashHandler.cpp
    )
endif(APPLE)

set(obs-streamelements-core_HEADERS
	deps/json11/json11.hpp
	deps/base64/base64.hpp
	deps/obs-websocket-api/obs-websocket-api.h
	deps/wide-string.hpp
	deps/cef-stub/cef_process_message.hpp
	deps/cef-stub/cef_string.hpp
	deps/cef-stub/cef_value.hpp
	cef-headers.hpp
	streamelements/deps/utf8.h
	streamelements/deps/utf8/checked.h
	streamelements/deps/utf8/unchecked.h
	streamelements/deps/utf8/core.h
	streamelements/deps/utf8/cpp11.h
	streamelements/deps/utf8/cpp17.h
	streamelements/Version.hpp
	streamelements/Version.generated.hpp
	streamelements/StreamElementsUtils.hpp
	streamelements/StreamElementsAsyncTaskQueue.hpp
	streamelements/StreamElementsBrowserWidget.hpp
	streamelements/StreamElementsBrowserWidgetManager.hpp
	streamelements/StreamElementsBandwidthTestClient.hpp
	streamelements/StreamElementsObsBandwidthTestClient.hpp
	streamelements/StreamElementsWidgetManager.hpp
	streamelements/StreamElementsObsAppMonitor.hpp
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
	streamelements/StreamElementsControllerServer.hpp
	streamelements/StreamElementsObsSceneManager.hpp
	streamelements/StreamElementsExternalSceneDataProviderManager.hpp
	streamelements/StreamElementsExternalSceneDataProviderSlobsClient.hpp
	streamelements/StreamElementsExternalSceneDataProvider.hpp
	streamelements/StreamElementsHttpClient.hpp
	streamelements/StreamElementsNativeOBSControlsManager.hpp
	streamelements/StreamElementsProfilesManager.hpp
	streamelements/StreamElementsBackupManager.hpp
	streamelements/StreamElementsCleanupManager.hpp
	streamelements/StreamElementsPreviewManager.hpp
	streamelements/StreamElementsSceneItemsMonitor.hpp
	streamelements/StreamElementsDeferredExecutive.hpp
	streamelements/StreamElementsRemoteIconLoader.hpp
	streamelements/StreamElementsScenesListWidgetManager.hpp
	streamelements/StreamElementsPleaseWaitWindow.hpp
	streamelements/StreamElementsHttpServerManager.hpp
	streamelements/StreamElementsWebsocketApiServer.hpp
	streamelements/StreamElementsLocalFilesystemHttpServer.hpp
	streamelements/deps/StackWalker/StackWalker.h
	streamelements/deps/zip/zip.h
	streamelements/deps/zip/miniz.h
	streamelements/deps/moodycamel/concurrentqueue.h
	streamelements/deps/moodycamel/blockingconcurrentqueue.h
	streamelements/deps/server/NamedPipesServer.hpp
	streamelements/deps/server/NamedPipesServerClientHandler.hpp
	streamelements/deps/server/HttpServer.hpp
	streamelements/deps/sqlite/sqlite3.h
	streamelements/deps/sqlite/sqlite3ext.h
	streamelements/deps/cpp-httplib/httplib.h
	)

if (APPLE)
    list(APPEND obs-streamelements-core_HEADERS
		streamelements/mach_exc.h
		macutil.mm
    )
endif()

if (APPLE)
	list(APPEND obs-streamelements-core_LIBRARIES objc)
endif()

add_library(obs-streamelements-core MODULE
	${obs-streamelements-core_SOURCES}
	${obs-streamelements-core_HEADERS}
	${obs-streamelements-core-streamelements_QRC_SOURCES}
	)

target_link_libraries(obs-streamelements-core
	${obs-streamelements-core_LIBRARIES}
	CURL::libcurl
	)

set_target_properties(obs-streamelements-core PROPERTIES FOLDER "plugins/obs-streamelements-core")

if (APPLE)
	set_target_properties(obs-streamelements-core PROPERTIES
		COMPILE_FLAGS "-mmacosx-version-min=10.15 -target x86_64-apple-macos10.15")
endif(APPLE)

# ----------------------------------------------------------------------------

if (WIN32)
	set_target_properties(obs-streamelements-core PROPERTIES LINK_FLAGS "/IGNORE:4099")
endif(WIN32)

if (UNIX AND NOT APPLE)
    set_target_properties(obs-streamelements-core PROPERTIES INSTALL_RPATH "$ORIGIN/")
endif()

# ----------------------------------------------------------------------------

if(ENABLE_ANGELSCRIPT)
	set(obs-streamelements-core-streamelements-restore-script-host_SOURCES
		streamelements/restore-script-host/main.rc
		streamelements/restore-script-host/main.cpp
		streamelements/restore-script-host/ScriptEngine.cpp
		deps/angelscript/source/as_atomic.cpp
		deps/angelscript/source/as_builder.cpp
		deps/angelscript/source/as_bytecode.cpp
		deps/angelscript/source/as_callfunc.cpp
		deps/angelscript/source/as_callfunc_mips.cpp
		deps/angelscript/source/as_callfunc_ppc.cpp
		deps/angelscript/source/as_callfunc_ppc_64.cpp
		deps/angelscript/source/as_callfunc_sh4.cpp
		deps/angelscript/source/as_callfunc_x64_gcc.cpp
		deps/angelscript/source/as_callfunc_x64_mingw.cpp
		deps/angelscript/source/as_callfunc_x64_msvc.cpp
		deps/angelscript/source/as_callfunc_x86.cpp
		deps/angelscript/source/as_callfunc_xenon.cpp
		deps/angelscript/source/as_compiler.cpp
		deps/angelscript/source/as_configgroup.cpp
		deps/angelscript/source/as_context.cpp
		deps/angelscript/source/as_datatype.cpp
		deps/angelscript/source/as_gc.cpp
		deps/angelscript/source/as_generic.cpp
		deps/angelscript/source/as_globalproperty.cpp
		deps/angelscript/source/as_memory.cpp
		deps/angelscript/source/as_module.cpp
		deps/angelscript/source/as_objecttype.cpp
		deps/angelscript/source/as_outputbuffer.cpp
		deps/angelscript/source/as_parser.cpp
		deps/angelscript/source/as_restore.cpp
		deps/angelscript/source/as_scriptcode.cpp
		deps/angelscript/source/as_scriptengine.cpp
		deps/angelscript/source/as_scriptfunction.cpp
		deps/angelscript/source/as_scriptnode.cpp
		deps/angelscript/source/as_scriptobject.cpp
		deps/angelscript/source/as_string.cpp
		deps/angelscript/source/as_string_util.cpp
		deps/angelscript/source/as_thread.cpp
		deps/angelscript/source/as_tokenizer.cpp
		deps/angelscript/source/as_typeinfo.cpp
		deps/angelscript/source/as_variablescope.cpp
		deps/angelscript/add_on/scriptstdstring/scriptstdstring.cpp
		deps/angelscript/add_on/scriptbuilder/scriptbuilder.cpp
	)

	if(MSVC AND CMAKE_CL_64)
			enable_language(ASM_MASM)
			if(CMAKE_ASM_MASM_COMPILER_WORKS)
					set(obs-streamelements-core-streamelements-restore-script-host_SOURCES
						${obs-streamelements-core-streamelements-restore-script-host_SOURCES}
						deps/angelscript/source/as_callfunc_x64_msvc_asm.asm
					)
			else()
					message(FATAL ERROR "MSVC x86_64 target requires a working assembler")
			endif()
	endif()

	if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "^arm")
			enable_language(ASM)
			if(CMAKE_ASM_COMPILER_WORKS)
					set(obs-streamelements-core-streamelements-restore-script-host_SOURCES
						${obs-streamelements-core-streamelements-restore-script-host_SOURCES}
						deps/angelscript/source/as_callfunc_arm.cpp
						deps/angelscript/source/as_callfunc_arm_gcc.S
					)


					set_property(SOURCE
						deps/angelscript/source/as_callfunc_arm_gcc.S
						APPEND PROPERTY COMPILE_FLAGS " -Wa,-mimplicit-it=always"
					)
			else()
					message(FATAL ERROR "ARM target requires a working assembler")
			endif()
	endif()

	set(obs-streamelements-core-streamelements-restore-script-host_HEADERS
		streamelements/restore-script-host/main.hpp
		streamelements/restore-script-host/ScriptEngine.hpp
		)

	#if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
	#endif()

	add_executable(obs-streamelements-core-streamelements-restore-script-host
		${obs-streamelements-core-streamelements-restore-script-host_SOURCES}
		${obs-streamelements-core-streamelements-restore-script-host_HEADERS}
		)

	target_link_libraries(obs-streamelements-core-streamelements-restore-script-host
		${obs-streamelements-core-streamelements-restore-script-host_LIBRARIES}
		)

	if (APPLE)
		set_target_properties(obs-streamelements-core-streamelements-restore-script-host PROPERTIES
			COMPILE_FLAGS "-mmacosx-version-min=10.15 -target x86_64-apple-macos10.15")
	endif(APPLE)

	if (WIN32)
		set_target_properties(obs-streamelements-core-streamelements-restore-script-host PROPERTIES COMPILE_FLAGS "/MP /MT")
		set_target_properties(obs-streamelements-core-streamelements-restore-script-host PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
	endif(WIN32)

	if (APPLE AND XCODE)
		set_xcode_property(obs-streamelements-core-streamelements-restore-script-host CLANG_CXX_LIBRARY "libc++")
	endif(APPLE AND XCODE)
endif()

# ----------------------------------------------------------------------------

if (WIN32)
	math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")

	add_custom_command(TARGET obs-streamelements-core POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
		"${CMAKE_CURRENT_SOURCE_DIR}/streamelements/qt.conf"
		"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/bin/${BITS}bit/"
	)

	if(ENABLE_BUGSPLAT)
		# Copy BugSplat binary files
		foreach( bugsplat_bin_file ${BUGSPLAT_BIN_FILES})
			add_custom_command(TARGET obs-streamelements-core POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy
				${bugsplat_bin_file}
				"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/bin/${BITS}bit/"
			)
		endforeach( bugsplat_bin_file )
	endif()
endif()

install_obs_plugin_with_data(obs-streamelements-core data)

if (ENABLE_ANGELSCRIPT)
	install_obs_plugin(obs-streamelements-core-streamelements-restore-script-host)
endif()
