#!/bin/sh
#
# mode=prevtag -- resolve the previous FULL release tag for a build that has just been
# tagged, so build.yml can attach a changelog to the [BETA] prerelease.
#
# Requirement 8 applies to prereleases as well as full releases: the base is always the
# last version that actually reached users on `latest`, never the previous prerelease.
# Because a prerelease is cut for every build with notes, diffing against the previous
# prerelease would yield fragmented one-commit changelogs.
#
# env: SE_TAG, SE_VERSION_ENCODED, GH_TOKEN, GITHUB_REPOSITORY
set -eu
. "$SE_ACTION_PATH/lib.sh"

TAG="${SE_TAG:?mode=prevtag requires 'tag'}"
ENCODED="${SE_VERSION_ENCODED:?mode=prevtag requires 'version-encoded'}"

is_version_tag "$TAG" || die "'$TAG' does not look like a version tag (yy.m.d.build)"

BUILD=$(tag_build_number "$TAG")
PREV_FULL_TAG=""
resolve_previous_full_tag "$BUILD" "$ENCODED"

note "previous full release tag: ${PREV_FULL_TAG:-<none>}"
emit previous_tag "$PREV_FULL_TAG"
