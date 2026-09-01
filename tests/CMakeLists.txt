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

if(NOT CMAKE_PROJECT_NAME)
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

# --- Behavioural test: the WYVRN configuration parser -----------------------
#
# Compiles the real production parser, not a mirror of it. That is possible
# because StreamElementsRazerWyvrnConfig.cpp deliberately depends on nothing but
# the standard library and the local CefValue model - no libobs, no Qt - so the
# thing under test here is the code that actually ships.
se_add_test(test_razer_wyvrn_config
  test_razer_wyvrn_config.cpp
  "${REPO_ROOT}/streamelements/StreamElementsRazerWyvrnConfig.cpp"
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
