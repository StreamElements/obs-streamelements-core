#pragma once

#include "SEOutput.hpp"
#include "SEVideoCompositionManager.hpp"
#include "SEAudioCompositionManager.hpp"

#include <shared_mutex>

class SEOutputManager {
private:
	std::shared_mutex m_mutex;
	std::map<ObsOutputType,
		 std::shared_ptr<std::map<std::string, std::shared_ptr<SEOutputBase>>>>
		m_map;
	std::shared_ptr<SEVideoCompositionManager>
		m_videoCompositionManager;
	std::shared_ptr<SEAudioCompositionManager>
		m_audioCompositionManager;

public:
	SEOutputManager(
		std::shared_ptr<SEVideoCompositionManager>
			videoCompositionManager,
		std::shared_ptr<SEAudioCompositionManager>
			audioCompositionManager);
	~SEOutputManager();

public:
	void DeserializeOutput(ObsOutputType outputType,
			  CefRefPtr<CefValue> input,
			  CefRefPtr<CefValue> &output);
	void SerializeAllOutputs(ObsOutputType outputType,
				 CefRefPtr<CefValue> &output);
	void RemoveOutputsByIds(ObsOutputType outputType,
			   CefRefPtr<CefValue> input,
			   CefRefPtr<CefValue> &output);
	void EnableOutputsByIds(ObsOutputType outputType,
			   CefRefPtr<CefValue> input,
			   CefRefPtr<CefValue> &output);
	void DisableOutputsByIds(ObsOutputType outputType,
			    CefRefPtr<CefValue> input,
			    CefRefPtr<CefValue> &output);

	void TriggerSplitRecordingOutputById(CefRefPtr<CefValue> input,
					     CefRefPtr<CefValue> &output);

	void TriggerSaveReplayBufferById(CefRefPtr<CefValue> input,
					 CefRefPtr<CefValue> &output);

	void Reset();

private:
	bool GetValidIds(ObsOutputType outputType,
			 CefRefPtr<CefValue> input,
			 std::map<std::string, bool> &output, bool testRemove,
			 bool testDisable);
};
