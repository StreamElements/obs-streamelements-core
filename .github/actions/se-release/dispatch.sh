#!/bin/sh
#
# Entry point for the se-release composite action. Routes to a mode script.
set -eu

: "${SE_ACTION_PATH:?SE_ACTION_PATH is not set}"
. "$SE_ACTION_PATH/lib.sh"

case "${SE_MODE:-}" in
prevtag) sh "$SE_ACTION_PATH/prevtag.sh" ;;
prerelease) sh "$SE_ACTION_PATH/prerelease.sh" ;;
assets) sh "$SE_ACTION_PATH/attach-assets.sh" ;;
resolve) sh "$SE_ACTION_PATH/resolve.sh" ;;
changelog) sh "$SE_ACTION_PATH/changelog.sh" ;;
publish | rollback) sh "$SE_ACTION_PATH/publish.sh" ;;
*) die "unknown mode '${SE_MODE:-}' (expected: prevtag, prerelease, assets, resolve, changelog, publish, rollback)" ;;
esac
