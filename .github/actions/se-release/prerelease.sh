#!/bin/sh
#
# mode=prerelease -- create (or refresh) the [BETA] prerelease for a freshly tagged
# master build. Called from build.yml's `finalize` job.
#
# env: SE_TAG, SE_NOTES_FILE, GH_TOKEN, GITHUB_SHA, DRY_RUN
set -eu
. "$SE_ACTION_PATH/lib.sh"

TAG="${SE_TAG:?mode=prerelease requires 'tag'}"
NOTES="${SE_NOTES_FILE:?mode=prerelease requires 'notes-file'}"
TITLE="[BETA] $TAG"

is_version_tag "$TAG" || die "'$TAG' does not look like a version tag (yy.m.d.build)"

# build.yml gates this step on has_release_notes == 'true', so an empty file here means
# that gate is broken rather than that there is nothing to say.
[ -s "$NOTES" ] || die "$NOTES is empty -- build.yml should not have reached this step"

BODY="${RUNNER_TEMP:-/tmp}/se-prerelease-body.md"
{
	if [ -n "${SE_SUMMARY:-}" ]; then
		printf "## What's new\n\n%s\n\n" "$SE_SUMMARY"
	fi
	# The hand-written notes are preserved verbatim between the fence markers, so the
	# promotion to a full release can recover them exactly and recompose (requirement 5).
	printf '%s\n' "$RELEASE_NOTES_BEGIN"
	cat "$NOTES"
	printf '\n%s\n\n' "$RELEASE_NOTES_END"
	if [ -n "${SE_CHANGELOG_FILE:-}" ] && [ -s "${SE_CHANGELOG_FILE:-}" ]; then
		cat "$SE_CHANGELOG_FILE"
	fi
} > "$BODY"

{
	printf '### Prerelease body for %s (dry-run: %s)\n\n' "$TITLE" "${DRY_RUN:-true}"
	printf '~~~markdown\n'
	cat "$BODY"
	printf '\n~~~\n'
} >> "${GITHUB_STEP_SUMMARY:-/dev/null}"

if state=$(gh release view "$TAG" --json isPrerelease -q .isPrerelease 2>/dev/null); then
	if [ "$state" = "false" ]; then
		# A finalize re-run for a version that has since been promoted to the `latest`
		# channel. Demoting a shipped release back to [BETA] would be actively harmful.
		note "Release $TAG is already a full release; leaving it untouched."
		emit release_action skipped
		exit 0
	fi
	note "Prerelease $TAG already exists; refreshing title and body."
	gh_write release edit "$TAG" --prerelease --title "$TITLE" --notes-file "$BODY"
	emit release_action refreshed
else
	# --target pins the release to the commit that was actually built. The tagger step
	# ran seconds ago and tag visibility through the API is not instantaneous; without
	# --target, a gh-side tag miss would silently anchor the release to whatever master
	# points at now, which may already have moved.
	note "Creating prerelease $TAG"
	gh_write release create "$TAG" --prerelease --title "$TITLE" \
		--notes-file "$BODY" --target "$GITHUB_SHA"
	emit release_action created
fi
