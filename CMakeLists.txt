if(CMAKE_PROJECT_NAME STREQUAL "obs-studio")
  #find_qt is replaced with find_package
  #install_obs_plugin_with_data is replaced with set_target_properties_obs
  set(BUILD_OUT_OF_TREE OFF)
else()
  set(BUILD_OUT_OF_TREE ON)
endif()

project(obs-streamelements-core)

set(CMAKE_AUTORCC ON)

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

# Which crash-reporting backend is compiled in. CI drives this; it decides in
# one place which implementation compiles, which SDK is linked, and which
# SE_CRASH_HANDLER_* definition the factory in StreamElementsCrashHandler.cpp
# sees -- so those three can never disagree. Exactly one backend is ever linked,
# which is what keeps two of them from contending for the process-wide
# exception filter.
set(STREAMELEMENTS_CRASH_HANDLER "bugsplat"
	CACHE STRING "Crash-reporting backend: bugsplat | sentry | none")
set_property(CACHE STREAMELEMENTS_CRASH_HANDLER PROPERTY STRINGS bugsplat sentry none)

set(ENABLE_BUGSPLAT FALSE)
set(ENABLE_SENTRY FALSE)

if (STREAMELEMENTS_CRASH_HANDLER STREQUAL "bugsplat")
	set(ENABLE_BUGSPLAT TRUE)
	add_compile_definitions(SE_CRASH_HANDLER_BUGSPLAT)
elseif (STREAMELEMENTS_CRASH_HANDLER STREQUAL "sentry")
	set(ENABLE_SENTRY TRUE)
	add_compile_definitions(SE_CRASH_HANDLER_SENTRY)
elseif (STREAMELEMENTS_CRASH_HANDLER STREQUAL "none")
	# Nothing to enable -- the factory returns nullptr.
else()
	message(FATAL_ERROR
		"STREAMELEMENTS_CRASH_HANDLER must be 'bugsplat', 'sentry' or 'none', got '${STREAMELEMENTS_CRASH_HANDLER}'")
endif()

message(STATUS "obs-streamelements-core: crash handler backend: ${STREAMELEMENTS_CRASH_HANDLER}")

# Razer WYVRN: Chroma RGB lighting and Sensa HD haptics.
#
# Windows-only, because the SDK is. One variable decides both which sources
# compile and whether SE_ENABLE_WYVRN is defined, so the two cannot disagree --
# the same shape as the crash-handler switch above.
#
# The integration is optional at RUNTIME as well: the SDK loads
# RzChromatic64.dll dynamically and verifies its Authenticode signature, so there
# is no link-time dependency on Razer and a machine without Synapse simply
# reports the subsystem unavailable.
# Off for now. The integration is complete and tested, but is being held back
# from a release while the hardware behaviour is confirmed -- with this off,
# none of it compiles and none of its API is registered, so a build ships no
# trace of it. Turning this back ON is the whole of reintegration.
option(STREAMELEMENTS_ENABLE_WYVRN "Build the Razer WYVRN integration" OFF)

if (STREAMELEMENTS_ENABLE_WYVRN AND NOT WIN32)
	# Not a supported target. Forced off rather than failing, so a macOS
	# build needs no special invocation.
	set(STREAMELEMENTS_ENABLE_WYVRN OFF)
	message(STATUS "obs-streamelements-core: WYVRN is Windows-only; disabled on this platform")
endif()

if (STREAMELEMENTS_ENABLE_WYVRN)
	add_compile_definitions(SE_ENABLE_WYVRN)
endif()

message(STATUS "obs-streamelements-core: Razer WYVRN: ${STREAMELEMENTS_ENABLE_WYVRN}")

if (WIN32)
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
	set(ENABLE_ANGELSCRIPT FALSE)

	add_compile_options(
		"-Wno-deprecated-declarations"
		"-Wno-unused-parameter"
		"-Wno-unused-variable"

		"-fvisibility=default"
	)

	# Xcode 26's clang added -Wdeprecated-literal-operator, which fires inside
	# vendored nlohmann/json (deps/cef-stub/../json.hpp) on its `operator ""_json`
	# declarations. Xcode builds with -Werror, so on that toolchain the plugin
	# cannot compile at all -- and deps/ is vendored, so the fix belongs here
	# rather than in the header.
	#
	# Probed rather than added blindly: older clang, including the one on CI's
	# macos-15 runners, does not know this flag, and -Werror turns an unknown
	# -Wno-* into an error of its own.
	include(CheckCXXCompilerFlag)
	check_cxx_compiler_flag("-Wno-deprecated-literal-operator"
		HAVE_WNO_DEPRECATED_LITERAL_OPERATOR)
	if (HAVE_WNO_DEPRECATED_LITERAL_OPERATOR)
		add_compile_options("-Wno-deprecated-literal-operator")
	endif()
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
		MAIN_DEPENDENCY /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/mach/mach_exc.defs
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/streamelements
		COMMAND mig -v /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/mach/mach_exc.defs
	)
endif(APPLE)

# ----------------------------------------------------------------------------

# sentry-native is vendored as the upstream release archive rather than an
# unpacked tree: 1 file instead of 3780, and upgrading is a matter of dropping in
# a new zip. It is extracted into the build directory, not the source tree, so
# nothing generated ever lands in git.
#
# Unlike deps/BugSplat.xcframework.zip this needs no CI unzip step and no README
# instructions -- CMake does it, so a plain configure works everywhere. The
# extraction is guarded on the marker file so it only runs once per build tree.
if (ENABLE_SENTRY)
	# Baked in rather than supplied by CI from a repository secret. A DSN is not
	# a credential: it authorises submitting events and nothing else -- it cannot
	# read them -- and it ships inside every copy of every Sentry client by
	# design. Treating it as a secret would buy nothing and would mean a build
	# without the secret silently reports nowhere.
	#
	# Overridable so a build can be pointed at a different Sentry project. Set it
	# empty and the handler skips sentry_init() entirely, behaving as though the
	# backend were 'none' rather than initialising against a bad DSN.
	set(STREAMELEMENTS_SENTRY_DSN
		"https://7e8322d21ed36c33376671007a2b99f7@o127985.ingest.us.sentry.io/4511887226830848"
		CACHE STRING "Sentry DSN compiled into the plugin")
	add_compile_definitions(SE_SENTRY_DSN="${STREAMELEMENTS_SENTRY_DSN}")

	if (STREAMELEMENTS_SENTRY_DSN STREQUAL "")
		message(WARNING
			"obs-streamelements-core: STREAMELEMENTS_CRASH_HANDLER=sentry but STREAMELEMENTS_SENTRY_DSN is empty; crash reporting will be inert")
	endif()

	set(SENTRY_ARCHIVE "${CMAKE_CURRENT_SOURCE_DIR}/deps/sentry-native.zip")
	set(SENTRY_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/sentry-native")

	if (NOT EXISTS "${SENTRY_SOURCE_DIR}/CMakeLists.txt")
		message(STATUS "obs-streamelements-core: extracting ${SENTRY_ARCHIVE}")
		file(MAKE_DIRECTORY "${SENTRY_SOURCE_DIR}")
		file(ARCHIVE_EXTRACT
			INPUT "${SENTRY_ARCHIVE}"
			DESTINATION "${SENTRY_SOURCE_DIR}")
	endif()

	# Raise how long the crashing process waits for the crash daemon, 10s -> 60s.
	#
	# sentry-native hard-codes SENTRY_CRASH_HANDLER_WAIT_TIMEOUT_MS as a
	# compile-time #define with no sentry_options_* equivalent, so patching the
	# source is the only way to reach it. At the stock 10s our exception filter
	# gives up while the daemon is still uploading and returns
	# EXCEPTION_CONTINUE_SEARCH; OBS's filter then terminates the process out
	# from under the in-flight request. Observed as a complete 1MB minidump left
	# on disk next to `WinHttpSendRequest failed with code 12017`
	# (ERROR_WINHTTP_OPERATION_CANCELLED) in the daemon log, and nothing at all
	# arriving in Sentry. See CORE-554.
	#
	# sentry_options_set_shutdown_timeout() does not cover this. It bounds how
	# long the daemon waits for its own transport during teardown, which is moot
	# once the process it belongs to has already been killed.
	#
	# Patched here rather than inside deps/sentry-native.zip so the divergence
	# from upstream is visible in the build definition, and re-applied on every
	# configure because the extract above is skipped once the tree exists.
	set(SENTRY_WAIT_HEADER
		"${SENTRY_SOURCE_DIR}/src/backends/native/sentry_crash_context.h")
	set(SENTRY_WAIT_STOCK "10000 // 10s max wait for daemon")
	set(SENTRY_WAIT_PATCHED "60000 // 60s max wait for daemon -- CORE-554")

	file(READ "${SENTRY_WAIT_HEADER}" SENTRY_WAIT_CONTENT)

	string(FIND "${SENTRY_WAIT_CONTENT}" "${SENTRY_WAIT_STOCK}"
		SENTRY_WAIT_STOCK_POS)
	string(FIND "${SENTRY_WAIT_CONTENT}" "${SENTRY_WAIT_PATCHED}"
		SENTRY_WAIT_PATCHED_POS)

	if (NOT SENTRY_WAIT_STOCK_POS EQUAL -1)
		string(REPLACE
			"${SENTRY_WAIT_STOCK}" "${SENTRY_WAIT_PATCHED}"
			SENTRY_WAIT_CONTENT "${SENTRY_WAIT_CONTENT}")
		file(WRITE "${SENTRY_WAIT_HEADER}" "${SENTRY_WAIT_CONTENT}")
		message(STATUS
			"obs-streamelements-core: patched SENTRY_CRASH_HANDLER_WAIT_TIMEOUT_MS to 60s")
	elseif (SENTRY_WAIT_PATCHED_POS EQUAL -1)
		# Neither form present: the vendored SDK moved underneath us. Failing
		# here is deliberate -- a silently skipped patch reintroduces a bug
		# whose only symptom is that crash reports quietly never arrive.
		message(FATAL_ERROR
			"obs-streamelements-core: could not patch SENTRY_CRASH_HANDLER_WAIT_TIMEOUT_MS in ${SENTRY_WAIT_HEADER} -- vendored sentry-native changed. Re-check the crash upload wait against CORE-554 before dropping this.")
	endif()

	# Stop the crash daemon inheriting every handle OBS happens to have open.
	#
	# sentry-native spawns sentry-crash.exe with CreateProcessW(...,
	# bInheritHandles = TRUE, ...), commented upstream as "Inherit handles
	# (for event_handle)". That comment is wrong, and the flag is the bug.
	#
	# The daemon does not need a single inherited handle. It attaches to the
	# IPC objects by name -- OpenFileMappingW(shm_name),
	# OpenEventW(event_name), OpenEventW(ready_event_name) in
	# sentry_crash_ipc.c -- and the events are created with a NULL
	# SECURITY_ATTRIBUTES, i.e. non-inheritable, so the numeric handle values
	# passed on its command line were never valid in the child anyway.
	#
	# What TRUE did do was hand the daemon every *other* inheritable handle in
	# OBS. A frozen OBS 32.2.2 was traced to sentry-crash.exe holding the write
	# end of win-capture's anonymous pipe: the get-graphics-offsets helper had
	# exited, but with the daemon still holding the pipe open libobs never saw
	# EOF, so load_graphics_offsets blocked in ReadFile forever, init_hooks
	# never finished, and game_capture_create waited on it with an INFINITE
	# timeout. Killing the daemon released the pipe and OBS resumed. The same
	# daemon also held OBS's log file, obs-browser's debug log and mojo pipes,
	# and three sockets.
	#
	# PROC_THREAD_ATTRIBUTE_HANDLE_LIST was tried first and is wrong here: a
	# handle list may only contain inheritable handles, so adding the two
	# events made CreateProcessW fail with ERROR_INVALID_PARAMETER and the
	# daemon stopped spawning entirely. FALSE is both simpler and correct.
	set(SENTRY_SPAWN_SOURCE
		"${SENTRY_SOURCE_DIR}/src/backends/native/sentry_crash_daemon.c")

	set(SENTRY_SPAWN_STOCK [==[            TRUE, // Inherit handles (for event_handle)]==])

	set(SENTRY_SPAWN_PATCHED [==[            FALSE, // SE.Live: inherit nothing -- the daemon opens its IPC by name. See CMakeLists.]==])

	file(READ "${SENTRY_SPAWN_SOURCE}" SENTRY_SPAWN_CONTENT)

	string(FIND "${SENTRY_SPAWN_CONTENT}" "${SENTRY_SPAWN_STOCK}"
		SENTRY_SPAWN_STOCK_POS)
	string(FIND "${SENTRY_SPAWN_CONTENT}" "${SENTRY_SPAWN_PATCHED}"
		SENTRY_SPAWN_PATCHED_POS)

	if (NOT SENTRY_SPAWN_STOCK_POS EQUAL -1)
		string(REPLACE "${SENTRY_SPAWN_STOCK}" "${SENTRY_SPAWN_PATCHED}"
			SENTRY_SPAWN_CONTENT "${SENTRY_SPAWN_CONTENT}")
		file(WRITE "${SENTRY_SPAWN_SOURCE}" "${SENTRY_SPAWN_CONTENT}")
		message(STATUS
			"obs-streamelements-core: patched sentry-crash spawn to inherit no handles")
	elseif (SENTRY_SPAWN_PATCHED_POS EQUAL -1)
		# As with the wait timeout above: a silently skipped patch reintroduces a
		# bug whose only symptom is an unrelated OBS subsystem deadlocking.
		message(FATAL_ERROR
			"obs-streamelements-core: could not patch daemon handle inheritance in ${SENTRY_SPAWN_SOURCE} -- vendored sentry-native changed. Re-check CreateProcessW bInheritHandles before dropping this.")
	endif()

	# The `native` backend, not crashpad: it hands control back to WER rather than
	# swallowing the host's crash handling, its hooks work on macOS, and it is the
	# only backend offering sentry_options_set_minidump_flags. See CORE-554.
	set(SENTRY_BACKEND "native" CACHE STRING "" FORCE)
	set(SENTRY_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
	set(SENTRY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(SENTRY_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(SENTRY_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

	if (MSVC)
		# Explicitly OFF, and not merely left at its default.
		#
		# The string(REPLACE "/MD" "/MT" ...) block further down this file looks
		# like it makes this project static-runtime, which would call for
		# SENTRY_BUILD_RUNTIMESTATIC=ON. It does not: obs-studio sets the runtime
		# through the MSVC_RUNTIME_LIBRARY target property, so CMAKE_CXX_FLAGS
		# never literally contains /MD and there is nothing for that replace to
		# match. The generated obs-streamelements-core.vcxproj says
		# MultiThreadedDLL, same as libobs.
		#
		# Turning this ON builds sentry /MT against a /MD host and the link fails
		# with LNK4098 (defaultlib 'LIBCMT' conflicts), which /WX promotes to
		# LNK1218. Match the host instead.
		set(SENTRY_BUILD_RUNTIMESTATIC OFF CACHE BOOL "" FORCE)
	endif()

	if (WIN32)
		# Syncs tags and attachments into Windows Error Reporting, so fast-fail
		# crashes -- which bypass SEH, and therefore bypass our exception filter
		# entirely -- still carry what was armed on the scope.
		set(SENTRY_INTEGRATION_WER ON CACHE BOOL "" FORCE)
	endif()

	add_subdirectory("${SENTRY_SOURCE_DIR}" sentry_build EXCLUDE_FROM_ALL)

	# obs-studio's dependency bundler walks the whole link graph to decide which
	# DLLs to copy, and _handle_generator_expression_dependency
	# (cmake/common/helpers_common.cmake:146) rejects $<LINK_ONLY:...> outright:
	#
	#   $<LINK_ONLY:Threads::Threads> is an unsupported generator expression for
	#   linked libraries.
	#
	# CMake generates that wrapper itself for a static library's PRIVATE
	# dependencies, so sentry is not doing anything unusual -- obs's parser
	# simply does not handle the form. Unwrapping it keeps Threads::Threads
	# linked while leaving the bundler something it can read.
	foreach(sentry_target sentry sentry_crashpad)
		if (TARGET ${sentry_target})
			get_target_property(sentry_iface_libs ${sentry_target} INTERFACE_LINK_LIBRARIES)
			if (sentry_iface_libs)
				string(REGEX REPLACE "\\$<LINK_ONLY:([^>]+)>" "\\1"
					sentry_iface_libs "${sentry_iface_libs}")
				set_target_properties(${sentry_target} PROPERTIES
					INTERFACE_LINK_LIBRARIES "${sentry_iface_libs}")
			endif()
		endif()
	endforeach()

	# The gating WER runtime exception module (CORE-864).
	#
	# Heap corruption and every __fastfail bypass SEH, so neither our filter
	# nor OBS's ever sees them; a WER runtime exception module, running out of
	# process inside WerFault.exe, is the only thing that can capture them.
	#
	# sentry builds one -- sentry-wer.dll, from the target above -- but it
	# claims every such crash in the host process, including OBS's own and
	# every other plugin's. Registering it directly would undo the
	# module-of-interest gate for one crash class. So we build our own, which
	# applies the gate and the standing-consent check and then forwards to
	# sentry's, and we register ours instead.
	#
	# Its own target because it is loaded into WerFault.exe, not into OBS: it
	# must link neither libobs nor Qt nor sentry. Both DLLs are copied beside
	# the plugin below, and sentry's is deliberately NOT placed next to
	# obs64.exe, which is the only place sentry's own registration looks --
	# that is what keeps ours the only module registered.
	if (WIN32)
		add_library(se-crash-wer SHARED
			streamelements/StreamElementsWerModule.cpp
			streamelements/StreamElementsWerModule.def
		)

		# No "lib" prefix and no debug postfix: WerFault loads it by the
		# exact path we register, and the .def names the DLL.
		set_property(TARGET se-crash-wer PROPERTY PREFIX "")
		set_property(TARGET se-crash-wer PROPERTY DEBUG_POSTFIX "")

		# Qt's autogen is on for everything in this directory. This target
		# has no Qt in it and must not acquire any -- it is loaded into
		# WerFault.exe, not into OBS -- so the moc/uic/rcc passes are turned
		# off rather than left to produce an empty translation unit.
		set_property(TARGET se-crash-wer PROPERTY AUTOMOC OFF)
		set_property(TARGET se-crash-wer PROPERTY AUTOUIC OFF)
		set_property(TARGET se-crash-wer PROPERTY AUTORCC OFF)

		# /MD, matching libobs and the plugin. See the note on
		# MSVC_RUNTIME_LIBRARY at the top of this file.
		set_property(TARGET se-crash-wer PROPERTY MSVC_RUNTIME_LIBRARY
			"MultiThreadedDLL")

		target_include_directories(se-crash-wer PRIVATE
			"${CMAKE_CURRENT_SOURCE_DIR}/streamelements")

		target_link_libraries(se-crash-wer PRIVATE dbghelp psapi)
	endif()
endif()

# WIN32 is part of the condition now that ENABLE_BUGSPLAT tracks the selected
# backend rather than the platform: this block finds the Windows .lib/.dll SDK,
# while macOS links the xcframework further down.
if (WIN32 AND ENABLE_BUGSPLAT)
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

endif()

# The base link set is a platform question; the crash backend only appends to it.
# These used to be tangled: the BugSplat branch was also what added w32-pthreads
# on Windows, so selecting any other backend silently dropped it from the link.
set(obs-streamelements-core_LIBRARIES
	libobs
	obs-frontend-api
	#Qt::Core
	Qt::Widgets
)

if (WIN32)
	list(APPEND obs-streamelements-core_LIBRARIES w32-pthreads)
endif()

if (WIN32 AND ENABLE_BUGSPLAT)
	list(APPEND obs-streamelements-core_LIBRARIES ${BUGSPLAT_LIBRARIES})
endif()

if (ENABLE_SENTRY)
	list(APPEND obs-streamelements-core_LIBRARIES sentry::sentry)
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
	streamelements/StreamElementsSecretRedactor.cpp
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
	streamelements/StreamElementsSharedVideoCompositionManager.cpp
	streamelements/StreamElementsOutput.cpp
	streamelements/StreamElementsOutputManager.cpp
	streamelements/StreamElementsObsActiveSceneTracker.cpp
	streamelements/SETrace.cpp
	streamelements/updater/confirmpendingupdatedialog.cpp
	streamelements/updater/networkdialog.cpp
	streamelements/updater/updater.cpp
	streamelements/deps/StackWalker/StackWalker.cpp
	streamelements/deps/zip/zip.c
	streamelements/deps/server/NamedPipesServer.cpp
	streamelements/deps/server/NamedPipesServerClientHandler.cpp
	streamelements/deps/server/HttpServer.cpp
	#streamelements/deps/sqlite/sqlite3.c
	)

if (APPLE)
list(APPEND obs-streamelements-core_SOURCES
        streamelements/StreamElementsUtils.mm
    )

	# The mig-generated Mach exception server belongs to the BugSplat handler,
	# not to the platform: the catch_mach_exception_* callbacks it dispatches
	# to are defined in StreamElementsBugSplatCrashHandler.mm and nowhere
	# else. Compiled unconditionally they leave those symbols undefined in any
	# macOS build that selects another backend, which is why this only
	# surfaced once one was built.
	if (ENABLE_BUGSPLAT)
		list(APPEND obs-streamelements-core_SOURCES
			streamelements/mach_excServer.c
			streamelements/mach_excUser.c
		)
	endif()
endif(APPLE)

# The factory is platform-agnostic and always compiles; the selected backend
# contributes its own platform-specific implementation. A 'none' build gets the
# factory alone, which returns nullptr.
list(APPEND obs-streamelements-core_SOURCES
	streamelements/StreamElementsCrashHandler.cpp
)

# Backend-agnostic crash-time collection, shared by whichever backend is
# selected and by both platforms. The collection itself is portable -- the same
# zip, the same system and performance snapshots, the same async- and API-call
# context; only the stack walk, the temporary-file dance and the window capture
# fork, and those are #ifdef'd inside.
if (ENABLE_BUGSPLAT OR ENABLE_SENTRY)
	list(APPEND obs-streamelements-core_SOURCES
		streamelements/StreamElementsCrashContext.cpp
	)
endif()

if (ENABLE_BUGSPLAT)
	if (APPLE)
		list(APPEND obs-streamelements-core_SOURCES
			streamelements/StreamElementsBugSplatCrashHandler.mm
		)
	else()
		list(APPEND obs-streamelements-core_SOURCES
			streamelements/StreamElementsBugSplatCrashHandler.cpp
		)
	endif(APPLE)
endif()

if (ENABLE_SENTRY)
	# One handler per platform. They share the header and the overall shape --
	# arm, hand off, ask -- but not the mechanism: Windows chains SEH filters,
	# macOS chains signal handlers and leans on sentry's own consent gate,
	# because sentry's macOS handler chains to its predecessor and its Windows
	# one does not. See StreamElementsSentryCrashHandler.hpp.
	if (APPLE)
		list(APPEND obs-streamelements-core_SOURCES
			streamelements/StreamElementsSentryCrashHandler.mm
		)
	else()
		list(APPEND obs-streamelements-core_SOURCES
			streamelements/StreamElementsSentryCrashHandler.cpp
		)
	endif()

	# The crash-time consent prompt. Only Sentry needs it: BugSplat's SDK
	# shows a prompt of its own, whereas Sentry uploads silently.
	if (WIN32)
		list(APPEND obs-streamelements-core_SOURCES
			streamelements/StreamElementsCrashConsentDialog.cpp
		)
	elseif (APPLE)
		list(APPEND obs-streamelements-core_SOURCES
			streamelements/StreamElementsCrashConsentDialog.mm
		)
	endif()
endif()

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
	streamelements/StreamElementsSecretRedactor.hpp
	streamelements/StreamElementsProgressDialog.hpp
	streamelements/StreamElementsPerformanceHistoryTracker.hpp
	streamelements/StreamElementsNetworkDialog.hpp
	streamelements/StreamElementsAnalyticsEventsManager.hpp
	streamelements/StreamElementsCrashHandler.hpp
	streamelements/StreamElementsCrashContext.hpp
	streamelements/StreamElementsCrashConsentDialog.hpp
	streamelements/StreamElementsBugSplatCrashHandler.hpp
	streamelements/StreamElementsSentryCrashHandler.hpp
	streamelements/StreamElementsWerRegistration.h
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
	streamelements/StreamElementsSharedVideoCompositionManager.hpp
	streamelements/StreamElementsOutput.hpp
	streamelements/StreamElementsOutputManager.hpp
	streamelements/StreamElementsObsActiveSceneTracker.hpp
	streamelements/canvas-config.hpp
	streamelements/canvas-scan.hpp
	streamelements/canvas-math.hpp
	streamelements/canvas-draw.hpp
	streamelements/canvas-mutate.hpp
	streamelements/canvas-controls.hpp
	streamelements/SETrace.hpp
	streamelements/updater/confirmpendingupdatedialog.hpp
	streamelements/updater/confirmpendingupdatedialog.ui
	streamelements/updater/networkdialog.hpp
	streamelements/updater/networkdialog.ui
	streamelements/updater/updater.hpp
	streamelements/deps/StackWalker/StackWalker.h
	streamelements/deps/zip/zip.h
	streamelements/deps/zip/miniz.h
	streamelements/deps/moodycamel/concurrentqueue.h
	streamelements/deps/moodycamel/blockingconcurrentqueue.h
	streamelements/deps/server/NamedPipesServer.hpp
	streamelements/deps/server/NamedPipesServerClientHandler.hpp
	streamelements/deps/server/HttpServer.hpp
	#streamelements/deps/sqlite/sqlite3.h
	#streamelements/deps/sqlite/sqlite3ext.h
	streamelements/deps/cpp-httplib/httplib.h
	streamelements/streamelements.qrc
	)

if (APPLE)
    list(APPEND obs-streamelements-core_HEADERS
		streamelements/mach_exc.h
		macutil.mm
    )
endif()

# The vendored WYVRN SDK. Razer ships it as source rather than a library, and
# it resolves RzChromatic64.dll at runtime via LoadLibraryW after checking the
# module is signed by Razer -- so nothing here creates a link-time dependency.
#
# NO_CHECK_WYVRNSDK_LIBRARY_SIGNATURE is deliberately NOT defined: it would
# skip that Authenticode check on a DLL loaded into the OBS process. A source
# invariant in tests/ asserts it stays absent.
if (STREAMELEMENTS_ENABLE_WYVRN)
	list(APPEND obs-streamelements-core_SOURCES
		streamelements/StreamElementsRazerWyvrnConfig.cpp
		streamelements/StreamElementsRazerWyvrnManager.cpp
		deps/wyvrn-sdk/WyvrnAPI.cpp
		deps/wyvrn-sdk/UnicodeWyvrnAPI.cpp
		deps/wyvrn-sdk/VerifyLibrarySignature.cpp
		deps/wyvrn-sdk/WyvrnLogger.cpp)

	list(APPEND obs-streamelements-core_HEADERS
		streamelements/StreamElementsRazerWyvrnConfig.hpp
		streamelements/StreamElementsRazerWyvrnManager.hpp
		deps/wyvrn-sdk/WyvrnAPI.h
		deps/wyvrn-sdk/UnicodeWyvrnAPI.h
		deps/wyvrn-sdk/VerifyLibrarySignature.h
		deps/wyvrn-sdk/WyvrnLogger.h
		deps/wyvrn-sdk/WyvrnErrors.h
		deps/wyvrn-sdk/WyvrnSDKTypes.h)
endif()

if (APPLE)
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/macos")
endif()

add_library(obs-streamelements-core MODULE
	${obs-streamelements-core_SOURCES}
	${obs-streamelements-core_HEADERS}
	${obs-streamelements-core-streamelements_QRC_SOURCES}
)

if (APPLE)
  # generate debug symbols in a .dsym file for release mode
  set(CMAKE_XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS[variant=RelWithDebInfo] "YES")
  set(CMAKE_XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS[variant=Release] "YES")
  set(CMAKE_XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS[variant=Debug] "YES")
  set(CMAKE_XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS[variant=MinSizeRel] "YES")

  set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT[variant=RelWithDebInfo] "dwarf-with-dsym")
  set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT[variant=Release] "dwarf-with-dsym")
  set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT[variant=Debug] "dwarf-with-dsym")
  set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT[variant=MinSizeRel] "dwarf-with-dsym")
  # strip symbols from final executable
  set(CMAKE_XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING[variant=RelWithDebInfo] "NO")
  set(CMAKE_XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING[variant=Release] "NO")
  set(CMAKE_XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING[variant=Debug] "NO")
  set(CMAKE_XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING[variant=MinSizeRel] "NO")

  target_compile_options(obs-streamelements-core PRIVATE
    "$<$<CXX_COMPILER_ID:Clang>:-g>"
  )

  # Applied to the target rather than through add_compile_options above: the
  # directory-level options do not survive onto this target under the Xcode
  # generator, so the suppression has to be attached here to take effect.
  if (HAVE_WNO_DEPRECATED_LITERAL_OPERATOR)
    target_compile_options(obs-streamelements-core PRIVATE
      "-Wno-deprecated-literal-operator"
    )
  endif()
endif()

if (APPLE)
	list(APPEND obs-streamelements-core_LIBRARIES
		"-framework AppKit"
		"-framework IOKit"
		"-framework Foundation"
		"-framework System"
        "-framework SystemConfiguration"
		# Core Animation, for the crash progress window. Its animation is
		# committed to the render server, so it keeps running while the
		# crashing thread is blocked collecting the report -- an AppKit
		# animation driven by the main run loop would freeze instead.
		"-framework QuartzCore"
	)

	find_library(COREFOUNDATION CoreFoundation)
	list(APPEND obs-streamelements-core_LIBRARIES
			${COREFOUNDATION})

	# Hoisted out of the BugSplat branch below: the bundle needs its Info.plist
	# and an rpath for embedded frameworks whichever crash backend is selected,
	# including none at all.
	set (MACOS_INFO_PLIST_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/MacOSXBundleInfo.plist.in")

	message("MACOS_INFO_PLIST_TEMPLATE: ${MACOS_INFO_PLIST_TEMPLATE}")

	set_target_properties(obs-streamelements-core PROPERTIES
		XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@loader_path/../Frameworks"
		MACOSX_BUNDLE_INFO_PLIST "${MACOS_INFO_PLIST_TEMPLATE}"
	)

	if (ENABLE_BUGSPLAT)
		message("--------------------------------------------------")
		message("Looking for BugSplat framework here: ${CMAKE_CURRENT_SOURCE_DIR}/deps")

		find_library(BUGSPLATMAC_FRAMEWORK
			NAMES BugSplat
			PATHS "${CMAKE_CURRENT_SOURCE_DIR}/deps"
			PATH_SUFFIXES framework xcframework
			NO_DEFAULT_PATH
			REQUIRED
		)

		message("BugSplat: ${BUGSPLATMAC_FRAMEWORK}")

		if (${BUGSPLATMAC_FRAMEWORK} STREQUAL BUGSPLAT_FRAMEWORK-NOTFOUND)
			message(FATAL_ERROR "BugSplat framework not found")
		else()
			message("BugSplat framework found: ${BUGSPLATMAC_FRAMEWORK}")

			list(APPEND obs-streamelements-code_LIBRARIES "$<LINK_LIBRARY:FRAMEWORK,${BUGSPLATMAC_FRAMEWORK}>")

			set_source_files_properties(streamelements/StreamElementsBugSplatCrashHandler.mm PROPERTIES COMPILE_FLAGS "-fmodules -fcxx-modules")

			set_target_properties(obs-streamelements-core PROPERTIES
				XCODE_EMBED_FRAMEWORKS "${BUGSPLATMAC_FRAMEWORK}"
			)
		endif()
		message("--------------------------------------------------")
	endif()
endif()

if (${QT_VERSION} EQUAL "6")
	qt6_add_resources(obs-streamelements-core "streamelements/streamelements.qrc"
		PREFIX "/"
		BASE "streamelements"
		FILES
		"streamelements/images/logo.png"
		"streamelements/images/updater_logo.png"
		"streamelements/images/updater_logo_dark.png"
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
	#set_target_properties(obs-streamelements-core PROPERTIES
	#	COMPILE_FLAGS "-mmacosx-version-min=10.15 -target x86_64-apple-macos10.15")

	set_property(TARGET obs-streamelements-core PROPERTY ENABLE_EXPORTS 1)
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

	if(ENABLE_SENTRY)
		# The sentry-crash daemon writes and uploads the minidump out of
		# process, so it has to ship with the plugin or the Sentry build
		# reports events with no dump attached.
		#
		# It goes beside our own binary rather than beside obs64.exe,
		# where the BugSplat DLLs above go. The SDK's default lookup is
		# next to the host executable, which would let any other plugin
		# shipping a sentry-crash.exe of its own shadow ours -- so the
		# handler passes this exact location to
		# sentry_options_set_handler_pathw() instead of relying on it.
		add_dependencies(obs-streamelements-core sentry-crash)

		# Next to the freshly built DLL, for running out of the build tree.
		add_custom_command(TARGET obs-streamelements-core POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"$<TARGET_FILE:sentry-crash>"
			"$<TARGET_FILE_DIR:obs-streamelements-core>"
		)

		# And into the rundir, which is what CI zips and the installer
		# packages. $<TARGET_FILE_DIR:...> above is the build tree, not the
		# rundir -- obs-studio populates that separately, and it copies the
		# plugin module but knows nothing about a daemon sitting beside it.
		# Without this the daemon never reaches the installer, and a Sentry
		# build reports events with no minidump attached.
		#
		# obs-plugins, not bin: the handler resolves the daemon relative to
		# its own module, which is what lands here.
		add_custom_command(TARGET obs-streamelements-core POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E make_directory
			"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit"
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"$<TARGET_FILE:sentry-crash>"
			"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
		)

		# The two WER module DLLs travel together and land in exactly the
		# same place, for two different reasons (CORE-864).
		#
		# se-crash-wer.dll is ours: the handler registers it by absolute
		# path, resolved relative to its own module, so it can live here
		# rather than beside obs64.exe.
		#
		# sentry-wer.dll is sentry's, and it is here *because* sentry will
		# not look here. wer_default_path() in sentry_backend_native.c
		# joins "sentry-wer.dll" onto the directory of the host executable
		# and registers whatever it finds. Putting it beside obs64.exe
		# would make sentry register it, and sentry's module claims every
		# fast-fail in the process -- OBS's own and every other plugin's --
		# with no gate. Keeping it here means sentry logs "Native WER
		# module not found", registers nothing, and ours stays the only
		# registered module; ours then loads this copy explicitly by the
		# path it was given.
		add_dependencies(obs-streamelements-core se-crash-wer sentry-wer)

		foreach(se_wer_target se-crash-wer sentry-wer)
			add_custom_command(TARGET obs-streamelements-core POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"$<TARGET_FILE:${se_wer_target}>"
				"$<TARGET_FILE_DIR:obs-streamelements-core>"
				COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"$<TARGET_FILE:${se_wer_target}>"
				"${CMAKE_BINARY_DIR}/rundir/$<CONFIGURATION>/obs-plugins/${BITS}bit/"
			)
		endforeach()
	endif()
endif()

if (APPLE AND ENABLE_SENTRY)
	# Same requirement as the Windows copy above -- without the daemon beside
	# the binary, Sentry produces events with no minidump attached -- but the
	# destination is inside the plugin bundle, next to the loadable module in
	# Contents/MacOS. $<TARGET_FILE_DIR:...> already resolves to that
	# directory for a MODULE inside a bundle.
	#
	# It has to be signed as its own executable when the bundle is signed:
	# --deep does not reliably cover nested executables and is deprecated by
	# Apple, so the packaging step signs this explicitly.
	add_dependencies(obs-streamelements-core sentry-crash)

	add_custom_command(TARGET obs-streamelements-core POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_FILE:sentry-crash>"
		"$<TARGET_FILE_DIR:obs-streamelements-core>"
	)
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

# Standalone regression tests. Off by default; they do not link libobs/Qt
# and can be built without an obs-studio tree. See tests/CMakeLists.txt.
option(SE_ENABLE_TESTS "Build standalone regression tests" OFF)
if(SE_ENABLE_TESTS)
	add_subdirectory(tests)
endif()
