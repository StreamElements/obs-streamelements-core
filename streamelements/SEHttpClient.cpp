#include "SEHttpClient.hpp"
#include "SEUtils.hpp"

#include <string>

void SEHttpClient::DeserializeHttpRequestText(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue> output)
{
	bool success = false;
	std::wstring result_string = L"";
	std::string error_string = "";
	int http_status_code = 0;

	if (input->GetType() == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

		// Type-checked as well as present. HasKey answers for
		// presence only, and GetString(k) forwards to the value's own
		// `if (m_type != VTYPE_STRING) throw;` -- a bare throw
		// outside a catch, which is std::terminate rather than
		// anything catchable. The "headers" read below already did
		// this; these two did not.
		std::string method = (d->HasKey("method") &&
				      d->GetType("method") == VTYPE_STRING)
					     ? d->GetString("method").ToString()
					     : "GET";

		std::string url =
			(d->HasKey("url") && d->GetType("url") == VTYPE_STRING)
				? d->GetString("url")
				: "";

		http_client_headers_t request_headers;

		if (d->HasKey("headers") && d->GetType("headers") == VTYPE_DICTIONARY) {
			CefRefPtr<CefDictionaryValue> h = d->GetDictionary("headers");
			CefDictionaryValue::KeyList keys;

			if (h->GetKeys(keys)) {
				for (std::string key : keys) {
					if (h->GetType(key) == VTYPE_STRING) {
						request_headers.emplace(
							std::make_pair<std::string, std::string>(
								key.c_str(),
								h->GetString(key).ToString()));
					}
					else if (h->GetType(key) == VTYPE_LIST) {
						CefRefPtr<CefListValue> l = h->GetList(key);

						for (size_t i = 0; i < l->GetSize(); ++i) {
							if (l->GetType(i) != VTYPE_STRING) {
								continue;
							}

							std::string val = l->GetString(i);

							request_headers.emplace(
								std::make_pair<std::string, std::string>(
									key.c_str(),
									val.c_str()));
						}
					}
				}
			}
		}

		std::transform(method.begin(), method.end(), method.begin(), ::toupper);

		auto cb = [&](char* data, void* userdata, char* error_msg, int http_code)
		{
			UNUSED_PARAMETER(userdata);

			if (http_code != 0) {
				http_status_code = http_code;
			}

			if (error_msg) {
				error_string = error_msg;
			}

			if (data) {
				try {
					result_string = utf8_to_wstring(data);
				}
				catch(...) {
					result_string = L"Invalid UTF-8 input";
					error_string = "Failed decoding UTF-8 response data";
				}
			}
		};

		//
		// No usable url: answer the failure shape rather than hand
		// "" to libcurl.
		//
		// Not hypothetical, and not a consequence of the type check
		// above. HttpGetString("") does not return on this machine,
		// and this body runs under the per-translation-unit API call
		// lock for its whole length -- so one such call from any
		// page silently wedges every host API method for the life of
		// the process, with OBS itself still reporting as
		// responsive. Measured: after one, getHostProperties() never
		// answered again.
		//
		// Reachable on master through the missing-key branch, which
		// produced the same "" long before a wrongly typed url
		// could.
		//
		if (url.empty()) {
			error_string = "No URL specified";
		} else if (method == "GET" || method == "DELETE" ||
			   method == "OPTIONS") {
			success = HttpGetString(
				method,
				url.c_str(),
				request_headers,
				cb,
				nullptr);
		} else {
			std::string post_data;

			if (d->HasKey("body") && d->GetType("body") == VTYPE_STRING) {
				post_data = wstring_to_utf8(
					d->GetString("body").ToWString());
			}

			success = HttpPostString(
				method,
				url.c_str(),
				request_headers,
				post_data.c_str(),
				cb,
				nullptr);
		}
	}

	CefRefPtr<CefDictionaryValue> output_dict = CefDictionaryValue::Create();

	output_dict->SetBool("success", success);

	if (success) {
		output_dict->SetString("body", result_string);
	}
	else {
		output_dict->SetNull("body");
	}

	output_dict->SetInt("statusCode", http_status_code);

	if (error_string.size()) {
		output_dict->SetString("statusText", error_string.c_str());
	}
	else {
		output_dict->SetString("statusText", "OK");
	}

	output->SetDictionary(output_dict);
}
