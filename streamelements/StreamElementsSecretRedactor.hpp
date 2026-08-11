#pragma once

#include <string>

//
// Removes streaming credentials from OBS configuration files before they leave
// the machine.
//
// Both diagnostics paths -- the crash report built by StreamElementsCrashContext
// and the user-initiated report built by StreamElementsReportIssueDialog -- zip
// up the entire obs-studio configuration tree. That tree includes
// basic/profiles/<name>/service.json, where OBS stores the stream key in
// plaintext. Those archives are uploaded to a third party, and neither BugSplat
// nor Sentry scrubs attachments.
//
// The file is redacted in place rather than excluded: which service is
// configured, whether authentication is enabled, the server URL and the encoder
// settings are all genuinely useful when diagnosing a streaming problem, and
// dropping the file loses all of it to protect one field.
//
// Redaction is deliberately a targeted byte replacement and not a JSON
// round-trip. Parsing and re-serialising would reformat the document, making
// diffs between a user's file and a known-good one useless, and -- more
// importantly -- Redact() runs on a dying thread in the crash path, where
// invoking a full JSON parser is exactly the sort of thing to avoid. Every byte
// that is not part of a credential value comes out where it went in.
//
class StreamElementsSecretRedactor {
public:
	// True when `zipPath` names a file known to carry credentials. Matching
	// is case-insensitive and accepts either slash direction.
	//
	// Redact() is only ever applied to paths this accepts. That containment
	// is what makes it safe to treat a field as generic as "key" as a
	// secret: in service.json it is the stream key, whereas elsewhere in the
	// configuration tree it would mean something else entirely.
	static bool IsSensitivePath(const std::wstring &zipPath);

	// Returns `content` with credential values replaced by [redacted].
	//
	// Redacted: the string values of "key", "password" and "bearer_token",
	// plus the userinfo component of "server" when the URL carries inline
	// credentials (rtmp://user:pass@host/...).
	//
	// This recognises patterns, it does not validate: any `"name": "value"`
	// pair it can see is redacted regardless of how it is nested or whether
	// the surrounding document is well formed. The consequence worth
	// knowing is the inverse -- a credential stored in a shape this does not
	// recognise, such as a non-string value or a key spelled differently by
	// a future OBS version, passes through untouched.
	static std::string Redact(const std::string &content);
};
