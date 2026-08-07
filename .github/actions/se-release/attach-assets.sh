#!/bin/sh
#
# mode=assets -- attach the platform installers to a GitHub release.
#
# The installers themselves live on the CDN; the manifest is what says which files
# belong to a build, so the URLs are read from there rather than reconstructed from the
# version. That keeps this correct if the installer naming convention ever changes --
# the manifest is the contract the updater already relies on.
#
# env: SE_TAG, SE_VERSION_ENCODED, SE_PLATFORMS, SE_CHANNEL, GH_TOKEN, DRY_RUN
set -eu
. "$SE_ACTION_PATH/lib.sh"

TAG="${SE_TAG:?mode=assets requires 'tag'}"
ENCODED="${SE_VERSION_ENCODED:?mode=assets requires 'version-encoded'}"
CHANNEL="${SE_CHANNEL:-signed}"
PLATFORMS="${SE_PLATFORMS:-windows macos}"
WORK="${RUNNER_TEMP:-/tmp}/se-assets"
mkdir -p "$WORK"

attached=0
skipped=0

for platform in $PLATFORMS; do
	manifest="$WORK/$platform.manifest"

	if ! fetch_channel_manifest "$platform" "$CHANNEL" "$manifest"; then
		warn "no '$CHANNEL' manifest for $platform; no installers attached for it"
		skipped=$((skipped + 1))
		continue
	fi

	if ! found=$(manifest_version_number "$manifest"); then
		warn "could not read a version from the $platform '$CHANNEL' manifest"
		skipped=$((skipped + 1))
		continue
	fi

	# The channel is a moving pointer: by the time this runs, a newer build may already
	# have published over it. Attaching then would ship someone else's installers under
	# this release's tag, which is worse than shipping none.
	if [ "$found" != "$ENCODED" ]; then
		warn "$platform/$CHANNEL now holds $found but this release is $ENCODED; skipping so the wrong build's installers are not attached"
		skipped=$((skipped + 1))
		continue
	fi

	urls=$(manifest_package_urls "$manifest") || urls=""
	if [ -z "$urls" ]; then
		warn "no package_url entries in the $platform '$CHANNEL' manifest"
		skipped=$((skipped + 1))
		continue
	fi

	for url in $urls; do
		name=$(basename "$url")
		file="$WORK/$name"

		if ! curl -fsSL --max-time 600 -o "$file" "$url"; then
			warn "could not download $url"
			skipped=$((skipped + 1))
			continue
		fi

		# A CDN error page is a 200 with a small HTML body; an installer never is.
		size=$(wc -c < "$file" | tr -d ' ')
		if [ "$size" -lt 102400 ]; then
			warn "$name is only $size bytes -- not a plausible installer; skipping"
			skipped=$((skipped + 1))
			continue
		fi

		note "attaching $name ($((size / 1048576)) MiB) to $TAG"
		# --clobber so re-running finalize replaces rather than duplicates.
		if gh_write release upload "$TAG" "$file" --clobber; then
			attached=$((attached + 1))
		else
			warn "failed to upload $name to $TAG"
			skipped=$((skipped + 1))
		fi
	done
done

if [ "$attached" -eq 0 ]; then
	warn "no installers were attached to $TAG. The release is still valid -- notes and changelog are unaffected -- but users cannot download binaries from it."
else
	note "attached $attached installer(s) to $TAG ($skipped skipped)"
fi
