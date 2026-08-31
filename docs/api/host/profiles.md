# Profiles

`window.host`

## `getAllProfiles(ResultCallback<ProfileInfo[]>)`

**Available since API version 1.22**

Get all available OBS profiles.

**Data structures:** [`ProfileInfo`](../types/ProfileInfo.md)

## `getCurrentProfile(ResultCallback<ProfileInfo>)`

**Available since API version 1.22**

Get current OBS profile.

**Data structures:** [`ProfileInfo`](../types/ProfileInfo.md)

## `setCurrentProfileById(ProfileInfo, ResultCallback<success>)`

> ⚠️ **Name does not match the implementation.** The registered handler is `setCurrentProfile`, which does what is described here; no handler named `setCurrentProfileById` has ever existed.

**Available since API version 1.22**

Set current OBS profile by ID.

Profile ID is indicated by *ProfileInfo.id*.

**Note**: profiles cannot be switched while Streaming and/or Recording are active.

**Data structures:** [`ProfileInfo`](../types/ProfileInfo.md)
