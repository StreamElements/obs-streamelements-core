#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR="${REPO_ROOT}/build-linux"
OUTPUT_DIR="${REPO_ROOT}/release-artifacts"
DISTRO_TAG="linux"
OBS_VERSION_TAG="obs-unknown"
ARCHIVE_VERSION_TAG=""
ARCHIVE_BASENAME=""
STAGING_DIR=""

usage() {
  cat <<USAGE
Usage: $(basename "$0") [options]

Package obs-streamelements-core as a precompiled Linux release artifact.

Options:
  --build-dir <path>        Build directory to package (default: ${BUILD_DIR})
  --output-dir <path>       Output directory for archives (default: ${OUTPUT_DIR})
  --distro-tag <tag>        Distro/runtime tag for archive name (default: ${DISTRO_TAG})
  --obs-version <tag>       OBS version tag for archive name (default: ${OBS_VERSION_TAG})
  --version <tag>           Release version tag (default: git describe or current commit)
  --archive-name <name>     Explicit archive basename without .tar.gz
  -h, --help                Show this help

Examples:
  $(basename "$0") --distro-tag fedora43 --obs-version obs32.1.2
  $(basename "$0") --build-dir ./build-linux --output-dir ./dist --version v0.1.0
USAGE
}

log() {
  printf '[package-linux-release] %s\n' "$*"
}

fail() {
  printf '[package-linux-release] ERROR: %s\n' "$*" >&2
  exit 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      [[ $# -ge 2 ]] || fail "Missing value for --build-dir"
      BUILD_DIR="$2"
      shift 2
      ;;
    --output-dir)
      [[ $# -ge 2 ]] || fail "Missing value for --output-dir"
      OUTPUT_DIR="$2"
      shift 2
      ;;
    --distro-tag)
      [[ $# -ge 2 ]] || fail "Missing value for --distro-tag"
      DISTRO_TAG="$2"
      shift 2
      ;;
    --obs-version)
      [[ $# -ge 2 ]] || fail "Missing value for --obs-version"
      OBS_VERSION_TAG="$2"
      shift 2
      ;;
    --version)
      [[ $# -ge 2 ]] || fail "Missing value for --version"
      ARCHIVE_VERSION_TAG="$2"
      shift 2
      ;;
    --archive-name)
      [[ $# -ge 2 ]] || fail "Missing value for --archive-name"
      ARCHIVE_BASENAME="$2"
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

if [[ -z "${ARCHIVE_VERSION_TAG}" ]]; then
  if git -C "${REPO_ROOT}" describe --tags --always >/dev/null 2>&1; then
    ARCHIVE_VERSION_TAG="$(git -C "${REPO_ROOT}" describe --tags --always)"
  else
    ARCHIVE_VERSION_TAG="manual"
  fi
fi

if [[ -z "${ARCHIVE_BASENAME}" ]]; then
  ARCHIVE_BASENAME="obs-streamelements-core-${ARCHIVE_VERSION_TAG}-${DISTRO_TAG}-${OBS_VERSION_TAG}"
fi

mkdir -p "${OUTPUT_DIR}"

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
[[ -d "${REPO_ROOT}/data" ]] || fail "Missing data directory under ${REPO_ROOT}"

STAGING_DIR="$(mktemp -d)"
trap 'rm -rf "${STAGING_DIR}"' EXIT

PLUGIN_ROOT="${STAGING_DIR}/${ARCHIVE_BASENAME}/obs-streamelements-core"
BIN_DIR="${PLUGIN_ROOT}/bin/64bit"
DATA_DIR="${PLUGIN_ROOT}/data/obs-plugins/obs-streamelements-core"

mkdir -p "${BIN_DIR}" "${DATA_DIR}"

install -m 0755 "${SO_PATH}" "${BIN_DIR}/obs-streamelements-core.so"
cp -a "${REPO_ROOT}/data/." "${DATA_DIR}/"

cat > "${STAGING_DIR}/${ARCHIVE_BASENAME}/INSTALL.txt" <<EOF
obs-streamelements-core Linux prebuilt package

Target tags:
- distro/runtime: ${DISTRO_TAG}
- OBS version: ${OBS_VERSION_TAG}
- release id: ${ARCHIVE_VERSION_TAG}

Install:
1. Close OBS.
2. Copy the obs-streamelements-core directory into:
   ~/.config/obs-studio/plugins/
3. Install the matching prebuilt obs-browser package from the paired release.
4. Start OBS and validate the plugin in the same OBS/runtime family used for this package.

Notes:
- This package is not universal across all Linux distributions.
- Match distro family, OBS version, Qt/CEF stack, and architecture.
- Do not mix this custom obs-browser flow with another active obs-browser variant.
EOF

ARCHIVE_PATH="${OUTPUT_DIR}/${ARCHIVE_BASENAME}.tar.gz"
tar -C "${STAGING_DIR}" -czf "${ARCHIVE_PATH}" "${ARCHIVE_BASENAME}"

log "Created archive: ${ARCHIVE_PATH}"
