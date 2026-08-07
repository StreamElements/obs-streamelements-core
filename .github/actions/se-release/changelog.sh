#!/bin/sh
#
# mode=changelog -- requirement 4, both halves:
#
#   * GitHub's own generated notes, for merged PRs and New Contributors, with an explicit
#     previous_tag_name so the range is never guessed;
#   * a git log section, because a large share of this repo's work lands as direct pushes
#     to master, which PR-based generation cannot see at all.
#
# env: SE_TAG, SE_PREVIOUS_TAG, SE_CHANGELOG_FILE, GH_TOKEN, GITHUB_REPOSITORY
set -eu
. "$SE_ACTION_PATH/lib.sh"

TAG="${SE_TAG:?mode=changelog requires 'tag'}"
PREV="${SE_PREVIOUS_TAG:-}"
OUT="${SE_CHANGELOG_FILE:?mode=changelog requires 'changelog-file'}"
TMP="${RUNNER_TEMP:-/tmp}"

# What `git log` walks to. build.yml passes $GITHUB_SHA because its tag was created via
# the API moments earlier and may not have landed in the local clone yet; release.yml
# lets this default to the tag, which is long since visible by then.
HEAD_REF="${SE_HEAD_REF:-$TAG}"

mkdir -p "$(dirname "$OUT")"
: > "$OUT"

if [ -z "$PREV" ]; then
	warn "no previous full release tag; emitting an empty changelog"
	exit 0
fi
note "changelog range: $PREV..$TAG"

printf '## Changelog\n\n' >> "$OUT"

# ---- GitHub's generated notes (PRs, New Contributors) ------------------------------
gen="$TMP/se-generated-notes.md"
gen_ok=false
if gh api -X POST "repos/$GITHUB_REPOSITORY/releases/generate-notes" \
	-f tag_name="$TAG" -f previous_tag_name="$PREV" \
	-q .body > "$gen" 2> "$TMP/se-gen-err.txt" && [ -s "$gen" ]; then
	# Demote its "## " headings so they nest under our "## Changelog".
	sed 's/^## /### /' "$gen" >> "$OUT"
	printf '\n' >> "$OUT"
	gen_ok=true
else
	warn "releases/generate-notes failed for $PREV..$TAG: $(tr -d '\n' < "$TMP/se-gen-err.txt")"
	warn "continuing with the git-log section only"
fi

# ---- contributors -----------------------------------------------------------------
# Derived from git log rather than from the generated notes above, because a large share
# of this repo's work lands as direct pushes to master, whose authors PR-based generation
# cannot see. The per-commit list this used to print was dropped: the generated section
# already covers what changed, and a raw commit dump is noise in a user-facing release.
#
# --invert-grep drops CI's own commits at the source; the bot name filter below is
# belt-and-braces and survives a future change to the commit-message prefix.
# Joined with awk rather than `paste -sd ', '`: paste treats the delimiter argument as a
# LIST of single characters used cyclically, so ', ' yields "a,b c" instead of "a, b, c".
people=$(
	git log --no-merges --invert-grep --grep='^\[WORKFLOW-AUTOMATION\]' \
		--pretty=tformat:'%an' "$PREV..$HEAD_REF" 2>/dev/null |
		grep -v '^SE\.Live Release Bot$' |
		sort -u |
		awk 'NR > 1 { printf ", " } { printf "%s", $0 } END { if (NR) print "" }'
) || people=""
[ -n "$people" ] && printf '**Contributors:** %s\n\n' "$people" >> "$OUT"

# GitHub's generated notes already end with their own "**Full Changelog**:" line, so only
# emit ours when that section is missing -- otherwise the body carries the link twice.
if [ "$gen_ok" != "true" ]; then
	printf '**Full changelog:** %s/%s/compare/%s...%s\n' \
		"${GITHUB_SERVER_URL:-https://github.com}" "${GITHUB_REPOSITORY:-}" "$PREV" "$TAG" >> "$OUT"
fi
