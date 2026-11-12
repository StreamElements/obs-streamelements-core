#pragma once

#include <obs.h>

#include <functional>
#include <memory>
#include <mutex>
#include <string>

//
// The "Save replay" hotkey of one SE.Live Backtrack (custom replay buffer)
// output.
//
// OBS's own "Save Replay" hotkey only reaches OBS's native replay buffer. A
// SE.Live canvas has its own buffer, so without this the only way to save it
// was the button in the SE.Live dock. This registers a frontend hotkey --
// listed under Settings > Hotkeys -- that runs the given callback on press.
//
// OBS persists only the hotkeys it registered itself, so the bindings are
// stored by this class, one small JSON file per output id under the module
// config directory (replay-buffer-hotkeys/), and reloaded on construction.
//
class SEReplayBufferHotkey {
public:
	using Callback = std::function<void()>;

private:
	//
	// Shared with the deferred save task, which can outlive this object.
	//
	struct State {
		std::mutex mutex;
		bool alive = true;
		obs_hotkey_id id = OBS_INVALID_HOTKEY_ID;
		std::string path;
	};

	std::shared_ptr<State> m_state = std::make_shared<State>();
	Callback m_onPressed;
	bool m_signalConnected = false;

public:
	SEReplayBufferHotkey(const std::string &outputId,
			     const std::string &outputName,
			     Callback onPressed);
	~SEReplayBufferHotkey();

	SEReplayBufferHotkey(const SEReplayBufferHotkey &) = delete;
	SEReplayBufferHotkey &operator=(const SEReplayBufferHotkey &) = delete;

private:
	static void hotkey_pressed(void *data, obs_hotkey_id id,
				   obs_hotkey_t *hotkey, bool pressed);
	static void bindings_changed(void *data, calldata_t *param);

	static std::string BuildStoragePath(const std::string &outputId);
	static void SaveLocked(State &state);
	static void LoadLocked(State &state);
};
