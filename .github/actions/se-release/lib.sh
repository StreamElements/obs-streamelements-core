# shellcheck shell=sh
#
# Shared helpers for GitHub Release automation. Sourced, never executed.
#
# Terminology used throughout these scripts:
#
#   encoded   the 14-digit version as it appears in a manifest, e.g. 20260205000549
#   tag       the dotted form that is actually a git tag,        e.g. 26.2.5.549
#   S         the version in the manifest being copied (the `from` channel)
#   L         the version currently sitting on the `latest` channel (the `to` channel,
#             relevant only on a stable -> latest rollback, where the upload replaces it)

# Delimiters that fence the pristine RELEASE_NOTES.md inside the notes artifact, and so
# inside every release body composed from it. build.yml writes them; release.yml reads
# back between them when the artifact is missing. They are what makes recomposing a body
# idempotent rather than cumulative -- without them, each promotion would wrap the
# previous body (summary + notes + changelog) inside a new one.
RELEASE_NOTES_BEGIN='<!-- SE_RELEASE_NOTES_BEGIN -->'
RELEASE_NOTES_END='<!-- SE_RELEASE_NOTES_END -->'

# Present on any body publish.sh has composed, at every channel. Used to short-circuit
# the second platform's promotion of the same version, which would otherwise recompose
# an identical body for no reason.
PUBLISHED_MARKER='<!-- SE_RELEASE_PUBLISHED -->'

# Written to a channel when the build being published there has no notes of its own.
#
# The manifest and the version SVG are always replaced on a promotion, so the notes file
# has to be replaced too -- leaving the previous one behind would make the channel serve
# notes describing a build it no longer points at. A placeholder keeps the three in step
# without pretending the notes exist; publish.sh treats a file carrying this marker as
# absent and falls back to the notes recovered from an existing release body.
NOTES_UNAVAILABLE_MARKER='<!-- SE_RELEASE_NOTES_UNAVAILABLE -->'

CDN_BASE='https://cdn.streamelements.com/obs/dist/obs-streamelements'

die() { printf '::error::%s\n' "$*" >&2; exit 1; }
warn() { printf '::warning::%s\n' "$*"; }
note() { printf '::notice::%s\n' "$*"; }

# Append a key=value pair to the step outputs, if we are running inside Actions.
emit() {
	[ -n "${GITHUB_OUTPUT:-}" ] || return 0
	printf '%s=%s\n' "$1" "$2" >> "$GITHUB_OUTPUT"
}

# Pull version_number out of an INI manifest.
#
# Three traps, all verified against the live files:
#   1. the manifests are CRLF end to end                 -> tr -d '\r'
#   2. the same 14-digit number also appears three times -> anchor on the key, never on
#      inside package_url / package_url_32 / _64            a bare run of digits
#   3. an arbitrary HTML blob follows the marker          -> stop at [[[BEGIN_RELEASE_NOTES]]]
#      [[[BEGIN_RELEASE_NOTES]]]
manifest_version_number() {
	_f="$1"
	[ -r "$_f" ] || die "manifest not readable: $_f"
	_n=$(
		tr -d '\r' < "$_f" |
			awk '/^\[\[\[BEGIN_RELEASE_NOTES\]\]\]/ { exit } { print }' |
			sed -n 's/^[[:space:]]*version_number[[:space:]]*=[[:space:]]*\([0-9][0-9]*\).*$/\1/p' |
			head -n 1
	)
	case "$_n" in
	'' | *[!0-9]*) die "no numeric 'version_number=' found in $_f" ;;
	esac
	[ "${#_n}" -eq 14 ] || die "version_number '$_n' in $_f is not 14 digits"
	printf '%s\n' "$_n"
}

# Echo every distinct package_url* value from an INI manifest, one per line.
#
# Logs nothing: callers capture stdout, so a stray note() here would end up in the
# returned list. Same parsing traps as manifest_version_number -- CRLF, and the HTML
# release-notes blob that follows the marker. The ?ts= cache-buster is stripped so the
# value can be used as a filename; it plays no part in fetching.
#
# Windows manifests repeat a URL across fields (package_url and package_url_32 are the
# same installer), and on macOS all three point at the same .pkg, so the list is deduped
# with the first occurrence winning.
manifest_package_urls() {
	_f="$1"
	[ -r "$_f" ] || return 1
	tr -d '\r' < "$_f" |
		awk '/^\[\[\[BEGIN_RELEASE_NOTES\]\]\]/ { exit } { print }' |
		sed -n 's#^[[:space:]]*package_url[a-z0-9_]*[[:space:]]*=[[:space:]]*\(https\{0,1\}://[^[:space:]]*\).*$#\1#p' |
		sed 's/?.*$//' |
		awk 'NF && !seen[$0]++'
}

# Fetch a channel manifest to a local path. Cache-busted: the promotion upload sets
# cache-control max-age=60, so a read shortly after a promotion can otherwise be stale.
# Returns non-zero (without dying) when the channel has no manifest.
fetch_channel_manifest() {
	_platform="$1"
	_channel="$2"
	_dest="$3"
	curl -fsS --max-time 30 -H 'Cache-Control: no-cache' -o "$_dest" \
		"$CDN_BASE/$_platform/$_channel/obs-streamelements.manifest?cb=$$-$_platform-$_channel"
}

# 20260205000549 -> 26.2.5.549
#
# Mirrors GetStreamElementsPluginVersionString() in
# streamelements/StreamElementsUtils.cpp, which is what the shipped plugin reports.
# Components are unpadded, matching every existing tag (26.2.5.549, 25.1.9.330).
# Note the century is discarded, exactly as the C++ does.
decode_version() {
	_n="$1"
	# Reject leading zeros explicitly: $(( )) would interpret them as octal.
	case "$_n" in
	'' | *[!0-9]* | 0*) die "decode_version: '$_n' is not a positive decimal number" ;;
	esac
	printf '%d.%d.%d.%d\n' \
		"$(((_n % 1000000000000) / 10000000000))" \
		"$(((_n % 10000000000) / 100000000))" \
		"$(((_n % 100000000) / 1000000))" \
		"$((_n % 1000000))"
}

# 26.2.5.549 -> 549
#
# The build number is BASE_BUILD_NUMBER + github.run_number, i.e. a global monotonic
# counter across the whole tag history. That makes it a truer total order than the
# yy.m.d prefix, which has at least one bogus entry (tag 15.12.15.535, whose build
# number 535 nonetheless slots correctly between 25.12.8.530 and 26.1.12.537).
tag_build_number() { printf '%s\n' "${1##*.}"; }

# True when $1 looks like one of this repo's tags.
is_version_tag() {
	case "$1" in
	'' | *[!0-9.]*) return 1 ;;
	esac
	printf '%s\n' "$1" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$'
}

# Resolve the tag of the previous FULL release into the global PREV_FULL_TAG (empty when
# there is none). Used by both workflows, since requirement 8 applies to prereleases and
# full releases alike.
#
#   $1  build number of the version in hand
#   $2  its encoded version, used to bound the bootstrap
#
# Sets a global rather than echoing its result on purpose: this function logs progress
# with note()/warn(), and those must go to stdout for GitHub to render them as
# annotations. A caller using $(...) would capture the log lines along with the tag.
#
# Selection is by build number strictly below $1 rather than "whatever is newest". That
# is what makes this safe to re-run: the answer is identical whether or not the current
# tag has already been flipped to a full release, so the self-reference hazard cannot
# arise even if a caller invokes it after the flip.
resolve_previous_full_tag() {
	_build="$1"
	_encoded="$2"
	_prev=$(
		gh api --paginate -X GET "repos/$GITHUB_REPOSITORY/releases" \
			-q '.[] | select(.draft == false and .prerelease == false) | .tag_name' 2>/dev/null |
			grep -E '^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$' |
			awk -F. -v cur="$_build" '($4 + 0) < (cur + 0) { print $4 "\t" $0 }' |
			sort -k1,1nr | head -n 1 | cut -f2
	) || _prev=""

	if [ -z "$_prev" ]; then
		# No full release older than this one. True on the very first promotion (this
		# repo has zero releases today), and true again on the second platform's
		# promotion of the same version, where the only full release is the current one
		# and is filtered out by the build-number bound.
		warn "no previous full release found; bootstrapping from the live 'latest' manifests"
		_best=0
		for _plat in windows macos; do
			_f="${RUNNER_TEMP:-/tmp}/se-bootstrap-$_plat.manifest"
			if fetch_channel_manifest "$_plat" latest "$_f"; then
				if _v=$(manifest_version_number "$_f"); then
					note "bootstrap: $_plat/latest = $_v"
					# Excluding anything >= the version in hand is the second line of
					# defence behind step order, and it is what makes the second
					# platform's promotion resolve to the same answer as the first.
					if [ "$_v" -lt "$_encoded" ] && [ "$_v" -gt "$_best" ]; then _best="$_v"; fi
				else
					warn "bootstrap: $_plat/latest is unparseable"
				fi
			else
				warn "bootstrap: could not fetch $_plat/latest manifest"
			fi
		done
		if [ "$_best" -gt 0 ]; then
			_prev=$(decode_version "$_best")
			note "bootstrap: previous version resolved to $_prev"
		else
			warn "bootstrap found nothing older than $_encoded; the changelog will be omitted"
		fi
	fi

	# The changelog needs the tag object, not merely a release, and a version that
	# reached a channel is not guaranteed to have been tagged. windows/stable served
	# 20241127000268 for a long time with no 24.11.27.268 tag; the tag and a back-filled
	# release were created by hand on 2026-08-25, so that particular gap is closed, but
	# the guard stays -- nothing in the pipeline enforces that a channel version was
	# ever tagged.
	if [ -n "$_prev" ] && ! git rev-parse -q --verify "refs/tags/$_prev^{commit}" > /dev/null 2>&1; then
		warn "previous tag '$_prev' is not present in this clone; the changelog will be omitted"
		_prev=""
	fi

	PREV_FULL_TAG="$_prev"
}

# Every mutating gh call goes through this, so the dry-run guard lives in exactly one
# place instead of on a dozen `if:` expressions.
gh_write() {
	if [ "${DRY_RUN:-true}" = "true" ]; then
		printf '::notice::[dry-run] would run: gh %s\n' "$*"
		return 0
	fi
	gh "$@"
}
