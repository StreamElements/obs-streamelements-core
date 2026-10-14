#pragma once

//
// Hashing and the session message signature scheme.
//
// CreateCryptoSecureRandomNumberString is declared here but still defined per
// platform where it always was -- the Windows half in SEUtils.cpp against
// bcrypt, the macOS half in SEUtils.mm. Moving those two bodies is a separate
// step; the declaration belongs with the rest of the crypto surface now.
//

#include <string>

std::string CreateCryptoSecureRandomNumberString();

std::string CreateSHA256Digest(std::string &input);
std::string CreateSessionMessageSignature(std::string &message);
bool VerifySessionMessageSignature(std::string &message,
				   std::string &signature);
