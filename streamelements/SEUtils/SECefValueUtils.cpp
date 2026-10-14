#include "SECefValueUtils.hpp"
#include <string>

bool IsCefValueEqual(CefRefPtr<CefValue> a, CefRefPtr<CefValue> b)
{
	std::string json1 = CefWriteJSON(a, JSON_WRITER_DEFAULT);
	std::string json2 = CefWriteJSON(a, JSON_WRITER_DEFAULT);

	return json1 == json2;
}
