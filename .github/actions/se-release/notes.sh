#!/bin/sh
#
# mode=notes -- compose obs-streamelements.release_notes.md, the reusable half of a
# release body. Called from build.yml's `finalize` job.
#
# This is the only release-related thing build.yml still does, and it has to happen
# there: RELEASE_NOTES.md is truncated and pushed moments later, and the What's New
# blurb comes from the `summary` job of the same run. Neither exists any more by the
# time a promotion runs.
#
# The artifact is uploaded to the CDN next to the manifest and travels with it through
# the channels, so every hop composes its release body from the same text -- what a user
# reads on `latest` is exactly what was reviewed on the [ALPHA] release.
#
# The changelog is deliberately NOT in here. It is cheap to recompute and it is relative
# to the previous full release, which moves between promotions; freezing it would
# publish a stale range.
#
# env: SE_TAG, SE_NOTES_FILE, SE_OUTPUT_DIR, SE_SUMMARY, SE_SUMMARY_FILE
set -eu
. "$SE_ACTION_PATH/lib.sh"

TAG="${SE_TAG:?mode=notes requires 'tag'}"
NOTES="${SE_NOTES_FILE:?mode=notes requires 'notes-file'}"
OUTDIR="${SE_OUTPUT_DIR:?mode=notes requires 'output-dir'}"

is_version_tag "$TAG" || die "'$TAG' does not look like a version tag (yy.m.d.build)"

# build.yml gates this step on has_release_notes == 'true', so an empty file here means
# that gate is broken rather than that there is nothing to say.
[ -s "$NOTES" ] || die "$NOTES is empty -- build.yml should not have reached this step"

# The blurb arrives either inline or as a file. The file form is preferred: model output
# never passes through GitHub expression interpolation on that path.
SUMMARY="${SE_SUMMARY:-}"
if [ -z "$SUMMARY" ] && [ -n "${SE_SUMMARY_FILE:-}" ] && [ -s "${SE_SUMMARY_FILE:-}" ]; then
	SUMMARY=$(cat "$SE_SUMMARY_FILE")
fi
if [ -n "$SUMMARY" ]; then
	note "composing the release notes artifact with a What's New section"
else
	warn "no release summary available; the artifact will carry the notes alone"
fi

mkdir -p "$OUTDIR"
ARTIFACT="$OUTDIR/obs-streamelements.release_notes.md"
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

note "wrote $ARTIFACT for upload to the CDN"

{
	printf '### Release notes artifact for %s\n\n' "$TAG"
	printf '~~~markdown\n'
	cat "$ARTIFACT"
	printf '\n~~~\n'
} >> "${GITHUB_STEP_SUMMARY:-/dev/null}"
