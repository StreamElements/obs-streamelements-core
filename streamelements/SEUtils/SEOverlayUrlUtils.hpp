#pragma once

//
// The StreamElements overlay URL format: parsing one into its ids and
// composing an editor URL from them.
//
// These two keep the StreamElements prefix deliberately -- it names an
// external system's URL scheme, not an object in this plug-in.
//

#include <string>

bool ParseStreamElementsOverlayURL(std::string url, std::string &overlayId,
				   std::string &accountId);

std::string GetStreamElementsOverlayEditorURL(std::string overlayId,
					      std::string accountId);
