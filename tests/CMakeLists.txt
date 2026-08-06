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
  )
  target_compile_features(${name} PRIVATE cxx_std_17)
  if(NOT MSVC)
    target_compile_options(${name} PRIVATE -Wall -Wextra)
  endif()
  add_test(NAME ${name} COMMAND ${name})
endfunction()

# --- Behavioural test: C2 (VideoEncoderTemplate::IsMatchingIndex = vs ==) ---
se_add_test(test_video_encoder_template_demo
  test_video_encoder_template_demo.cpp)

# --- Source-invariant test: catches re-introduction of any of the
#     bug patterns this PR fixes. ---
se_add_test(test_source_invariants
  test_source_invariants.cpp)
