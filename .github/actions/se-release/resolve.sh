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
# bound, and the honest one is "whatever the destination channel served until a moment
# ago" -- the version sitting on it right now. Reading it here rather than after the
# upload is the same ordering constraint the rollback path has, for the same reason: the
# upload is about to overwrite it with the version being promoted.
#
# Computed for every forward hop, not just beta. It was beta-only until the release and
# its issues moved to signed -> qa, which left that hop passing no base at all: the CLI
# logged "No recent releases found; assuming first sync", inspected the single HEAD
# commit, and attached one issue to release 26.8.20.897 where the range held eight.
#
# Windows and macOS promote separately, so the second platform through reads back the
# first one's upload and resolves an empty range. That is correct rather than a bug: the
# issues were attached on the first pass and `sync` for the same version adds nothing.
#
# Unlike the rollback branch above this one falls through: every promotion now writes a
# release, so previous_tag and the recovered notes are needed on this hop too.
case "${SE_TO:-}" in
qa | beta | latest)
	BASE=""
	dest="${RUNNER_TEMP:-/tmp}/se-$SE_TO-$PLATFORM.manifest"
	# stderr is dropped because manifest_version_number reports a parse failure with
	# die(), and an ::error:: annotation would misrepresent what is only a missing lower
	# bound: the CLI falls back to its own scan base and the sync still happens.
	if fetch_channel_manifest "$PLATFORM" "$SE_TO" "$dest" && B=$(manifest_version_number "$dest" 2> /dev/null); then
		if [ "$B" -ge "$ENCODED" ]; then
			note "linear: $PLATFORM/$SE_TO already serves $B (>= $ENCODED); no new commits to scan"
		else
			BASE=$(decode_version "$B")
			note "linear: $PLATFORM/$SE_TO serves $B; scanning $BASE..$TAG"
		fi
	else
		warn "linear: no readable version on $PLATFORM/$SE_TO; the CLI will choose its own scan base"
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
	;;
esac

# A release no longer exists before the first promotion -- signed -> qa is what creates
# it -- so its absence is reported rather than fatal. Requirement 10, "nothing reaches
# `latest` without a release", is enforced in publish.sh, which is the step that knows
# which hop this is and can still create one.
if IS_PRERELEASE=$(gh release view "$TAG" --json isPrerelease -q .isPrerelease 2>/dev/null); then
	note "GitHub release $TAG exists (prerelease=$IS_PRERELEASE)"
else
	IS_PRERELEASE=""
	note "no GitHub release for $TAG yet; this promotion will create it"
fi
emit is_prerelease "$IS_PRERELEASE"

# --------------------------------------------------------------- requirements 8 and 9
PREV_FULL_TAG=""
resolve_previous_full_tag "$BUILD" "$ENCODED"
PREV_TAG="$PREV_FULL_TAG"
emit previous_tag "$PREV_TAG"

# ------------------------------------------------------------------ Linear scan base
# The Linear release must list every issue fixed since the last FULL release, so its
# commit scan takes the same lower bound as the changelog in the GitHub release body --
# PREV_TAG -- rather than the version currently on the destination channel.
#
# Those two ranges are not the same range. Consecutive qa builds are a day or two apart,
# while a full release can be weeks back, so scanning only the channel range is what left
# Linear release 26.8.20.897 holding a fraction of what the changelog listed. Same base,
# same commits, same issues, by construction.
#
# previous_channel_tag remains the fallback and is strictly better than nothing: until
# the first full release exists PREV_TAG resolves only through the `latest` manifest
# bootstrap, and when that is unavailable too it is empty. An empty base_ref sends the
# CLI back to its own scan base -- one commit, one issue -- which is the failure this is
# here to prevent.
LINEAR_BASE="$PREV_TAG"
if [ -z "$LINEAR_BASE" ] && [ -n "${BASE:-}" ]; then
	LINEAR_BASE="$BASE"
	note "linear: no previous full release; falling back to channel base $LINEAR_BASE"
fi
if [ -n "$LINEAR_BASE" ]; then
	note "linear: issue scan range $LINEAR_BASE..$TAG"
else
	warn "linear: no scan base resolved; the CLI will fall back to its own, which sees only the head commit"
fi
emit linear_base_tag "$LINEAR_BASE"

# ------------------------------------------------------------------- requirement 5
# Recover the pristine RELEASE_NOTES.md fenced into an existing release body. This is a
# fallback: the notes artifact fetched from the source channel is the primary source,
# and on signed -> qa there is no release to recover from yet.
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
if [ ! -s "$OUTDIR/notes.md" ]; then
	note "no notes recovered from a release body for $TAG; the channel artifact will be used"
fi
