#pragma once

#include "cef-headers.hpp"
#include "SEVideoCompositionManager.hpp"
#include "SEVideoComposition.hpp"

class SESharedVideoCompositionManager: public SEVideoCompositionEventListener {
private:
	std::shared_mutex m_mutex;

	std::map<std::string,
		 std::shared_ptr<
			 SEVideoCompositionBase::CompositionInfo>>
		m_canvasUUIDToVideoCompositionInfoMap;

	std::shared_ptr<SEVideoCompositionManager>
		m_videoCompositionManager = nullptr;

private:
	bool SerializeCanvas(obs_canvas_t *canvas,
			     CefRefPtr<CefDictionaryValue> d);

	static void handle_obs_frontend_event(enum obs_frontend_event event,
					      void *data);

public:
	SESharedVideoCompositionManager(
		std::shared_ptr<SEVideoCompositionManager> videoCompositionManager);

	~SESharedVideoCompositionManager();

	void Reset();

	void DeserializeSharedVideoComposition(CefRefPtr<CefValue> input,
					       CefRefPtr<CefValue> &output);

	void DeserializeSharedVideoCompositionProperties(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output);

	void SerializeAllSharedVideoCompositions(CefRefPtr<CefValue> &output);

	void RemoveSharedVideoCompositionsByIds(CefRefPtr<CefValue> input,
						CefRefPtr<CefValue> &output);

	void ConnectVideoCompositionToSharedVideoComposition(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output);

	void DisconnectVideoCompositionsFromSharedVideoCompositionsByIds(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output);
};
