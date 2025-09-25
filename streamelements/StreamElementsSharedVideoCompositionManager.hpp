#pragma once

#include "cef-headers.hpp"
#include "StreamElementsVideoCompositionManager.hpp"
#include "StreamElementsVideoComposition.hpp"

class StreamElementsSharedVideoCompositionManager: public StreamElementsVideoCompositionEventListener {
private:
	std::shared_mutex m_mutex;

	std::map<std::string,
		 std::shared_ptr<
			 StreamElementsVideoCompositionBase::CompositionInfo>>
		m_canvasUUIDToVideoCompositionInfoMap;

	std::shared_ptr<StreamElementsVideoCompositionManager>
		m_videoCompositionManager = nullptr;

private:
	bool SerializeCanvas(obs_canvas_t *canvas,
			     CefRefPtr<CefDictionaryValue> d);

public:
	StreamElementsSharedVideoCompositionManager(
		std::shared_ptr<StreamElementsVideoCompositionManager> videoCompositionManager);

	~StreamElementsSharedVideoCompositionManager();

	void DeserializeSharedVideoComposition(CefRefPtr<CefValue> input,
					       CefRefPtr<CefValue> &output);

	void SerializeAllSharedVideoCompositions(CefRefPtr<CefValue> input,
						 CefRefPtr<CefValue> &output);

	void RemoveSharedVideoCompositionsByIds(CefRefPtr<CefValue> input,
						CefRefPtr<CefValue> &output);

	void ConnectVideoCompositionToSharedVideoComposition(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output);

	void DisconnectVideoCompositionsFromSharedVideoCompositionsByIds(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output);
};
