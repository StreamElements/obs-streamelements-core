#include "SECryptoUtils.hpp"

#include "../deps/picosha2/picosha2.h"

#include <mutex>
#include <vector>

static std::mutex s_session_message_signature_mutex;
static std::string s_session_message_signature_random = "";
std::string CreateSHA256Digest(std::string &input)
{
	std::vector<unsigned char> hash(picosha2::k_digest_size);
	picosha2::hash256(input.begin(), input.end(), hash.begin(), hash.end());

	return picosha2::bytes_to_hex_string(hash.begin(), hash.end());
}

std::string CreateSessionMessageSignature(std::string &message)
{
	if (!s_session_message_signature_random.size()) {
		std::lock_guard<std::mutex> guard(
			s_session_message_signature_mutex);
		if (!s_session_message_signature_random.size()) {
			s_session_message_signature_random =
				CreateCryptoSecureRandomNumberString();
		}
	}

	std::string digest_input = message + s_session_message_signature_random;

	return CreateSHA256Digest(digest_input);
}

bool VerifySessionMessageSignature(std::string &message, std::string &signature)
{
	std::string digest_input = message + s_session_message_signature_random;

	std::string digest = CreateSHA256Digest(digest_input);

	return digest == signature;
}
