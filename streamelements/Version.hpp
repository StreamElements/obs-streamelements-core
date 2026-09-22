#pragma once

/* Numeric value indicating the current major version of the API.
 * This value must be incremented each time a breaking change to
 * the API is introduced(change of existing API methods/properties
 * signatures).
 */
#ifndef HOST_API_VERSION_MAJOR
#define HOST_API_VERSION_MAJOR 6
#endif

/* Numeric value indicating the current minor version of the API.
 * This value will be incremented each time a non-breaking change
 * to the API is introduced (additional functionality, bugfixes
 * of existing functionality).
 *
 * 6.8 is the Razer WYVRN surface: getHostCapabilities, getAllRazerWyvrnEvents,
 * setRazerWyvrnEvent and hostRazerWyvrnStatusChanged. All of it is compiled
 * out wherever SE_ENABLE_WYVRN is not defined -- always on macOS, where the
 * SDK does not exist -- so the minor version follows the same switch rather
 * than being set by hand. Hand-setting it let the two disagree: when the
 * integration was held back the version had to be rolled back separately,
 * and flipping the build option alone would have registered the 6.8 calls
 * while still reporting 6.7.
 */
#ifndef HOST_API_VERSION_MINOR
#ifdef SE_ENABLE_WYVRN
#define HOST_API_VERSION_MINOR 8
#else
#define HOST_API_VERSION_MINOR 7
#endif
#endif

/* Numeric value in the YYYYMMDDHHmmss format, indicating the current
 * version of the plugin.
 *
 * This version number is used by obs-streamelements plug-in to
 * determine whether an update is available and should be offered to
 * the user.
 *
 * This value should be set as part of the build process.
 */
#ifndef STREAMELEMENTS_PLUGIN_VERSION
#include "Version.generated.hpp"
#endif
