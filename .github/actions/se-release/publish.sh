#!/bin/sh
#
# mode=publish   flip the [BETA] prerelease for $TAG into a full, Latest release
# mode=rollback  put a displaced release back to [BETA], and re-assert the incoming one
#
# Both are convergent: safe to re-run, and safe to run again for the second platform's
# promotion of the same version.
#
# env: SE_MODE, SE_TAG, SE_PREVIOUS_TAG, SE_NOTES_FILE, SE_CHANGELOG_FILE,
#      SE_DISPLACED_TAG, SE_SHOULD_REVERT, SE_SUMMARY, GH_TOKEN, DRY_RUN
set -eu
. "$SE_ACTION_PATH/lib.sh"

TAG="${SE_TAG:?publish requires 'tag'}"
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

# ---------------------------------------------------------------------- mode: publish
# One API call, read through gh's own query engine rather than jq: gh embeds gojq, so
# this has no external dependency. The first line is isPrerelease; everything after it is
# the body, which may itself contain newlines.
if ! gh release view "$TAG" --json body,isPrerelease -q '.isPrerelease, .body' > "$TMP/se-current.txt" 2>/dev/null; then
	# resolve.sh already failed hard on this for a real promotion, so reaching here means
	# a dry-run or a hand-invoked call.
	warn "no GitHub release for $TAG; nothing to publish"
	exit 0
fi
WAS_PRERELEASE=$(head -n 1 "$TMP/se-current.txt")
cur="$TMP/se-current-body.md"
tail -n +2 "$TMP/se-current.txt" > "$cur"

# Idempotence for the second platform's promotion of the same version, and for any
# re-run: a body we already composed carries $PUBLISHED_MARKER. Short-circuit so we
# neither churn the body nor discard it.
if [ "$WAS_PRERELEASE" = "false" ] && grep -qF "$PUBLISHED_MARKER" "$cur"; then
	note "$TAG is already a published full release; only re-asserting Latest."
	gh_write release edit "$TAG" --latest
	exit 0
fi

# Decide the notes source and log it BEFORE composing. note()/warn() write to stdout so
# GitHub renders them as annotations, which means they must never be called inside the
# redirection below or they end up inside the published release body.
if [ -n "${SE_RELEASE_NOTES_FILE:-}" ] && [ -s "${SE_RELEASE_NOTES_FILE:-}" ] &&
	! grep -qF "$NOTES_UNAVAILABLE_MARKER" "$SE_RELEASE_NOTES_FILE"; then
	# The artifact that travelled with the manifest from `signed/`: Claude's blurb plus
	# the hand-written notes, composed at build time. Reused verbatim, so the text a user
	# sees on `latest` is exactly what was reviewed on the prerelease and no second model
	# call is needed.
	note "using the release notes artifact that travelled with the manifest"
	USE_ARTIFACT=true
else
	# Fallback for builds predating the artifact, or where the CDN copy is missing:
	# rebuild from the notes recovered out of the prerelease body. No blurb on this path.
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

# Always render the exact body, dry-run or not -- seeing what would be published is the
# entire point of a dry-run here.
{
	printf '### Release body for %s (dry-run: %s)\n\n' "$TAG" "${DRY_RUN:-true}"
	printf '~~~markdown\n'
	cat "$body"
	printf '\n~~~\n'
} >> "${GITHUB_STEP_SUMMARY:-/dev/null}"

# ----------------------------------------------------------------- requirements 1 & 2
# Drop the [BETA] prefix, clear prerelease, take the Latest designation.
note "Publishing $TAG as a full release"
gh_write release edit "$TAG" \
	--title "$TAG" \
	--prerelease=false \
	--latest \
	--notes-file "$body"
