#!/bin/sh
#
# mode=release   create or relabel the GitHub release for $TAG to match the channel it
#                has just been promoted to
# mode=rollback  put a displaced release back to [BETA], and re-assert the incoming one
#
# The release now follows the channel rather than the build:
#
#   signed -> qa      created as a prerelease titled "[ALPHA] <tag>"
#   qa -> beta        relabelled to "[BETA] <tag>", still a prerelease
#   beta -> latest    titled "<tag>", full release, Latest
#
# Nothing is created at build time. A build that is never promoted has a tag and no
# release, which is the point: the releases page lists what was actually shipped
# somewhere rather than every master build that happened to carry notes.
#
# Both modes are convergent: safe to re-run, and safe to run again for the second
# platform's promotion of the same version.
#
# env: SE_MODE, SE_TAG, SE_TO, SE_PREVIOUS_TAG, SE_NOTES_FILE, SE_RELEASE_NOTES_FILE,
#      SE_CHANGELOG_FILE, SE_DISPLACED_TAG, SE_SHOULD_REVERT, GH_TOKEN, DRY_RUN
set -eu
. "$SE_ACTION_PATH/lib.sh"

TAG="${SE_TAG:?release requires 'tag'}"
MODE="${SE_MODE:?}"
TMP="${RUNNER_TEMP:-/tmp}"

# ------------------------------------------------------------------ requirements 11-12
if [ "$MODE" = "rollback" ]; then
	DISPLACED="${SE_DISPLACED_TAG:-}"
	if [ "${SE_SHOULD_REVERT:-false}" = "true" ] && [ -n "$DISPLACED" ]; then
		note "Reverting $DISPLACED to a prerelease titled '[BETA] $DISPLACED'"
		# Setting --prerelease clears the Latest designation server-side. The body is
		# deliberately left intact: it still carries the fenced notes, so a later
		# re-promotion recomposes identically.
		#
		# [BETA] rather than [ALPHA]: the release is being taken off `latest`, and the
		# channel it demonstrably still qualifies for is beta -- it was there before it
		# was promoted.
		if gh release view "$DISPLACED" --json id > /dev/null 2>&1; then
			gh_write release edit "$DISPLACED" --prerelease --title "[BETA] $DISPLACED"
		else
			warn "no GitHub release for displaced version $DISPLACED; nothing to revert"
		fi
	else
		note "No release is being displaced from 'latest'; leaving GitHub releases alone."
	fi

	# Re-assert the incoming (older) version as the full, Latest release. It may predate
	# this automation or never have been tagged at all -- windows/stable currently points
	# at 20241127000268, whose tag 24.11.27.268 does not exist -- so warn, never fail:
	# the revert above is the part that matters.
	if gh release view "$TAG" --json id > /dev/null 2>&1; then
		note "Re-asserting $TAG as the full, Latest release"
		gh_write release edit "$TAG" --prerelease=false --title "$TAG" --latest
	else
		warn "no GitHub release for $TAG; cannot re-assert it as Latest. The 'latest' channel now serves a build with no GitHub release."
	fi
	exit 0
fi

# ------------------------------------------------------------------------ mode: release
TO="${SE_TO:?mode=release requires 'to'}"
case "$TO" in
qa) TITLE="[ALPHA] $TAG" ; WANT_PRERELEASE=true ;;
beta) TITLE="[BETA] $TAG" ; WANT_PRERELEASE=true ;;
latest) TITLE="$TAG" ; WANT_PRERELEASE=false ;;
*) die "mode=release does not handle promotions to '$TO' (expected qa, beta or latest)" ;;
esac

# One API call, read through gh's own query engine rather than jq: gh embeds gojq, so
# this has no external dependency. The first line is isPrerelease; everything after it is
# the body, which may itself contain newlines.
EXISTS=true
if ! gh release view "$TAG" --json body,isPrerelease -q '.isPrerelease, .body' > "$TMP/se-current.txt" 2> /dev/null; then
	EXISTS=false
fi

cur="$TMP/se-current-body.md"
: > "$cur"
WAS_PRERELEASE=""
if [ "$EXISTS" = "true" ]; then
	WAS_PRERELEASE=$(head -n 1 "$TMP/se-current.txt")
	tail -n +2 "$TMP/se-current.txt" > "$cur"
fi

if [ "$EXISTS" = "false" ]; then
	# ------------------------------------------------------------- requirement 10
	# A version may not reach `latest` without a release. Kept as a hard failure here,
	# and only here: reaching `latest` is what makes a build everyone's.
	[ "$TO" != "latest" ] || die "version $TAG has no GitHub release and no promotion created one. Promote it to 'qa' first, or create the release by hand, before promoting it to 'latest'."

	# Every release is anchored to the tag build.yml pushed for the built commit. No tag
	# means the build ran with an empty RELEASE_NOTES.md, so build.yml skipped tagging
	# and there is nothing to anchor to. Warn rather than fail: the channel promotion
	# itself is legitimate, and the hard stop above still catches it at `latest`.
	if ! gh api "repos/$GITHUB_REPOSITORY/git/ref/tags/$TAG" > /dev/null 2>&1; then
		warn "no tag $TAG in this repository, so no release can be created for it. The master build that produced this version almost certainly ran with an empty RELEASE_NOTES.md. Promoting it to 'latest' will fail until a release exists."
		exit 0
	fi
fi

# A release that is already full must never be demoted by a later qa or beta promotion
# of the same version -- re-promoting a shipped build through the channels is routine,
# and relabelling it [ALPHA] would misrepresent what users are running.
if [ "$EXISTS" = "true" ] && [ "$WAS_PRERELEASE" = "false" ] && [ "$WANT_PRERELEASE" = "true" ]; then
	note "$TAG is already a full release; leaving it untouched rather than relabelling it '$TITLE'."
	exit 0
fi

# Idempotence for the second platform's promotion of the same version, and for any
# re-run: a body we already composed carries $PUBLISHED_MARKER. Short-circuit so we
# neither churn the body nor discard it.
if [ "$EXISTS" = "true" ] && [ "$WAS_PRERELEASE" = "$WANT_PRERELEASE" ] && grep -qF "$PUBLISHED_MARKER" "$cur"; then
	note "$TAG is already recorded for '$TO'; re-asserting its title and designation only."
	if [ "$TO" = "latest" ]; then
		gh_write release edit "$TAG" --title "$TITLE" --latest
	else
		gh_write release edit "$TAG" --title "$TITLE"
	fi
	exit 0
fi

# Decide the notes source and log it BEFORE composing. note()/warn() write to stdout so
# GitHub renders them as annotations, which means they must never be called inside the
# redirection below or they end up inside the published release body.
if [ -n "${SE_RELEASE_NOTES_FILE:-}" ] && [ -s "${SE_RELEASE_NOTES_FILE:-}" ] &&
	! grep -qF "$NOTES_UNAVAILABLE_MARKER" "$SE_RELEASE_NOTES_FILE"; then
	# The artifact that travelled with the manifest from `signed/`: the What's New blurb
	# plus the hand-written notes, composed at build time. Reused verbatim at every hop,
	# so the text does not drift between [ALPHA], [BETA] and the full release.
	note "using the release notes artifact that travelled with the manifest"
	USE_ARTIFACT=true
else
	# Fallback for builds predating the artifact, or where the CDN copy is missing:
	# rebuild from the notes recovered out of the existing release body. No blurb on this
	# path.
	warn "no release notes artifact for $TAG; composing from the recovered notes instead"
	USE_ARTIFACT=false
fi

body="$TMP/se-release-body.md"
{
	printf '%s\n\n' "$PUBLISHED_MARKER"

	if [ "$USE_ARTIFACT" = "true" ]; then
		cat "$SE_RELEASE_NOTES_FILE"
		printf '\n'
	else
		printf '## Release Notes\n\n'
		printf '%s\n' "$RELEASE_NOTES_BEGIN"
		if [ -n "${SE_NOTES_FILE:-}" ] && [ -s "${SE_NOTES_FILE:-}" ]; then cat "$SE_NOTES_FILE"; fi
		printf '%s\n\n' "$RELEASE_NOTES_END"
	fi

	# Always recomputed rather than taken from the artifact: the range is relative to the
	# previous full release, which can have moved since this build was made.
	if [ -n "${SE_CHANGELOG_FILE:-}" ] && [ -s "${SE_CHANGELOG_FILE:-}" ]; then cat "$SE_CHANGELOG_FILE"; fi
} > "$body"

# GitHub rejects release bodies over 125000 characters.
if [ "$(wc -c < "$body")" -gt 120000 ]; then
	warn "composed body exceeds 120000 bytes; truncating"
	head -c 118000 "$body" > "$body.cut"
	printf '\n\n_Release notes truncated; see the full changelog link above._\n' >> "$body.cut"
	mv "$body.cut" "$body"
fi

# Hand the composed body to whatever else has to publish the same text -- release.yml
# feeds it to the Linear release, so the two descriptions cannot drift. Written even on
# a dry run, so a rehearsal exercises that path too.
if [ -n "${SE_OUTPUT_DIR:-}" ]; then
	mkdir -p "$SE_OUTPUT_DIR"
	cp "$body" "$SE_OUTPUT_DIR/release-body.md"
	note "wrote $SE_OUTPUT_DIR/release-body.md"
fi
# Always render the exact body, dry-run or not -- seeing what would be published is the
# entire point of a dry-run here.
{
	printf '### Release body for %s -> %s (dry-run: %s)\n\n' "$TITLE" "$TO" "${DRY_RUN:-true}"
	printf '~~~markdown\n'
	cat "$body"
	printf '\n~~~\n'
} >> "${GITHUB_STEP_SUMMARY:-/dev/null}"

if [ "$EXISTS" = "false" ]; then
	note "Creating release $TITLE for the '$TO' channel"
	# No --target: the tag already exists and points at the commit that was built, so
	# naming it is both sufficient and safer than resolving a commit here.
	if [ "$WANT_PRERELEASE" = "true" ]; then
		gh_write release create "$TAG" --prerelease --title "$TITLE" --notes-file "$body"
	else
		gh_write release create "$TAG" --title "$TITLE" --latest --notes-file "$body"
	fi
	exit 0
fi

note "Updating release $TAG to $TITLE for the '$TO' channel"
if [ "$WANT_PRERELEASE" = "true" ]; then
	gh_write release edit "$TAG" --prerelease --title "$TITLE" --notes-file "$body"
else
	# ------------------------------------------------------------- requirements 1 & 2
	# Drop the [ALPHA]/[BETA] prefix, clear prerelease, take the Latest designation.
	gh_write release edit "$TAG" --title "$TITLE" --prerelease=false --latest --notes-file "$body"
fi
