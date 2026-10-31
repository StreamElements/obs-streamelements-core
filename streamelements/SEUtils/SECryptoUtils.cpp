#include "SECryptoUtils.hpp"

#include "../deps/picosha2/picosha2.h"

#include <vector>

//
// One salt for the process, built on first use by whichever of signing
// and verification gets there first.
//
// It used to be the file static below this comment, lazily filled by a
// hand-written double-checked lock inside CreateSessionMessageSignature()
// -- and only there. VerifySessionMessageSignature() read the same
// variable without ever initialising it, so until something had signed a
// URL the salt was the empty string and a digest taken over the bare
// message verified. That digest is the whole of the access control on
// SELocalFilesystemHttpServer, which serves any absolute path handed to
// it, and nothing signs a URL until a page asks for an asset -- so a
// session in which that never happened accepted forged digests for its
// whole run.
//
// A function-local static answers both halves. It is initialised exactly
// once, by the first caller whichever one that is, and C++11 makes other
// threads wait for that initialisation rather than read a std::string
// mid-assignment -- which is what the outer, unlocked check of the old
// double-checked lock did.
//
// Four draws because this is a secret rather than an identifier, and
// CreateCryptoSecureRandomNumberString() yields one 64-bit value per call
// on Windows.
//
static const std::string &SessionMessageSignatureSalt()
{
	static const std::string salt =
		CreateCryptoSecureRandomNumberString() +
		CreateCryptoSecureRandomNumberString() +
		CreateCryptoSecureRandomNumberString() +
		CreateCryptoSecureRandomNumberString();

	return salt;
}

std::string CreateSHA256Digest(std::string &input)
{
	std::vector<unsigned char> hash(picosha2::k_digest_size);
	picosha2::hash256(input.begin(), input.end(), hash.begin(), hash.end());

	return picosha2::bytes_to_hex_string(hash.begin(), hash.end());
}

std::string CreateSessionMessageSignature(std::string &message)
{
	std::string digest_input = message + SessionMessageSignatureSalt();

	return CreateSHA256Digest(digest_input);
}

bool VerifySessionMessageSignature(std::string &message, std::string &signature)
{
	const std::string &salt = SessionMessageSignatureSalt();

	//
	// Fails closed. An empty salt means the random source gave us
	// nothing, and a digest anyone can recompute from the path alone is
	// not a signature -- better to serve nothing than to serve any file
	// that is asked for.
	//
	if (!salt.size())
		return false;

	std::string digest_input = message + salt;

	std::string digest = CreateSHA256Digest(digest_input);

	return digest == signature;
}
