//
// endModalDialog / endNonModalDialog / endDialog: what they do to the dialog
// they are called from.
//
// These three are the only way a modal SE.Live dialog ever closes -- the
// handler that opened it is blocked in exec() holding its own translation
// unit's API lock until one of them runs -- and until now they had no
// coverage of any kind, because they were three copies of one body inside a
// QDialog translation unit.
//
// EndDialogFromApiCall is that body, and it names no Qt type, so the dialog
// here is a recording stand-in and the assertions are ordinary.
//

#include "streamelements/SEDialogApiCall.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool cond, const char *msg)
{
	if (!cond) {
		std::fprintf(stderr, "FAIL: %s\n", msg);
		++failures;
	}
}

void check_eq(const std::string &got, const std::string &want, const char *msg)
{
	if (got != want) {
		std::fprintf(stderr, "FAIL: %s (got \"%s\", wanted \"%s\")\n",
			     msg, got.c_str(), want.c_str());
		++failures;
	}
}

class RecordingDialog : public ISEDialogHost {
public:
	std::vector<std::string> results;
	int accepts = 0;

	void SetResultJson(std::string json) override
	{
		results.push_back(json);
	}

	void Accept() override { ++accepts; }
};

struct Call {
	RecordingDialog dialog;
	CefRefPtr<CefListValue> args = CefListValue::Create();
	CefRefPtr<CefValue> result = CefValue::Create();
	int stops = 0;

	void run()
	{
		EndDialogFromApiCall(&dialog, args, result,
				     [this]() { ++stops; });
	}
};

//
// The ordinary case: a page calls host.endDialog({ok: 1}) and the value it
// passed comes back out as the dialog's result.
//
void a_value_is_written_back_as_json_and_the_dialog_closes()
{
	Call c;

	CefRefPtr<CefDictionaryValue> payload = CefDictionaryValue::Create();

	payload->SetInt("ok", 1);

	CefRefPtr<CefValue> arg = CefValue::Create();

	arg->SetDictionary(payload);
	c.args->SetValue(0, arg);

	c.run();

	check(c.dialog.results.size() == 1,
	      "the dialog result is set exactly once");

	if (c.dialog.results.size() == 1) {
		check_eq(c.dialog.results[0], "{\"ok\":1}",
			 "the argument is written back as JSON");
	}

	check(c.dialog.accepts == 1, "the dialog is closed exactly once");

	check(c.stops == 1,
	      "the handler is stopped, so a message already queued for it "
	      "does not run against a dialog on its way out");
}

//
// An explicit null is the literal "null", not an empty string: whoever
// opened the dialog parses this value back.
//
void an_explicit_null_argument_becomes_the_literal_null()
{
	Call c;

	CefRefPtr<CefValue> arg = CefValue::Create();

	arg->SetNull();
	c.args->SetValue(0, arg);

	c.run();

	check(c.dialog.results.size() == 1, "null: the result is still set");

	if (c.dialog.results.size() == 1) {
		check_eq(c.dialog.results[0], "null",
			 "null: as the literal, which is valid JSON -- an "
			 "empty string is not");
	}

	check(c.dialog.accepts == 1, "null: the dialog still closes");
}

void a_scalar_argument_is_written_back_as_itself()
{
	Call c;

	CefRefPtr<CefValue> arg = CefValue::Create();

	arg->SetString("done");
	c.args->SetValue(0, arg);

	c.run();

	if (c.dialog.results.size() == 1) {
		check_eq(c.dialog.results[0], "\"done\"",
			 "a string argument keeps its JSON quoting");
	} else {
		check(false, "a scalar argument sets the result");
	}
}

//
// Current behaviour, recorded rather than endorsed: no arguments at all and
// the call does nothing -- the dialog stays open, the handler keeps running,
// and the page is told `false`, which is also what it is told on success.
// So a page that calls endDialog() with no argument waits forever and cannot
// tell why from the return value.
//
void no_arguments_leaves_the_dialog_open_and_says_nothing_about_it()
{
	Call c;

	c.run();

	check(c.dialog.results.empty(),
	      "no arguments: the result is not touched");

	check(c.dialog.accepts == 0, "no arguments: the dialog is not closed");

	check(c.stops == 0, "no arguments: the handler keeps running");

	check(c.result->GetType() == VTYPE_BOOL && !c.result->GetBool(),
	      "no arguments: the page is told false -- the same thing a "
	      "successful close tells it");
}

//
// The return value carries no information, on any path. Pinned because it is
// surprising, and because changing it changes what every existing dialog
// page sees.
//
void the_return_value_is_false_even_on_success()
{
	Call c;

	CefRefPtr<CefValue> arg = CefValue::Create();

	arg->SetString("done");
	c.args->SetValue(0, arg);

	c.run();

	check(c.dialog.accepts == 1, "false-on-success: the dialog closed");

	check(c.result->GetType() == VTYPE_BOOL && !c.result->GetBool(),
	      "false-on-success: and the page is still told false. A page "
	      "cannot distinguish a close from an ignored call");
}

//
// The handler is stopped before the result is written, not after. Both
// orderings were in the tree -- endDialog did one, the other two did the
// other -- and the order that survives is the one that cannot process a
// queued message against a half-updated dialog.
//
void the_handler_is_stopped_before_the_dialog_is_touched()
{
	RecordingDialog dialog;

	CefRefPtr<CefListValue> args = CefListValue::Create();
	CefRefPtr<CefValue> result = CefValue::Create();
	CefRefPtr<CefValue> arg = CefValue::Create();

	arg->SetBool(true);
	args->SetValue(0, arg);

	std::vector<std::string> order;

	class OrderedDialog : public ISEDialogHost {
	public:
		std::vector<std::string> *order = nullptr;

		void SetResultJson(std::string) override
		{
			order->push_back("result");
		}

		void Accept() override { order->push_back("accept"); }
	};

	OrderedDialog ordered;

	ordered.order = &order;

	EndDialogFromApiCall(&ordered, args, result,
			     [&order]() { order.push_back("stop"); });

	check(order.size() == 3,
	      "ordering: stop, result and accept all happen");

	if (order.size() == 3) {
		check(order[0] == "stop" && order[1] == "result" &&
			      order[2] == "accept",
		      "ordering: stopped first, then the result, then the "
		      "close");
	}
}

//
// A null dialog is not a crash. Unreachable in the plug-in -- the handler is
// constructed with its dialog and outlives nothing -- but the three bodies
// dereferenced it without checking, and this is a translation unit a test
// can reach.
//
void a_missing_dialog_is_survivable()
{
	CefRefPtr<CefListValue> args = CefListValue::Create();
	CefRefPtr<CefValue> result = CefValue::Create();
	CefRefPtr<CefValue> arg = CefValue::Create();

	arg->SetBool(true);
	args->SetValue(0, arg);

	int stops = 0;

	EndDialogFromApiCall(nullptr, args, result, [&stops]() { ++stops; });

	check(stops == 0,
	      "no dialog: nothing is stopped, because nothing happened");

	check(result->GetType() == VTYPE_BOOL && !result->GetBool(),
	      "no dialog: the result is still false rather than untouched");
}

//
// Why the empty-serialization guard in EndDialogFromApiCall cannot fire, as
// an assertion rather than as a claim.
//
// All three original copies checked that CefWriteJSON returned something
// before using it. With this CefValue model it always does: serialization
// goes through nlohmann::json::dump(), which writes "null" for anything it
// cannot represent. A binary value is the case to look at -- it is the one
// type that both reaches the serializing branch (it is not VTYPE_NULL) and
// has no JSON form at all -- and it still writes four characters. If that
// ever stops being true the guard becomes live code, and this is what says
// so.
//
void the_json_writer_never_returns_nothing()
{
	const char bytes[] = {1, 2, 3};

	CefRefPtr<CefValue> binary = CefValue::Create();

	binary->SetBinary(CefBinaryValue::Create(bytes, sizeof(bytes)));

	check(binary->GetType() != VTYPE_NULL,
	      "writer: a binary value is not VTYPE_NULL, so it reaches the "
	      "serializing branch rather than the null one");

	check_eq(CefWriteJSON(binary, JSON_WRITER_DEFAULT).ToString(), "null",
		 "writer: and a value with no JSON form still serializes to "
		 "the literal null rather than to nothing -- which is why the "
		 "size check in EndDialogFromApiCall is unreachable");
}

} // namespace

int main()
{
	the_json_writer_never_returns_nothing();
	a_value_is_written_back_as_json_and_the_dialog_closes();
	an_explicit_null_argument_becomes_the_literal_null();
	a_scalar_argument_is_written_back_as_itself();
	no_arguments_leaves_the_dialog_open_and_says_nothing_about_it();
	the_return_value_is_false_even_on_success();
	the_handler_is_stopped_before_the_dialog_is_touched();
	a_missing_dialog_is_survivable();

	if (failures) {
		std::fprintf(stderr, "%d assertion(s) failed\n", failures);
		return 1;
	}

	std::puts("test_dialog_api_call: all assertions hold");
	return 0;
}
