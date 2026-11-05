# Regression tests for crash-bug fixes.
#
# These tests are NOT part of the OBS plugin build. They are standalone
# C++17 executables that do not link libobs or Qt. Two kinds of tests live
# here:
#
#   1. Behavioural tests that reproduce a minimal mirror of the buggy code
#      and assert it behaves correctly. (e.g. test_video_encoder_template_demo)
#
#   2. Source-invariant tests that open the real production source files
#      and assert specific buggy patterns are absent. These act as a
#      regression gate without requiring the OBS build to succeed.
#
# Build:
#   cmake -S tests -B tests/build -DENABLE_TESTS=ON
#   cmake --build tests/build
#   ctest --test-dir tests/build --output-on-failure
#
# Or from the project root with ENABLE_TESTS=ON.

cmake_minimum_required(VERSION 3.16)

# Standalone only. Not `if(NOT CMAKE_PROJECT_NAME)`: project() writes that
# name into the cache as a STATIC entry, so on the second configure of the
# same build directory the guard sees it already set, skips project(), and
# leaves CXX unenabled -- every target then fails with "can not determine
# linker language" and ctest silently re-runs the previous binaries.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(obs-streamelements-core-tests CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

# Configure the source-root path into the source-invariant test so it can
# read production files at runtime regardless of where it is built from.
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/source_paths.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/source_paths.hpp"
  @ONLY
)

set(GENERATED_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/generated")

function(se_add_test name)
  add_executable(${name} ${ARGN})
  target_include_directories(${name} PRIVATE
    "${GENERATED_INCLUDE}"
    "${REPO_ROOT}"
    "${REPO_ROOT}/deps"
    # cef-stub's cef_string.cpp reaches for deps/utf8.h, which in turn includes
    # utf8/checked.h - and those live under streamelements/deps, exactly as the
    # plug-in build wires them up (CMakeLists.txt:104).
    "${REPO_ROOT}/streamelements/deps"
  )
  target_compile_features(${name} PRIVATE cxx_std_17)
  if(NOT MSVC)
    target_compile_options(${name} PRIVATE -Wall -Wextra)
  endif()
  add_test(NAME ${name} COMMAND ${name})
endfunction()

# --- Behavioural test: the pure string helpers ------------------------------
#
# The production .cpp, compiled beside the test. Possible only since the
# SEUtils split: this cluster has a header of its own now, where SEUtils.hpp
# pulls in obs.h, obs-module.h, curl and eight Qt headers and so cannot be
# included from here at all.
se_add_test(test_string_utils
  test_string_utils.cpp
  "${REPO_ROOT}/streamelements/SEUtils/SEStringUtils.cpp"
)

# --- Behavioural test: the session message signature scheme ---------------
#
# The digest SELocalFilesystemHttpServer checks before it opens and serves a
# file, so it is the whole of that server's access control rather than a
# checksum. The production .cpp is compiled here;
# CreateCryptoSecureRandomNumberString() is not -- its two bodies need the
# Windows SDK or Objective-C -- so each test below links its own, which is
# also what makes the salt deterministic enough to assert against.
se_add_test(test_session_signature
  test_session_signature.cpp
  "${REPO_ROOT}/streamelements/SEUtils/SECryptoUtils.cpp"
)

# The same production .cpp against a random source that returns nothing. A
# second executable rather than a second test case, because the salt is a
# function-local static: one process gets one salt, so the no-entropy world
# has to be a process of its own.
se_add_test(test_session_signature_no_entropy
  test_session_signature_no_entropy.cpp
  "${REPO_ROOT}/streamelements/SEUtils/SECryptoUtils.cpp"
)

# --- Behavioural test: the StreamElements overlay URL scheme --------------
#
# One regex and one concatenation, so nothing but the production .cpp is
# linked. Neither helper has a caller yet; the tests record what the
# pattern actually accepts, which is looser than it reads.
se_add_test(test_overlay_url_utils
  test_overlay_url_utils.cpp
  "${REPO_ROOT}/streamelements/SEUtils/SEOverlayUrlUtils.cpp"
)

# --- Behavioural test: the libcurl wrappers --------------------------------
#
# The one test here that is not self-contained: it links libcurl and runs a
# cpp-httplib server on a loopback port. Registered only when curl is found,
# so the rest of the suite keeps the property that it builds anywhere with
# nothing but a compiler -- a missing curl skips this one test rather than
# failing the configure.
#
# Compiling SEHttpUtils.cpp here at all is new: it used to include
# SEAppEnvUtils.hpp for the one --streamelements-http-proxy lookup, and that
# header pulls in <QWidget>. The declaration moved to SEEnvConfigUtils.hpp,
# which is why nothing below needs Qt.
find_package(CURL QUIET)

if(NOT CURL_FOUND)
  # The obs-studio dependency bundle, if this checkout is sitting inside
  # one: plugins/obs-streamelements-core, so the tree root two levels up
  # is where .deps lives.
  #
  # The architecture has to be part of the pattern. The bundles for both
  # sit side by side, and an x86 import library would be found first by
  # date and then fail to link against an x64 test.
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_se_dep_arch "x64")
  else()
    set(_se_dep_arch "x86")
  endif()

  file(GLOB _se_obs_deps
    "${REPO_ROOT}/../../.deps/obs-deps-*-${_se_dep_arch}")

  # Newest bundle first, since the names carry their date.
  list(SORT _se_obs_deps)
  list(REVERSE _se_obs_deps)

  foreach(_se_dep IN LISTS _se_obs_deps)
    if(EXISTS "${_se_dep}/include/curl/curl.h")
      file(GLOB _se_curl_lib "${_se_dep}/lib/libcurl_imp.lib"
                             "${_se_dep}/lib/libcurl.lib"
                             "${_se_dep}/lib/libcurl.a"
                             "${_se_dep}/lib/libcurl.dylib")

      if(_se_curl_lib)
        set(CURL_FOUND TRUE)
        set(CURL_INCLUDE_DIRS "${_se_dep}/include")
        set(CURL_LIBRARIES "${_se_curl_lib}")

        # An import library needs its DLL beside the executable, or the
        # test fails to start rather than failing an assertion.
        file(GLOB _se_curl_runtime "${_se_dep}/bin/libcurl.dll")

        break()
      endif()
    endif()
  endforeach()
endif()

if(CURL_FOUND)
  se_add_test(test_http_utils
    test_http_utils.cpp
    "${REPO_ROOT}/streamelements/SEUtils/SEHttpUtils.cpp"
    "${REPO_ROOT}/streamelements/SEUtils/SEStringUtils.cpp"
  )

  target_include_directories(test_http_utils PRIVATE "${CURL_INCLUDE_DIRS}")
  target_link_libraries(test_http_utils PRIVATE "${CURL_LIBRARIES}")

  if(WIN32)
    # Winhttp for the proxy lookup SetGlobalCURLOptions does, ws2_32 for the
    # server's sockets. SEHttpUtils.cpp carries its own #pragma comment for
    # Winhttp, but naming it here keeps the dependency visible.
    target_link_libraries(test_http_utils PRIVATE winhttp ws2_32)
  endif()

  if(_se_curl_runtime)
    add_custom_command(TARGET test_http_utils POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "${_se_curl_runtime}"
              "$<TARGET_FILE_DIR:test_http_utils>"
      COMMENT "Copying libcurl beside test_http_utils")
  endif()
else()
  message(STATUS
    "curl not found: skipping test_http_utils. The rest of the suite does "
    "not need it.")
endif()

# --- Behavioural test: the WYVRN configuration parser -----------------------
#
# Compiles the real production parser, not a mirror of it. That is possible
# because SERazerWyvrnConfig.cpp deliberately depends on nothing but
# the standard library and the local CefValue model - no libobs, no Qt - so the
# thing under test here is the code that actually ships.
se_add_test(test_razer_wyvrn_config
  test_razer_wyvrn_config.cpp
  "${REPO_ROOT}/streamelements/SERazerWyvrnConfig.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_string.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_value.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_value_binary.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_value_dictionary.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_value_json.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_value_list.cpp"
)

# --- Behavioural test: the API handler infrastructure ---------------------
#
# The registry, the dispatcher, the API context stack and the renderer
# handshake that builds window.host -- the first coverage any of it has had.
#
# Possible because SEApiMessageHandlerBase.cpp's include closure is 18 local
# headers and nothing external but the standard library: the 208 host API
# methods are in SEApiMessageHandler.cpp, which still pulls in obs.h and 33
# Qt headers through the services its bodies dereference.
#
# Four seams are linked here rather than compiled: IsObsInitialized,
# IsTraceLogLevel, SELog/SELogVa and __QtPostTask_Impl, each of whose real
# body needs libobs or Qt. SEDiagnosticsUtils.cpp is compiled as it ships, so
# the API-context stack under test is the one the crash handler reads.
set(SE_API_HANDLER_SOURCES
  api_handler_harness.cpp
  "${REPO_ROOT}/streamelements/SEApiMessageHandlerBase.cpp"
  "${REPO_ROOT}/streamelements/SEUtils/SEDiagnosticsUtils.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_string.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_process_message.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_value.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_value_binary.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_value_dictionary.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_value_json.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_value_list.cpp"
)

se_add_test(test_api_handler_infrastructure
  test_api_handler_infrastructure.cpp
  ${SE_API_HANDLER_SOURCES}
)

# The crash gate needs a process of its own: SetCrashReportingInProgress() is
# one-way by design, so one process gets one answer.
se_add_test(test_api_handler_crash_gate
  test_api_handler_crash_gate.cpp
  ${SE_API_HANDLER_SOURCES}
)

# --- Behavioural test: the three host API methods that close a dialog ------
#
# endModalDialog / endNonModalDialog / endDialog were three copies of one body
# inside SEBrowserDialog.cpp, a QDialog translation unit neither ratchet reads
# and nothing here could compile. The body is EndDialogFromApiCall now, which
# names no Qt type, so the dialog under it is a recording stand-in.
#
# They are the only way a modal SE.Live dialog ever closes, which is what
# makes an untested one worth this much attention.
se_add_test(test_dialog_api_call
  test_dialog_api_call.cpp
  "${REPO_ROOT}/streamelements/SEDialogApiCall.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_string.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_value.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_value_binary.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_value_dictionary.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_value_json.cpp"
  "${REPO_ROOT}/deps/cef-stub/cef_value_list.cpp"
)

# --- Behavioural test: C2 (VideoEncoderTemplate::IsMatchingIndex = vs ==) ---
se_add_test(test_video_encoder_template_demo
  test_video_encoder_template_demo.cpp)

# --- Behavioural test: CORE-860 (the module-of-interest gate must not be
#     rubber-stamped by the crash handler's own frames on the abort path) ---
se_add_test(test_crash_gate_leading_frames
  test_crash_gate_leading_frames.cpp)

# --- Behavioural test: CORE-862 (queued Qt tasks must not run once crash
#     reporting has begun, and a dropped task must still release its waiter) ---
se_add_test(test_crash_suppresses_queued_tasks
  test_crash_suppresses_queued_tasks.cpp)

# --- Behavioural test: CORE-863 (the OOM handler must observe and chain,
#     not seize the host's allocation-failure policy) ---
se_add_test(test_crash_oom_handler
  test_crash_oom_handler.cpp)

# --- Behavioural test: CORE-865 (the lazy encoder provider must acquire and
#     release its encoder once per OBJECT, not once per consumer) ---
se_add_test(test_lazy_encoder_refcount
  test_lazy_encoder_refcount.cpp)

# --- The WER runtime exception module gate: heap corruption and fast-fail
#     never reach the in-process filter, so this is the only gate they get. ---
se_add_test(test_wer_gate
  test_wer_gate.cpp)

# --- Source-invariant test: catches re-introduction of any of the
#     bug patterns this PR fixes. ---
se_add_test(test_source_invariants
  test_source_invariants.cpp)

# --- Behavioural test: the 208 host API method bodies ----------------------
#
# The other half of SEApiMessageHandler: test_api_handler_infrastructure
# compiles the base, this compiles the file with the 208 API_HANDLER_BEGIN
# blocks in it. That became possible only once part 2 of CORE-757 took that
# translation unit to zero obs and zero Qt headers in its include closure.
#
# Compiling was not the whole price. The bodies CALL their services, so the
# linker wants a definition for each: fake_api_services.cpp derives a fake
# from each of the sixteen ISE* interfaces, and stub_concrete_services.cpp
# supplies the 51 methods plus 11 free functions belonging to the fifteen
# services the container still carries concretely. Both record into one
# journal the tests program and read.
# The same translation unit again, with both optional features ON -- the only
# coverage the seven guarded handler bodies can have, since the shipping value
# of both switches compiles them out. A target of its own because the switches
# change what the file contains, which one binary cannot hold twice.
#
# SE_ENABLE_WYVRN brings SERazerWyvrnManager onto the container, so that one
# needs a link seam of its own.
se_add_test(test_api_handler_optional
  test_api_handler_optional.cpp
  fake_api_services.cpp
  stub_concrete_services.cpp
  stub_razer_wyvrn.cpp
  ${SE_API_HANDLER_SOURCES}
  "${REPO_ROOT}/streamelements/SEApiMessageHandler.cpp"
)
target_compile_definitions(test_api_handler_optional PRIVATE
  SE_ENABLE_WYVRN
  SE_ENABLE_CENTRAL_WIDGET_DECORATIONS=1
)

# test_api_handler_contract.cpp is part of the SAME executable rather than
# a target of its own: compiling SEApiMessageHandler.cpp is most of this
# target's build time, and the two files are two kinds of test over it --
# narrative cases and the contract table -- not two things to build.
se_add_test(test_api_handler_bodies
  test_api_handler_bodies.cpp
  test_api_handler_contract.cpp
  fake_api_services.cpp
  stub_concrete_services.cpp
  ${SE_API_HANDLER_SOURCES}
  "${REPO_ROOT}/streamelements/SEApiMessageHandler.cpp"
)
