#include "StreamElementsSecretRedactor.hpp"

#include <algorithm>
#include <cctype>
#include <cwctype>

/* ================================================================= */

static const char *const REDACTED = "[redacted]";

//
// Keys whose string value is a credential in its entirety.
//
// "key" is the stream key for rtmp_common services; "password" is the custom
// RTMP auth password; "bearer_token" is used by services authenticating with a
// token rather than a key.
//
static bool IsSecretKeyName(const std::string &name)
{
	return name == "key" || name == "password" || name == "bearer_token";
}

//
// Finds the closing quote of a JSON string whose contents start at `start`,
// honouring backslash escapes. Returns npos when the string is unterminated.
//
static size_t FindClosingQuote(const std::string &s, size_t start)
{
	for (size_t i = start; i < s.size(); ++i) {
		if (s[i] == '\\') {
			++i; // skip the escaped character

			continue;
		}

		if (s[i] == '"')
			return i;
	}

	return std::string::npos;
}

//
// Replaces the userinfo component of a URL, if it has one.
//
// OBS stores custom RTMP endpoints as a plain URL, and a user pasting one from
// their provider can easily bring credentials along:
//
//     rtmp://user:pass@live.example.com/app
//
// The host and path stay -- they say which provider and ingest endpoint were in
// use, which is the diagnostically interesting part.
//
static bool RedactUrlUserInfo(const std::string &url, std::string &result)
{
	const size_t schemeEnd = url.find("://");

	if (schemeEnd == std::string::npos)
		return false;

	const size_t authorityStart = schemeEnd + 3;

	// The authority ends at the first '/', '?' or '#'.
	size_t authorityEnd = url.size();

	for (size_t i = authorityStart; i < url.size(); ++i) {
		if (url[i] == '/' || url[i] == '?' || url[i] == '#') {
			authorityEnd = i;

			break;
		}
	}

	const size_t at = url.rfind('@', authorityEnd);

	if (at == std::string::npos || at < authorityStart)
		return false;

	result = url.substr(0, authorityStart) + REDACTED +
		 url.substr(at, url.size() - at);

	return true;
}

/* ================================================================= */

bool StreamElementsSecretRedactor::IsSensitivePath(const std::wstring &zipPath)
{
	std::wstring normalized = zipPath;

	std::transform(normalized.begin(), normalized.end(), normalized.begin(),
		       ::towlower);
	std::transform(normalized.begin(), normalized.end(), normalized.begin(),
		       [](wchar_t ch) { return ch == L'\\' ? L'/' : ch; });

	// basic/profiles/<profile name>/service.json, wherever it sits inside
	// the archive.
	if (normalized.find(L"basic/profiles/") == std::wstring::npos)
		return false;

	const std::wstring suffix = L"/service.json";

	if (normalized.size() < suffix.size())
		return false;

	return normalized.compare(normalized.size() - suffix.size(),
				  suffix.size(), suffix) == 0;
}

std::string StreamElementsSecretRedactor::Redact(const std::string &content)
{
	std::string result;
	result.reserve(content.size() + 64);

	size_t i = 0;

	while (i < content.size()) {
		if (content[i] != '"') {
			result += content[i++];

			continue;
		}

		// A quoted token. It is a key if a colon follows it.
		const size_t nameStart = i + 1;
		const size_t nameEnd = FindClosingQuote(content, nameStart);

		if (nameEnd == std::string::npos) {
			// Unterminated: copy the remainder verbatim.
			result.append(content, i, std::string::npos);

			break;
		}

		const std::string name =
			content.substr(nameStart, nameEnd - nameStart);

		size_t cursor = nameEnd + 1;

		while (cursor < content.size() &&
		       isspace((unsigned char)content[cursor]))
			++cursor;

		bool handled = false;

		if (cursor < content.size() && content[cursor] == ':') {
			++cursor;

			while (cursor < content.size() &&
			       isspace((unsigned char)content[cursor]))
				++cursor;

			if (cursor < content.size() && content[cursor] == '"') {
				const size_t valueStart = cursor + 1;
				const size_t valueEnd =
					FindClosingQuote(content, valueStart);

				if (valueEnd != std::string::npos) {
					const std::string value =
						content.substr(
							valueStart,
							valueEnd - valueStart);

					std::string replacement;
					bool redact = false;

					if (IsSecretKeyName(name)) {
						replacement = REDACTED;
						redact = true;
					} else if (name == "server") {
						redact = RedactUrlUserInfo(
							value, replacement);
					}

					if (redact) {
						// Everything up to and including
						// the value's opening quote is
						// copied byte for byte.
						result.append(content, i,
							      valueStart - i);
						result += replacement;
						result += '"';

						i = valueEnd + 1;
						handled = true;
					}
				}
			}
		}

		if (!handled) {
			// Copy the quoted token and carry on from just after
			// it, so its contents are never rescanned as keys.
			result.append(content, i, nameEnd + 1 - i);

			i = nameEnd + 1;
		}
	}

	return result;
}
