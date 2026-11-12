#include "SEReplayBufferHotkey.hpp"
#include "SEReplayBufferHotkeyNaming.hpp"
#include "SEUtils.hpp"

#include <obs-module.h>
#include <util/bmem.h>
#include <util/platform.h>

static const char *HOTKEY_BINDINGS_CHANGED_SIGNAL = "hotkey_bindings_changed";
static const char *HOTKEY_DATA_KEY = "bindings";

std::string SEReplayBufferHotkey::BuildStoragePath(const std::string &outputId)
{
	char *dir = obs_module_config_path("replay-buffer-hotkeys");

	// Null when the module config directory is unavailable. Then the
	// hotkey still works for this session; it just is not remembered.
	if (!dir)
		return std::string();

	os_mkdirs(dir);

	std::string path = std::string(dir) + "/" +
			   SEReplayBufferHotkeyFileStem(outputId) + ".json";

	bfree(dir);

	return path;
}

void SEReplayBufferHotkey::SaveLocked(State &state)
{
	if (!state.alive || state.path.empty() ||
	    state.id == OBS_INVALID_HOTKEY_ID)
		return;

	obs_data_array_t *bindings = obs_hotkey_save(state.id);

	if (!bindings)
		return;

	obs_data_t *root = obs_data_create();

	obs_data_set_array(root, HOTKEY_DATA_KEY, bindings);
	obs_data_save_json_safe(root, state.path.c_str(), "tmp", "bak");

	obs_data_release(root);
	obs_data_array_release(bindings);
}

void SEReplayBufferHotkey::LoadLocked(State &state)
{
	if (state.path.empty() || state.id == OBS_INVALID_HOTKEY_ID)
		return;

	obs_data_t *root =
		obs_data_create_from_json_file(state.path.c_str());

	if (!root)
		return;

	obs_data_array_t *bindings = obs_data_get_array(root, HOTKEY_DATA_KEY);

	if (bindings) {
		obs_hotkey_load(state.id, bindings);

		obs_data_array_release(bindings);
	}

	obs_data_release(root);
}

void SEReplayBufferHotkey::hotkey_pressed(void *data, obs_hotkey_id,
					  obs_hotkey_t *, bool pressed)
{
	auto self = static_cast<SEReplayBufferHotkey *>(data);

	if (!pressed || !self || !self->m_onPressed)
		return;

	self->m_onPressed();
}

void SEReplayBufferHotkey::bindings_changed(void *data, calldata_t *param)
{
	auto self = static_cast<SEReplayBufferHotkey *>(data);

	if (!self)
		return;

	auto key = static_cast<obs_hotkey_t *>(calldata_ptr(param, "key"));

	if (!key)
		return;

	std::shared_ptr<State> state = self->m_state;

	if (obs_hotkey_get_id(key) != state->id)
		return;

	//
	// This signal can be raised while libobs holds its hotkey lock, and
	// obs_hotkey_save() takes that lock. Save from the UI thread instead.
	//
	QtPostTask([state]() {
		std::lock_guard<std::mutex> guard(state->mutex);

		SaveLocked(*state);
	});
}

SEReplayBufferHotkey::SEReplayBufferHotkey(const std::string &outputId,
					   const std::string &outputName,
					   Callback onPressed)
	: m_onPressed(std::move(onPressed))
{
	std::lock_guard<std::mutex> guard(m_state->mutex);

	m_state->path = BuildStoragePath(outputId);

	const std::string description = FormatString(
		obs_module_text("StreamElements.Output.ReplayBuffer.SaveHotkey"),
		outputName.c_str());

	m_state->id = obs_hotkey_register_frontend(
		SEReplayBufferHotkeyName(outputId).c_str(), description.c_str(),
		hotkey_pressed, this);

	// Restore before connecting: loading raises the changed signal, and
	// there is nothing to write back for what was just read.
	LoadLocked(*m_state);

	signal_handler_connect(obs_get_signal_handler(),
			       HOTKEY_BINDINGS_CHANGED_SIGNAL,
			       bindings_changed, this);
	m_signalConnected = true;
}

SEReplayBufferHotkey::~SEReplayBufferHotkey()
{
	if (m_signalConnected) {
		signal_handler_disconnect(obs_get_signal_handler(),
					  HOTKEY_BINDINGS_CHANGED_SIGNAL,
					  bindings_changed, this);
	}

	std::lock_guard<std::mutex> guard(m_state->mutex);

	// Final write: catches a rebind whose deferred save has not run yet.
	SaveLocked(*m_state);

	m_state->alive = false;

	if (m_state->id != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(m_state->id);
		m_state->id = OBS_INVALID_HOTKEY_ID;
	}
}
