#include "StreamElementsRazerWyvrnConfig.hpp"

#include <algorithm>
#include <cctype>

/* ========================================================================= */

namespace {

std::string ToLower(const std::string &s)
{
	std::string out;
	out.reserve(s.size());

	for (unsigned char c : s)
		out.push_back((char)std::tolower(c));

	return out;
}

//
// Small readers that answer "absent" and "wrong type" the same way, with the
// caller's default.
//
// The configs are hand-authored and third-party, so a field being missing or
// carrying an unexpected type is routine rather than exceptional. Every read
// goes through these so no single malformed value can abort a scan.
//
std::string ReadString(CefRefPtr<CefDictionaryValue> d, const char *key,
		       const std::string &fallback = "")
{
	if (!d.get() || !d->HasKey(key) || d->GetType(key) != VTYPE_STRING)
		return fallback;

	return d->GetString(key).ToString();
}

bool ReadBool(CefRefPtr<CefDictionaryValue> d, const char *key,
	      bool fallback = false)
{
	if (!d.get() || !d->HasKey(key))
		return fallback;

	if (d->GetType(key) == VTYPE_BOOL)
		return d->GetBool(key);

	// "Interrupt": 0 / 1 appears as well as true / false.
	if (d->GetType(key) == VTYPE_INT)
		return d->GetInt(key) != 0;

	return fallback;
}

int ReadInt(CefRefPtr<CefDictionaryValue> d, const char *key, int fallback = 0)
{
	if (!d.get() || !d->HasKey(key))
		return fallback;

	if (d->GetType(key) == VTYPE_INT)
		return d->GetInt(key);

	if (d->GetType(key) == VTYPE_DOUBLE)
		return (int)d->GetDouble(key);

	if (d->GetType(key) == VTYPE_BOOL)
		return d->GetBool(key) ? 1 : 0;

	return fallback;
}

double ReadDouble(CefRefPtr<CefDictionaryValue> d, const char *key,
		  double fallback = 0.0)
{
	if (!d.get() || !d->HasKey(key))
		return fallback;

	// "Gain" is written both as 1 and as 1.0 across the shipped configs.
	if (d->GetType(key) == VTYPE_DOUBLE)
		return d->GetDouble(key);

	if (d->GetType(key) == VTYPE_INT)
		return (double)d->GetInt(key);

	return fallback;
}

CefRefPtr<CefListValue> ReadList(CefRefPtr<CefDictionaryValue> d,
				 const char *key)
{
	if (!d.get() || !d->HasKey(key) || d->GetType(key) != VTYPE_LIST)
		return nullptr;

	return d->GetList(key);
}

CefRefPtr<CefDictionaryValue> DictAt(CefRefPtr<CefListValue> list, size_t index)
{
	if (!list.get() || index >= list->GetSize())
		return nullptr;

	if (list->GetType(index) != VTYPE_DICTIONARY)
		return nullptr;

	return list->GetDictionary(index);
}

std::vector<StreamElementsRazerWyvrnHapticTarget>
ParseTargeting(CefRefPtr<CefDictionaryValue> hapticEvent)
{
	std::vector<StreamElementsRazerWyvrnHapticTarget> result;

	auto list = ReadList(hapticEvent, "Targeting");
	if (!list.get())
		return result;

	for (size_t i = 0; i < list->GetSize(); ++i) {
		auto entry = DictAt(list, i);
		if (!entry.get())
			continue;

		StreamElementsRazerWyvrnHapticTarget target;
		target.target = ReadString(entry, "Target");
		target.spatialization =
			ReadString(entry, "Spatialization", "Global");
		target.gain = ReadDouble(entry, "Gain", 1.0);

		// An entry with no Target says nothing about where the effect
		// lands, so it is dropped rather than passed on as a blank.
		if (!target.target.empty())
			result.push_back(std::move(target));
	}

	return result;
}

void ParseCommandList(CefRefPtr<CefDictionaryValue> root, const char *key,
		      const char *kind, const std::string &sourceName,
		      std::vector<StreamElementsRazerWyvrnEventInfo> &output)
{
	auto commands = ReadList(root, key);
	if (!commands.get())
		return;

	for (size_t i = 0; i < commands->GetSize(); ++i) {
		auto command = DictAt(commands, i);
		if (!command.get())
			continue;

		StreamElementsRazerWyvrnEventInfo info;
		info.id = ReadString(command, "External_Command_ID");
		info.source = sourceName;
		info.kind = kind;

		// Without an id there is nothing to name, so the entry is
		// unusable whatever else it carries.
		if (info.id.empty())
			continue;

		auto chromaEvents = ReadList(command, "Chroma_Events");
		if (chromaEvents.get()) {
			for (size_t c = 0; c < chromaEvents->GetSize(); ++c) {
				auto entry = DictAt(chromaEvents, c);
				if (!entry.get())
					continue;

				StreamElementsRazerWyvrnChromaComponent chroma;
				chroma.effect =
					ReadString(entry, "Chroma_Effect");
				chroma.interrupt = ReadBool(entry, "Interrupt");

				if (!chroma.effect.empty())
					info.chroma.push_back(
						std::move(chroma));
			}
		}

		auto hapticEvents = ReadList(command, "Haptic_Events");
		if (hapticEvents.get()) {
			for (size_t h = 0; h < hapticEvents->GetSize(); ++h) {
				auto entry = DictAt(hapticEvents, h);
				if (!entry.get())
					continue;

				StreamElementsRazerWyvrnHapticComponent haptic;
				haptic.effect =
					ReadString(entry, "Haptic_Effect");
				haptic.loop = ReadInt(entry, "Loop");
				haptic.mixing = ReadString(entry, "Mixing");
				haptic.priority = ReadString(entry, "Priority");
				haptic.targeting = ParseTargeting(entry);

				if (!haptic.effect.empty())
					info.haptics.push_back(
						std::move(haptic));
			}
		}

		output.push_back(std::move(info));
	}
}

} // namespace

/* ========================================================================= */

bool IsRazerWyvrnConfigFileName(const std::string &fileName)
{
	// Three casings exist on a stock machine: wyvrn.config x121,
	// Wyvrn.config x12, WYVRN.config x13. A case-sensitive comparison finds
	// 13 of 146 and silently loses the rest.
	return ToLower(fileName) == "wyvrn.config";
}

std::vector<StreamElementsRazerWyvrnEventInfo>
ParseRazerWyvrnConfig(const std::string &json, const std::string &sourceName)
{
	std::vector<StreamElementsRazerWyvrnEventInfo> result;

	if (json.empty())
		return result;

	auto parsed = CefParseJSON(CefString(json),
				   JSON_PARSER_ALLOW_TRAILING_COMMAS);

	// Unreadable JSON yields nothing. One bad file out of 146 must not stop
	// a scan, and there is no useful recovery beyond skipping it.
	if (!parsed.get() || parsed->GetType() != VTYPE_DICTIONARY)
		return result;

	auto root = parsed->GetDictionary();
	if (!root.get())
		return result;

	// A document with neither key is the audio-to-haptic profile schema -
	// 124 of the 146 files on a stock machine. It declares no events, which
	// is a normal outcome and not a parse failure.
	ParseCommandList(root, "ExternalCommands", "exact", sourceName, result);
	ParseCommandList(root, "FallbackCommands", "fallbackPattern",
			 sourceName, result);

	return result;
}

size_t CountRazerWyvrnEventsOfKind(
	const std::vector<StreamElementsRazerWyvrnEventInfo> &events,
	const std::string &kind)
{
	return (size_t)std::count_if(
		events.begin(), events.end(),
		[&](const StreamElementsRazerWyvrnEventInfo &e) {
			return e.kind == kind;
		});
}
