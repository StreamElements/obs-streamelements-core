#pragma once

#include "SEOutput.hpp"
#include "SEVideoCompositionManager.hpp"
#include "SEAudioCompositionManager.hpp"

#include <shared_mutex>

class SEHostEventDispatcher;

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

	//
	// Handed to every output this manager creates, because an output
	// announces its own start, stop, error and list changes.
	//
	std::shared_ptr<SEHostEventDispatcher> m_hostEventDispatcher;

public:
	SEOutputManager(
		std::shared_ptr<SEVideoCompositionManager>
			videoCompositionManager,
		std::shared_ptr<SEAudioCompositionManager>
			audioCompositionManager,
		std::shared_ptr<SEHostEventDispatcher> hostEventDispatcher);
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
