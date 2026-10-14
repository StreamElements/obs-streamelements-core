#pragma once

#include "cef-headers.hpp"
#include "SEVideoCompositionManager.hpp"
#include "SEVideoComposition.hpp"

#include <memory>

//
// Forward declaration rather than an include: the real header drags windows.h
// in ahead of winsock2.h on these translation units.
//
class SEWebsocketApiServer;

class SEMessageBus;

class SESharedVideoCompositionManager: public SEVideoCompositionEventListener {
private:
	std::shared_ptr<SEWebsocketApiServer> m_apiServer;
	std::shared_ptr<SEMessageBus> m_messageBus;

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
		std::shared_ptr<SEWebsocketApiServer> apiServer,
		std::shared_ptr<SEVideoCompositionManager>
			videoCompositionManager,
		std::shared_ptr<SEMessageBus> messageBus);

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
