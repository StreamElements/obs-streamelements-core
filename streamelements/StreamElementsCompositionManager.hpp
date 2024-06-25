#include "StreamElementsComposition.hpp"

class StreamElementsCompositionManager {
private:
	std::recursive_mutex m_mutex;
	std::map<std::string, std::shared_ptr<StreamElementsCompositionBase>> m_map;

public:
	StreamElementsCompositionManager();
	~StreamElementsCompositionManager();

public:
	void DeserializeComposition(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output);
	void SerializeAllCompositions(CefRefPtr<CefValue> &output);
	void RemoveCompositionsByIds(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output);
};
