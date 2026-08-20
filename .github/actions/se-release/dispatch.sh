#!/bin/sh
#
# Entry point for the se-release composite action. Routes to a mode script.
set -eu

: "${SE_ACTION_PATH:?SE_ACTION_PATH is not set}"
. "$SE_ACTION_PATH/lib.sh"

case "${SE_MODE:-}" in
prevtag) sh "$SE_ACTION_PATH/prevtag.sh" ;;
notes) sh "$SE_ACTION_PATH/notes.sh" ;;
assets) sh "$SE_ACTION_PATH/attach-assets.sh" ;;
resolve) sh "$SE_ACTION_PATH/resolve.sh" ;;
changelog) sh "$SE_ACTION_PATH/changelog.sh" ;;
release | rollback) sh "$SE_ACTION_PATH/publish.sh" ;;
*) die "unknown mode '${SE_MODE:-}' (expected: prevtag, notes, assets, resolve, changelog, release, rollback)" ;;
esac
