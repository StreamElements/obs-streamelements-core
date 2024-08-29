#pragma once

#include "StreamElementsOutput.hpp"
#include "StreamElementsVideoCompositionManager.hpp"

class StreamElementsOutputManager {
private:
	std::recursive_mutex m_mutex;
	std::map<StreamElementsOutputBase::ObsOutputType,
		 std::map<std::string, std::shared_ptr<StreamElementsOutputBase>>>
		m_map;
	std::shared_ptr<StreamElementsVideoCompositionManager> m_compositionManager;

public:
	StreamElementsOutputManager(
		std::shared_ptr<StreamElementsVideoCompositionManager> compositionManager);
	~StreamElementsOutputManager();

public:
	void
	DeserializeOutput(StreamElementsOutputBase::ObsOutputType outputType,
			  CefRefPtr<CefValue> input,
			  CefRefPtr<CefValue> &output);
	void SerializeAllOutputs(StreamElementsOutputBase::ObsOutputType outputType, CefRefPtr<CefValue> &output);
	void
	RemoveOutputsByIds(StreamElementsOutputBase::ObsOutputType outputType,
			   CefRefPtr<CefValue> input,
			   CefRefPtr<CefValue> &output);
	void
	EnableOutputsByIds(StreamElementsOutputBase::ObsOutputType outputType,
			   CefRefPtr<CefValue> input,
			   CefRefPtr<CefValue> &output);
	void
	DisableOutputsByIds(StreamElementsOutputBase::ObsOutputType outputType,
			    CefRefPtr<CefValue> input,
			    CefRefPtr<CefValue> &output);

	void Reset();

private:
	bool GetValidIds(StreamElementsOutputBase::ObsOutputType outputType,
			 CefRefPtr<CefValue> input,
			 std::map<std::string, bool> &output, bool testRemove,
			 bool testDisable);
};
