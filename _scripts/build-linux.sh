#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR="${REPO_ROOT}/build-linux"
BUILD_TYPE="RelWithDebInfo"
QT_VERSION="6"
GENERATOR="Ninja"
TARGET="obs-streamelements-core"
DETECTED_JOBS="$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN || echo 1)"
MAX_DEFAULT_JOBS=4
if [[ "${DETECTED_JOBS}" -gt "${MAX_DEFAULT_JOBS}" ]]; then
  JOBS="${MAX_DEFAULT_JOBS}"
else
  JOBS="${DETECTED_JOBS}"
fi
CLEAN=0
CONFIGURE_ONLY=0
NO_CONFIGURE=0
INSTALL_USER=0
USER_PLUGIN_DIR="${XDG_CONFIG_HOME:-${HOME}/.config}/obs-studio/plugins/obs-streamelements-core"
OBS_BROWSER_DIR=""
CMAKE_ARGS=()

usage() {
  cat <<USAGE
Usage: $(basename "$0") [options]

Build obs-streamelements-core on Linux.

Options:
  --build-dir <path>          Build directory (default: ${BUILD_DIR})
  --build-type <type>         CMAKE_BUILD_TYPE (default: ${BUILD_TYPE})
  --qt-version <5|6>          Qt major version for CMake (default: ${QT_VERSION})
  --generator <name>          CMake generator (default: ${GENERATOR})
  --target <name>             Build target (default: ${TARGET})
  --jobs <n>                  Parallel jobs for cmake --build (default: ${JOBS}; detected: ${DETECTED_JOBS})
  --clean                     Remove build directory before configuring
  --no-configure              Skip configure step and only build
  --configure-only            Run configure step only
  --cmake-arg <arg>           Extra CMake configure argument (repeatable)
  --install-user              Install built plugin into ~/.config/obs-studio/plugins
  --user-plugin-dir <path>    Install root for plugin (default: ${USER_PLUGIN_DIR})
  --obs-browser-dir <path>    Path to obs-browser checkout (optional; auto-detected as sibling ../obs-browser)
  -h, --help                  Show this help

Examples:
  $(basename "$0")
  $(basename "$0") --clean --install-user
  $(basename "$0") --build-dir ./build-ci --cmake-arg -DCMAKE_CXX_COMPILER=clang++
USAGE
}

log() {
  printf '[build-linux] %s\n' "$*"
}

fail() {
  printf '[build-linux] ERROR: %s\n' "$*" >&2
  exit 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      [[ $# -ge 2 ]] || fail "Missing value for --build-dir"
      BUILD_DIR="$2"
      shift 2
      ;;
    --build-type)
      [[ $# -ge 2 ]] || fail "Missing value for --build-type"
      BUILD_TYPE="$2"
      shift 2
      ;;
    --qt-version)
      [[ $# -ge 2 ]] || fail "Missing value for --qt-version"
      QT_VERSION="$2"
      shift 2
      ;;
    --generator)
      [[ $# -ge 2 ]] || fail "Missing value for --generator"
      GENERATOR="$2"
      shift 2
      ;;
    --target)
      [[ $# -ge 2 ]] || fail "Missing value for --target"
      TARGET="$2"
      shift 2
      ;;
    --jobs)
      [[ $# -ge 2 ]] || fail "Missing value for --jobs"
      JOBS="$2"
      shift 2
      ;;
    --clean)
      CLEAN=1
      shift
      ;;
    --configure-only)
      CONFIGURE_ONLY=1
      shift
      ;;
    --no-configure)
      NO_CONFIGURE=1
      shift
      ;;
    --cmake-arg)
      [[ $# -ge 2 ]] || fail "Missing value for --cmake-arg"
      CMAKE_ARGS+=("$2")
      shift 2
      ;;
    --install-user)
      INSTALL_USER=1
      shift
      ;;
    --user-plugin-dir)
      [[ $# -ge 2 ]] || fail "Missing value for --user-plugin-dir"
      USER_PLUGIN_DIR="$2"
      shift 2
      ;;
    --obs-browser-dir)
      [[ $# -ge 2 ]] || fail "Missing value for --obs-browser-dir"
      OBS_BROWSER_DIR="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      fail "Unknown argument: $1"
      ;;
  esac
done

if [[ "${CONFIGURE_ONLY}" -eq 1 && "${NO_CONFIGURE}" -eq 1 ]]; then
  fail "--configure-only and --no-configure cannot be used together"
fi

if [[ "${QT_VERSION}" != "5" && "${QT_VERSION}" != "6" ]]; then
  fail "--qt-version must be either 5 or 6"
fi

command -v cmake >/dev/null 2>&1 || fail "cmake is required"

if [[ -z "${OBS_BROWSER_DIR}" ]]; then
  AUTO_OBS_BROWSER_DIR="$(cd "${REPO_ROOT}/.." && pwd)/obs-browser"
  if [[ -d "${AUTO_OBS_BROWSER_DIR}" ]]; then
    OBS_BROWSER_DIR="${AUTO_OBS_BROWSER_DIR}"
  fi
fi

if [[ -n "${OBS_BROWSER_DIR}" ]]; then
  [[ -d "${OBS_BROWSER_DIR}" ]] || fail "obs-browser directory not found: ${OBS_BROWSER_DIR}"
  log "Using obs-browser from: ${OBS_BROWSER_DIR}"
  CMAKE_ARGS+=("-DOBS_BROWSER_DIR=${OBS_BROWSER_DIR}")
fi

if [[ "${CLEAN}" -eq 1 ]]; then
  log "Cleaning build directory: ${BUILD_DIR}"
  rm -rf "${BUILD_DIR}"
fi

if [[ "${NO_CONFIGURE}" -eq 0 ]]; then
  log "Configuring: ${REPO_ROOT} -> ${BUILD_DIR}"
  cmake \
    -S "${REPO_ROOT}" \
    -B "${BUILD_DIR}" \
    -G "${GENERATOR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DQT_VERSION="${QT_VERSION}" \
    "${CMAKE_ARGS[@]}"
fi

if [[ "${CONFIGURE_ONLY}" -eq 1 ]]; then
  log "Configure-only mode complete"
  exit 0
fi

log "Building target '${TARGET}' with ${JOBS} jobs"
cmake --build "${BUILD_DIR}" --target "${TARGET}" --parallel "${JOBS}"

if [[ "${INSTALL_USER}" -eq 1 ]]; then
  SO_PATH=""
  CANDIDATES=(
    "${BUILD_DIR}/libobs-streamelements-core.so"
    "${BUILD_DIR}/obs-streamelements-core.so"
    "${BUILD_DIR}/plugins/obs-streamelements-core/libobs-streamelements-core.so"
    "${BUILD_DIR}/plugins/obs-streamelements-core/obs-streamelements-core.so"
  )

  for candidate in "${CANDIDATES[@]}"; do
    if [[ -f "${candidate}" ]]; then
      SO_PATH="${candidate}"
      break
    fi
  done

  if [[ -z "${SO_PATH}" ]]; then
    SO_PATH="$(
      find "${BUILD_DIR}" -maxdepth 5 -type f \
        \( -name 'obs-streamelements-core.so' -o -name 'libobs-streamelements-core.so' \) \
        | head -n 1 || true
    )"
  fi

  [[ -n "${SO_PATH}" ]] || fail "Could not locate obs-streamelements-core.so under ${BUILD_DIR}"

  BIN_DIR="${USER_PLUGIN_DIR}/bin/64bit"
  DATA_DIR="${USER_PLUGIN_DIR}/data/obs-plugins/obs-streamelements-core"

  log "Installing binary to ${BIN_DIR}"
  mkdir -p "${BIN_DIR}"
  install -m 0755 "${SO_PATH}" "${BIN_DIR}/obs-streamelements-core.so"

  log "Installing data files to ${DATA_DIR}"
  rm -rf "${DATA_DIR}"
  mkdir -p "${DATA_DIR}"
  cp -a "${REPO_ROOT}/data/." "${DATA_DIR}/"

  log "User install complete"
fi

log "Done"
