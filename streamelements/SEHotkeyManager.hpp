#pragma once

#include "ISEHotkeyManager.hpp"

#include "obs.h"
#include "obs-hotkey.h"
#include "cef-headers.hpp"

#include <map>
#include <memory>
#include <mutex>

class SEHostEventDispatcher;

class SEHotkeyManager : public ISEHotkeyManager {
public:
	//
	// The dispatcher is injected: a hotkey press and a change to the
	// bindings are both announced to pages, and fetching the server for
	// it was this class's only use of global state. The one static
	// callback below is already handed `this`, so it needs no plumbing
	// of its own.
	//
	SEHotkeyManager(
		std::shared_ptr<SEHostEventDispatcher> hostEventDispatcher);
	~SEHotkeyManager();

	bool SerializeHotkeyBindings(CefRefPtr<CefValue> &result,
				     bool onlyManagedBindings) override;
	bool DeserializeHotkeyBindings(CefRefPtr<CefValue> input);
	bool DeserializeHotkeyTriggers(CefRefPtr<CefValue> input) override;

	//
	// The new binding's id, or -1. It returned obs_hotkey_id, whose
	// invalid value is OBS_INVALID_HOTKEY_ID -- so the handler body held
	// a libobs type and compared against a libobs constant to decide
	// whether to answer the page with a number or with null.
	//
	int64_t
	DeserializeSingleHotkeyBinding(CefRefPtr<CefValue> input) override;

	bool RemoveHotkeyBindingById(uint64_t id) override;

private:
	bool RemoveHotkeyBindingByObsId(obs_hotkey_id id);

public:
	void RemoveAllManagedHotkeyBindings() override;

protected:
	virtual void hotkeyTriggered(obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

private:
	std::recursive_mutex m_mutex;

	std::map<std::string, obs_hotkey_id> m_registeredHotkeyNamesToHotkeyIds;
	std::map<obs_hotkey_id, std::string> m_registeredHotkeyIdsToNames;
	std::map<obs_hotkey_id, CefRefPtr<CefValue>> m_registeredHotkeySerializedValues;
	std::map<obs_hotkey_id, std::string> m_registeredHotkeyDataString;

	std::shared_ptr<SEHostEventDispatcher> m_hostEventDispatcher;

private:
	static void hotkey_change_handler(void *data, calldata_t *);
};
