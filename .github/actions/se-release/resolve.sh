#!/bin/sh
#
# mode=resolve -- everything that must be read BEFORE the promotion is written to the CDN.
#
# This step is ordered above "Upload Manifests to CDN Google Storage" for two independent
# reasons, and reordering it silently breaks both:
#
#   promote path   when no full release exists yet, the previous version is bootstrapped
#                  from the live `latest` manifests. After the upload that read returns
#                  the version currently being promoted.
#   rollback path  L *is* the current `latest` manifest's version_number, which the upload
#                  is about to overwrite with S. Read afterwards, `S < L` compares S with
#                  itself and the guard never fires.
#
# env: SE_MANIFEST, SE_PLATFORM, SE_TO, SE_IS_ROLLBACK, SE_OUTPUT_DIR,
#      GH_TOKEN, GITHUB_REPOSITORY
set -eu
. "$SE_ACTION_PATH/lib.sh"

MANIFEST="${SE_MANIFEST:?mode=resolve requires 'manifest'}"
PLATFORM="${SE_PLATFORM:?mode=resolve requires 'platform'}"
OUTDIR="${SE_OUTPUT_DIR:?mode=resolve requires 'output-dir'}"
IS_ROLLBACK="${SE_IS_ROLLBACK:-false}"
mkdir -p "$OUTDIR"

# ------------------------------------------------------------------- requirement 3
# The version in the manifest being copied. On a promotion this is the version arriving
# on `latest`; on a stable -> latest rollback it is S, the older build coming back.
ENCODED=$(manifest_version_number "$MANIFEST")
TAG=$(decode_version "$ENCODED")
BUILD=$(tag_build_number "$TAG")
note "manifest version_number=$ENCODED -> tag $TAG (build $BUILD)"

emit version_encoded "$ENCODED"
emit version_string "$TAG"

# ------------------------------------------------------- requirements 11 and 12
# Rollback: work out which release is being displaced from `latest`, and whether it is
# safe to relabel it.
if [ "$IS_ROLLBACK" = "true" ]; then
	other=$([ "$PLATFORM" = "windows" ] && echo macos || echo windows)
	should_revert=false
	displaced_tag=""

	cur="${RUNNER_TEMP:-/tmp}/se-latest-$PLATFORM.manifest"
	if fetch_channel_manifest "$PLATFORM" latest "$cur"; then
		L=$(manifest_version_number "$cur")
		note "rollback: $PLATFORM/latest currently serves $L; incoming is $ENCODED"

		if [ "$ENCODED" -ge "$L" ]; then
			# Nothing is moving backwards, so nothing is displaced.
			note "rollback: $ENCODED >= $L, no release is displaced from 'latest'"
		else
			displaced_tag=$(decode_version "$L")
			# Requirement 12: a full release is cut when ANY platform reaches `latest`,
			# so it may only be revoked once NO platform still serves it. Only this
			# platform's channel is about to change, so the other platform's current
			# `latest` is already its post-rollback state.
			oth="${RUNNER_TEMP:-/tmp}/se-latest-$other.manifest"
			if fetch_channel_manifest "$other" latest "$oth"; then
				O=$(manifest_version_number "$oth")
				if [ "$O" = "$L" ]; then
					note "rollback: $other/latest still serves $L; leaving release $displaced_tag as a full release"
				else
					note "rollback: $other/latest serves $O, not $L; release $displaced_tag will revert to prerelease"
					should_revert=true
				fi
			else
				# Conservative: never relabel a release we cannot prove is unused.
				warn "rollback: could not read $other/latest; leaving release $displaced_tag alone"
			fi
		fi
	else
		warn "rollback: could not read $PLATFORM/latest; no release will be reverted"
	fi

	emit displaced_tag "$displaced_tag"
	emit should_revert "$should_revert"
	exit 0
fi

# -------------------------------------------------------------------- Linear sync base
# `qa -> beta` is the hop at which a build becomes something worth announcing, so that is
# where release.yml creates the Linear release for it. The commit scan needs a lower
# bound, and the honest one is "whatever beta served until a moment ago" -- which is the
# version sitting on the destination channel right now. Reading it here rather than after
# the upload is the same ordering constraint the rollback path has, for the same reason:
# the upload is about to overwrite it with the version being promoted.
#
# Windows and macOS promote separately, so the second platform through reads back the
# first one's upload and resolves an empty range. That is correct rather than a bug: the
# issues were attached on the first pass and `sync` for the same version adds nothing.
#
# Nothing below this point applies to a beta promotion -- no release is created or
# relabelled there, so requirement 10 must not fire and there is no changelog to base.
if [ "${SE_TO:-}" = "beta" ]; then
	BASE=""
	beta="${RUNNER_TEMP:-/tmp}/se-beta-$PLATFORM.manifest"
	# stderr is dropped because manifest_version_number reports a parse failure with
	# die(), and an ::error:: annotation would misrepresent what is only a missing lower
	# bound: the CLI falls back to its own scan base and the sync still happens.
	if fetch_channel_manifest "$PLATFORM" beta "$beta" && B=$(manifest_version_number "$beta" 2> /dev/null); then
		if [ "$B" -ge "$ENCODED" ]; then
			note "linear: $PLATFORM/beta already serves $B (>= $ENCODED); no new commits to scan"
		else
			BASE=$(decode_version "$B")
			note "linear: $PLATFORM/beta serves $B; scanning $BASE..$TAG"
		fi
	else
		warn "linear: no readable version on $PLATFORM/beta; the CLI will choose its own scan base"
	fi

	# The scan is `git log <base>..<tag>`, so the lower bound has to exist as a tag in
	# this clone. Not every version that reached a channel was tagged -- windows/stable
	# references 20241127000268 and tag 24.11.27.268 was never pushed -- so this is a live
	# condition rather than a hypothetical.
	if [ -n "$BASE" ] && ! git rev-parse -q --verify "refs/tags/$BASE^{commit}" > /dev/null 2>&1; then
		warn "linear: tag '$BASE' is not present in this clone; the CLI will choose its own scan base"
		BASE=""
	fi

	emit previous_channel_tag "$BASE"
	exit 0
fi

# ------------------------------------------------------------------ requirement 10
# A version may not reach `latest` without a release. The usual cause is a build whose
# RELEASE_NOTES.md was empty: build.yml then skips both the tag and the prerelease, so
# there is nothing to promote. Failing here means it fails before anything ships.
if IS_PRERELEASE=$(gh release view "$TAG" --json isPrerelease -q .isPrerelease 2>/dev/null); then
	note "GitHub release $TAG exists (prerelease=$IS_PRERELEASE)"
else
	die "version $ENCODED decodes to tag $TAG, which has no GitHub release. The master build that produced it almost certainly ran with an empty RELEASE_NOTES.md, so .github/workflows/build.yml skipped both the tag and the [BETA] prerelease. Add release notes and rebuild, or create tag $TAG and its prerelease by hand, before promoting this build to 'latest'."
fi
emit is_prerelease "$IS_PRERELEASE"

# --------------------------------------------------------------- requirements 8 and 9
PREV_FULL_TAG=""
resolve_previous_full_tag "$BUILD" "$ENCODED"
PREV_TAG="$PREV_FULL_TAG"
emit previous_tag "$PREV_TAG"

# ------------------------------------------------------------------- requirement 5
# Recover the pristine RELEASE_NOTES.md that build.yml fenced into the prerelease body.
# Done here rather than in publish.sh because Claude needs it as input, and Claude runs
# in between.
: > "$OUTDIR/notes.md"
gh release view "$TAG" --json body -q .body > "$OUTDIR/body.md" 2>/dev/null || : > "$OUTDIR/body.md"
awk -v b="$RELEASE_NOTES_BEGIN" -v e="$RELEASE_NOTES_END" '
	$0 == b { inside = 1; next }
	$0 == e { inside = 0; next }
	inside  { print }
' "$OUTDIR/body.md" > "$OUTDIR/notes.md"

if [ ! -s "$OUTDIR/notes.md" ] && [ -r RELEASE_NOTES.history.md ]; then
	# Fallback for releases created before this automation, or hand-edited ones:
	# build.yml archives every version's notes under a "### <version>" heading.
	warn "no fenced notes in the body of $TAG; falling back to RELEASE_NOTES.history.md"
	awk -v v="### $TAG" '
		$0 == v          { found = 1; next }
		found && /^### / { exit }
		found            { print }
	' RELEASE_NOTES.history.md > "$OUTDIR/notes.md" || :
fi
[ -s "$OUTDIR/notes.md" ] || warn "no release notes recovered for $TAG"
