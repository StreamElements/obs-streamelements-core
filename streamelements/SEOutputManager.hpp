#pragma once

#include "SEObsOutputType.hpp"
#include "cef-headers.hpp"

#include <map>
#include <memory>
#include <shared_mutex>
#include <string>

//
// Forward declarations rather than includes. Each of these is held only
// through a shared_ptr and this class's destructor is defined in the .cpp,
// which is what lets the types stay incomplete here. Including them instead
// cost twenty-one external headers -- obs.h, obs-frontend-api.h and
// nineteen Qt ones -- for three pointers, with no member of any of them
// named in this file (CORE-757 part 2).
//
class SEOutputBase;
class SEVideoCompositionManager;
class SEAudioCompositionManager;
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
