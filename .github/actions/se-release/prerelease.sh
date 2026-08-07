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

# The reusable artifact: the version-pinned half of the release body -- Claude's blurb
# plus the hand-written notes verbatim. This is published to the CDN next to the manifest
# as obs-streamelements.release_notes.md and travels with it through the channels, so
# promoting a build to `latest` reuses this text instead of paying for another model call.
#
# The changelog is deliberately NOT in here. It is cheap to recompute and it is relative
# to the previous full release, which can differ between build time and promotion time;
# freezing it would publish a stale range.
# The blurb arrives either inline or as a file. The file form is preferred: model
# output never passes through GitHub expression interpolation on that path.
SUMMARY="${SE_SUMMARY:-}"
if [ -z "$SUMMARY" ] && [ -n "${SE_SUMMARY_FILE:-}" ] && [ -s "${SE_SUMMARY_FILE:-}" ]; then
	SUMMARY=$(cat "$SE_SUMMARY_FILE")
fi
if [ -n "$SUMMARY" ]; then
	note "composing the release body with a What's New section"
else
	warn "no release summary available; composing from notes and changelog only"
fi

ARTIFACT="${RUNNER_TEMP:-/tmp}/obs-streamelements.release_notes.md"
{
	if [ -n "$SUMMARY" ]; then
		printf "## What's New\n\n%s\n\n" "$SUMMARY"
	fi
	# The heading sits outside the fence on purpose: the fence must hold the pristine
	# RELEASE_NOTES.md and nothing else, so a promotion can recover it byte for byte.
	printf '## Release Notes\n\n'
	printf '%s\n' "$RELEASE_NOTES_BEGIN"
	cat "$NOTES"
	printf '\n%s\n' "$RELEASE_NOTES_END"
} > "$ARTIFACT"

if [ -n "${SE_OUTPUT_DIR:-}" ]; then
	mkdir -p "$SE_OUTPUT_DIR"
	cp "$ARTIFACT" "$SE_OUTPUT_DIR/obs-streamelements.release_notes.md"
	note "wrote $SE_OUTPUT_DIR/obs-streamelements.release_notes.md for upload to the CDN"
fi

BODY="${RUNNER_TEMP:-/tmp}/se-prerelease-body.md"
{
	cat "$ARTIFACT"
	printf '\n'
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
