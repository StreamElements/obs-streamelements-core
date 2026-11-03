#pragma once

#include "cef-headers.hpp"

#include "SEObsOutputType.hpp"

//
// What the host API needs from the output manager: SE.Live's own
// streaming, recording and replay-buffer outputs -- created, removed,
// enabled, disabled, enumerated, and the two trigger methods.
//
// Six of the eight take an ObsOutputType the page never sees: eighteen
// handlers map onto these six, differing only by that constant. It is
// why the enum has a header of its own rather than living in
// SEOutput.hpp behind twenty-one external includes.
//
// Reset() is here because SESessionManager calls it when signing out,
// and that is the rule for this whole set: what only the root calls
// stays off the contract, and this one has a second consumer.
//
// Not OBS's own native stream -- that is ISEHostStreamingManager.
//
class ISEOutputManager {
public:
	virtual ~ISEOutputManager() {}

	//
	// Every output of that kind, with its settings and state.
	//
	virtual void SerializeAllOutputs(ObsOutputType outputType,
					 CefRefPtr<CefValue> &output) = 0;

	//
	// Creates one of that kind from a serialized description.
	//
	virtual void DeserializeOutput(ObsOutputType outputType,
				       CefRefPtr<CefValue> input,
				       CefRefPtr<CefValue> &output) = 0;

	//
	// Removes the listed outputs of that kind.
	//
	virtual void RemoveOutputsByIds(ObsOutputType outputType,
					CefRefPtr<CefValue> input,
					CefRefPtr<CefValue> &output) = 0;

	//
	// Enables them.
	//
	virtual void EnableOutputsByIds(ObsOutputType outputType,
					CefRefPtr<CefValue> input,
					CefRefPtr<CefValue> &output) = 0;

	//
	// Disables them.
	//
	virtual void DisableOutputsByIds(ObsOutputType outputType,
					 CefRefPtr<CefValue> input,
					 CefRefPtr<CefValue> &output) = 0;

	//
	// Saves the replay buffer of one output, named by id.
	//
	virtual void
	TriggerSaveReplayBufferById(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output) = 0;

	//
	// Splits the recording file of one output, named by id.
	//
	virtual void
	TriggerSplitRecordingOutputById(CefRefPtr<CefValue> input,
					CefRefPtr<CefValue> &output) = 0;

	//
	// Drops every non-native output. Called when signing out and not only
	// by the root, which is why it is on this contract.
	//
	virtual void Reset() = 0;
};
