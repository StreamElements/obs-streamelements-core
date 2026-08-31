# Profiles

`window.host`

## `getAllProfiles(ResultCallback<ProfileInfo[]>)`

**Available since API version 1.22**

Get all available OBS profiles.

## `getCurrentProfile(ResultCallback<ProfileInfo>)`

**Available since API version 1.22**

Get current OBS profile.

## `setCurrentProfileById(ProfileInfo, ResultCallback<success>)`

**Available since API version 1.22**

Set current OBS profile by ID.

Profile ID is indicated by *ProfileInfo.id*.

**Note**: profiles cannot be switched while Streaming and/or Recording are active.
