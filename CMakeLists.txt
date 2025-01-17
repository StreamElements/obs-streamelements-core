if(CMAKE_PROJECT_NAME STREQUAL "obs-studio")
  #find_qt is replaced with find_package
  #install_obs_plugin_with_data is replaced with set_target_properties_obs
  set(BUILD_OUT_OF_TREE OFF)
else()
  set(BUILD_OUT_OF_TREE ON)
endif()

project(obs-streamelements-core)

if(NOT QT_VERSION)
  set(QT_VERSION
      "5"
      CACHE STRING "OBS Qt version [5, 6]" FORCE)
  set_property(CACHE QT_VERSION PROPERTY STRINGS 5 6)
endif()

if(BUILD_OUT_OF_TREE)
	#find_qt(VERSION ${QT_VERSION} COMPONENTS Core Widgets)
	find_qt(VERSION ${QT_VERSION} COMPONENTS Widgets)
else()
  find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
endif()

if (WIN32)
	set(ENABLE_BUGSPLAT TRUE)
	set(ENABLE_ANGELSCRIPT TRUE)

	add_compile_options(
		/wd4267
		/wd4091
		/wd4554
		/wd4099
		/wd4996
		/wd4091
		/wd4273
		/wd4018
	)
else()
	set(ENABLE_BUGSPLAT FALSE)
	set(ENABLE_ANGELSCRIPT FALSE)
endif(WIN32)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/deps")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/streamelements/deps")

find_package(CURL REQUIRED)

if (ENABLE_ANGELSCRIPT)
	include_directories(SYSTEM
		deps/angelscript/include
		deps/angelscript/add_on
	)
endif()

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
	set(BUGSPLAT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/BugSplat/BugSplat")
	set(BUGSPLAT_INCLUDE_DIR "${BUGSPLAT_ROOT_DIR}/inc")

	include_directories(SYSTEM ${BUGSPLAT_INCLUDE_DIR})

	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		find_library(BUGSPLAT_LIBRARY
			NAMES BugSplat64.lib "BugSplat"
			PATHS ${BUGSPLAT_ROOT_DIR} ${BUGSPLAT_ROOT_DIR}/x64/Release)

		set(BUGSPLAT_BIN_DIR "${BUGSPLAT_ROOT_DIR}/x64/Release")
		set(BUGSPLAT_BIN_FILES
			${BUGSPLAT_BIN_DIR}/BsSndRpt64.exe
			${BUGSPLAT_BIN_DIR}/BugSplat64.dll
			${BUGSPLAT_BIN_DIR}/BugSplatHD64.exe
			${BUGSPLAT_BIN_DIR}/BugSplatRc64.dll
		)
	elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
		find_library(BUGSPLAT_LIBRARY
			NAMES BugSplat.lib "BugSplat"
			PATHS ${BUGSPLAT_ROOT_DIR} ${BUGSPLAT_ROOT_DIR}/Win32/Release)

		set(BUGSPLAT_BIN_DIR "${BUGSPLAT_ROOT_DIR}/Win32/Release")
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
		#Qt::Core
		Qt::Widgets
		w32-pthreads
		${BUGSPLAT_LIBRARIES}
	)
else()
	set(obs-streamelements-core_LIBRARIES
		libobs
		obs-frontend-api
		#Qt::Core
		Qt::Widgets
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

set(CMAKE_AUTOMOC TRUE)
list(APPEND obs-streamelements-core_LIBRARIES
	Qt::Widgets
	)

if(MSVC)
	string(REPLACE "/MD" "/MT"
		"CMAKE_C_FLAGS"
		"${CMAKE_C_FLAGS}")

	string(REPLACE "/MD" "/MT"
		"CMAKE_CXX_FLAGS"
		"${CMAKE_CXX_FLAGS}")

	string(REPLACE "/WX" "/W1"
		"CMAKE_C_FLAGS"
		"${CMAKE_C_FLAGS}")

	string(REPLACE "/WX" "/W1"
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

		string(REPLACE "/WX" "/W1"
			"CMAKE_C_FLAGS_${CONFIG_TYPE}"
			"${CMAKE_C_FLAGS_${CONFIG_TYPE}}")

		string(REPLACE "/WX" "/W1"
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
	streamelements/audio-wrapper-source.c
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
	streamelements/StreamElementsVideoComposition.cpp
	streamelements/StreamElementsVideoCompositionManager.cpp
	streamelements/StreamElementsVideoCompositionViewWidget.cpp
	streamelements/StreamElementsAudioComposition.cpp
	streamelements/StreamElementsAudioCompositionManager.cpp
	streamelements/StreamElementsOutput.cpp
	streamelements/StreamElementsOutputManager.cpp
	streamelements/StreamElementsObsActiveSceneTracker.cpp
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
	streamelements/audio-wrapper-source.h
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
	streamelements/StreamElementsVideoComposition.hpp
	streamelements/StreamElementsVideoCompositionManager.hpp
	streamelements/StreamElementsVideoCompositionViewWidget.hpp
	streamelements/StreamElementsAudioComposition.hpp
	streamelements/StreamElementsAudioCompositionManager.hpp
	streamelements/StreamElementsOutput.hpp
	streamelements/StreamElementsOutputManager.hpp
	streamelements/StreamElementsObsActiveSceneTracker.hpp
	streamelements/canvas-config.hpp
	streamelements/canvas-scan.hpp
	streamelements/canvas-math.hpp
	streamelements/canvas-draw.hpp
	streamelements/canvas-mutate.hpp
	streamelements/canvas-controls.hpp
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
	streamelements/streamelements.qrc
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

if (${QT_VERSION} EQUAL "5")
#	qt5_add_resources(obs-streamelements-core-streamelements_QRC_SOURCES "streamelements/streamelements.qrc")
endif()

add_library(obs-streamelements-core MODULE
	${obs-streamelements-core_SOURCES}
	${obs-streamelements-core_HEADERS}
	${obs-streamelements-core-streamelements_QRC_SOURCES}
)

if (${QT_VERSION} EQUAL "6")
	qt6_add_resources(obs-streamelements-core "streamelements/streamelements.qrc"
		PREFIX "/"
		BASE "streamelements"
		FILES
		"streamelements/images/logo.png"
		"streamelements/images/icon.png"
		"streamelements/images/icon.ico"
		"streamelements/images/logo_137x30.png"
		"streamelements/images/logo_100x30.png"
		"streamelements/images/toolbar/back.ico"
		"streamelements/images/toolbar/forward.ico"
		"streamelements/images/toolbar/home.ico"
		"streamelements/images/toolbar/reload.ico"
		"streamelements/images/toolbar/reload.png"
		"streamelements/images/toolbar/close.png"
		"streamelements/images/toolbar/dockToggle.png"
		"streamelements/html/error.html"
		"streamelements/html/loading.html"
	)
endif()

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

	if(MSVC AND CMAKE_CL_64)
			enable_language(ASM_MASM)
			if(CMAKE_ASM_MASM_COMPILER_WORKS)
				ADD_CUSTOM_COMMAND(OUTPUT as_callfunc_x64_msvc_asm.obj
					COMMAND "${CMAKE_ASM_MASM_COMPILER}" -c "${CMAKE_CURRENT_SOURCE_DIR}/deps/angelscript/source/as_callfunc_x64_msvc_asm.asm"
					DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/deps/angelscript/source/as_callfunc_x64_msvc_asm.asm"
					COMMENT "generate as_callfunc_x64_msvc_asm.obj")

				add_executable(obs-streamelements-core-streamelements-restore-script-host
					${obs-streamelements-core-streamelements-restore-script-host_SOURCES}
					${obs-streamelements-core-streamelements-restore-script-host_HEADERS}
					as_callfunc_x64_msvc_asm.obj
				)
			else()
					message(FATAL ERROR "MSVC x86_64 target requires a working assembler")
			endif()
	else()
		add_executable(obs-streamelements-core-streamelements-restore-script-host
			${obs-streamelements-core-streamelements-restore-script-host_SOURCES}
			${obs-streamelements-core-streamelements-restore-script-host_HEADERS}
		)
	endif()

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


if(BUILD_OUT_OF_TREE)
	install_obs_plugin_with_data(obs-streamelements-core data)
else()
	set_target_properties_obs(obs-streamelements-core PROPERTIES FOLDER "plugins/streamelements" PREFIX "")
endif()

if (ENABLE_ANGELSCRIPT)
	if(BUILD_OUT_OF_TREE)
		install_obs_plugin(obs-streamelements-core-streamelements-restore-script-host)
	else()
		set_target_properties_obs(obs-streamelements-core-streamelements-restore-script-host PROPERTIES FOLDER "plugins/streamelements" PREFIX "")
	endif()
endif()
