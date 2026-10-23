//
// Behavioural tests for the session message signature scheme.
//
// This digest is not a checksum: it is the whole of the access control on
// SELocalFilesystemHttpServer, which opens and serves any absolute path a
// request names. The only thing standing between a request and an arbitrary
// file read is that the caller could not have produced the digest without
// the per-process salt.
//
// The production .cpp is compiled beside the test. The salt's random source
// is not: CreateCryptoSecureRandomNumberString() is declared in the
// production header and defined per platform in files that need the Windows
// SDK or Objective-C, so the test supplies its own definition at link time.
// That also makes the salt deterministic here, which is what lets these
// tests say anything exact about the digests.
//
// Test order matters in this file and main() keeps it: the salt is a
// function-local static, so "what does verification do before anything has
// signed?" can only be asked once per process, and it has to be asked
// first.
//

#include "streamelements/SEUtils/SECryptoUtils.hpp"

#include <cctype>
#include <cstdio>
#include <string>

static int failures = 0;

static void check(bool cond, const char *msg)
{
	if (!cond) {
		std::fprintf(stderr, "FAIL: %s\n", msg);
		++failures;
	}
}

//
// The link seam. Four of these are concatenated into the salt, so the salt
// in this process is this string four times over -- known, but never
// assumed below: every expectation is computed through CreateSHA256Digest()
// rather than written out as a literal.
//
std::string CreateCryptoSecureRandomNumberString()
{
	return "A1B2C3D4E5F60718";
}

static std::string digest_of(std::string input)
{
	return CreateSHA256Digest(input);
}

/* --------------------------------------------------------------- the bug */

//
// Must run first. CreateSessionMessageSignature() used to be the only
// function that initialised the salt, through a double-checked lock of its
// own; verification read the same file static without it. So before
// anything had signed a URL the salt was the empty string, and the digest
// verification computed was SHA256(message) -- a value anyone who knows the
// path can produce.
//
// Nothing signs a URL until a page asks for an asset (a backup listing, a
// scene-data query, a WYVRN asset enumeration), so this was not a narrow
// start-up window: a session where none of those ran accepted forged
// digests for its entire life.
//
static void test_verification_before_any_signing_rejects_an_unsalted_digest()
{
	std::string message = "C:/Windows/win.ini";
	std::string unsalted = digest_of(message);

	check(!VerifySessionMessageSignature(message, unsalted),
	      "a digest taken over the bare message verified -- the salt was "
	      "not initialised by the verification path, so the signature "
	      "could be computed by anyone who knows the path");
}

/* ------------------------------------------------------------- the digest */

static void test_sha256_known_vector()
{
	std::string abc = "abc";

	check(digest_of(abc) ==
		      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff"
		      "61f20015ad",
	      "CreateSHA256Digest does not produce the standard SHA-256 of "
	      "\"abc\" in lower-case hex");

	std::string empty = "";

	check(digest_of(empty) ==
		      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991"
		      "b7852b855",
	      "CreateSHA256Digest does not produce the standard SHA-256 of "
	      "the empty string");
}

/* ---------------------------------------------------------- the round trip */

static void test_sign_then_verify()
{
	const char *messages[] = {
		"",
		"C:/Users/someone/AppData/Roaming/obs-studio/basic/scenes.json",
		"a path with spaces and \xC3\xA9\xC3\xA8 accents",
		"a%20percent%20encoded%20looking%20path",
	};

	for (const char *m : messages) {
		std::string message = m;
		std::string signature =
			CreateSessionMessageSignature(message);

		check(VerifySessionMessageSignature(message, signature),
		      "a signature this process produced did not verify");
	}
}

static void test_the_salt_participates()
{
	std::string message = "C:/Windows/win.ini";

	std::string signature = CreateSessionMessageSignature(message);

	check(signature != digest_of(message),
	      "the signature equals the unsalted digest of the message, so "
	      "the salt is empty or is not being mixed in");

	//
	// Same salt for the life of the process: two signings of one message
	// have to agree, or a URL signed early stops verifying later.
	//
	std::string again = CreateSessionMessageSignature(message);

	check(signature == again,
	      "signing the same message twice gave different digests, so the "
	      "salt is not stable for the process");
}

static void test_mismatches_are_rejected()
{
	std::string message = "C:/Windows/win.ini";
	std::string other = "C:/Windows/system.ini";

	std::string signature = CreateSessionMessageSignature(message);
	std::string otherSignature = CreateSessionMessageSignature(other);

	check(!VerifySessionMessageSignature(other, signature),
	      "a signature for one path verified another path");

	check(!VerifySessionMessageSignature(message, otherSignature),
	      "a signature for another path verified this one");

	std::string empty = "";

	check(!VerifySessionMessageSignature(message, empty),
	      "an empty signature verified");

	//
	// The digest is lower-case hex. An upper-case spelling of the same
	// value is a different string and must not pass -- this pins the
	// comparison as exact rather than case-insensitive, which is what a
	// caller reading "digest" off a URL query parameter will hand in.
	//
	std::string upper = signature;

	for (char &c : upper)
		c = (char)std::toupper((unsigned char)c);

	if (upper != signature) {
		check(!VerifySessionMessageSignature(message, upper),
		      "an upper-case spelling of the digest verified");
	}
}

int main()
{
	//
	// First, and deliberately: see the comment on the function.
	//
	test_verification_before_any_signing_rejects_an_unsalted_digest();

	test_sha256_known_vector();
	test_sign_then_verify();
	test_the_salt_participates();
	test_mismatches_are_rejected();

	if (failures) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);

		return 1;
	}

	std::printf("all session signature tests passed\n");

	return 0;
}
