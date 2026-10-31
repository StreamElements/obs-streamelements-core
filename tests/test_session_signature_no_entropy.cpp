//
// The one branch test_session_signature.cpp cannot reach: what verification
// does when the salt is unusable.
//
// The salt is a function-local static, so a process gets exactly one of
// them, and "the random source returned nothing" is therefore a different
// process rather than a different test case. Hence a second executable over
// the same production .cpp, whose only difference is the link seam below.
//
// The property is that verification fails closed. An empty salt makes the
// digest SHA256(message), which anyone who knows the path can compute, so
// the right answer is to verify nothing at all rather than to accept a
// value that is no longer a secret. SELocalFilesystemHttpServer serves any
// absolute path whose digest checks out, so the alternative is serving any
// file that is asked for.
//
// Not reachable on the platforms we ship today -- the Windows body returns
// "0" on failure and the macOS body cannot fail -- which is exactly why it
// is worth pinning here: it is the kind of defensive branch that rots
// unnoticed until the day something changes underneath it.
//

#include "streamelements/SEUtils/SECryptoUtils.hpp"

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
// The link seam, returning nothing -- a random source that has failed.
//
std::string CreateCryptoSecureRandomNumberString()
{
	return std::string();
}

int main()
{
	std::string message = "C:/Windows/win.ini";

	//
	// The digest verification would compute with an empty salt, i.e. the
	// one an attacker can produce from the path alone.
	//
	std::string unsalted = CreateSHA256Digest(message);

	check(!VerifySessionMessageSignature(message, unsalted),
	      "with no salt, a digest over the bare message verified: "
	      "verification must fail closed rather than accept a digest "
	      "anyone can compute");

	//
	// And it rejects everything else too, including what this process's
	// own signing produces -- nothing is servable when the secret is
	// gone.
	//
	std::string signature = CreateSessionMessageSignature(message);

	check(!VerifySessionMessageSignature(message, signature),
	      "with no salt, verification accepted a signature: it must "
	      "reject every signature, including ones this process made");

	if (failures) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);

		return 1;
	}

	std::printf("all no-entropy session signature tests passed\n");

	return 0;
}
