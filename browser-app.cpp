/******************************************************************************
 Copyright (C) 2014 by John R. Bradley <jrb@turrettech.com>
 Copyright (C) 2018 by Hugh Bailey ("Jim") <jim@obsproject.com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#include "browser-app.hpp"
#include "browser-version.h"
#include <json11/json11.hpp>
#include <include/cef_parser.h>		// CefParseJSON, CefWriteJSON

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef USE_QT_LOOP
#include <util/base.h>
#include <util/platform.h>
#include <util/threading.h>
#include <QTimer>
#endif

#define UNUSED_PARAMETER(x) \
	{                   \
		(void)x;    \
	}

using namespace json11;

static std::string StringReplaceAll(std::string str, const std::string& from, const std::string& to) {
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
	}
	return str;
}

CefRefPtr<CefRenderProcessHandler> BrowserApp::GetRenderProcessHandler()
{
	return this;
}

CefRefPtr<CefBrowserProcessHandler> BrowserApp::GetBrowserProcessHandler()
{
	return this;
}

void BrowserApp::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar)
{
#if CHROME_VERSION_BUILD >= 3683
	registrar->AddCustomScheme("http",
				   CEF_SCHEME_OPTION_STANDARD |
					   CEF_SCHEME_OPTION_CORS_ENABLED);
#elif CHROME_VERSION_BUILD >= 3029
	registrar->AddCustomScheme("http", true, false, false, false, true,
				   false);
#else
	registrar->AddCustomScheme("http", true, false, false, false, true);
#endif
}

void BrowserApp::OnBeforeChildProcessLaunch(
	CefRefPtr<CefCommandLine> command_line)
{
#ifdef _WIN32
	std::string pid = std::to_string(GetCurrentProcessId());
	command_line->AppendSwitchWithValue("parent_pid", pid);
#else
	(void)command_line;
#endif
}

void BrowserApp::OnBeforeCommandLineProcessing(
	const CefString &, CefRefPtr<CefCommandLine> command_line)
{
	if (!shared_texture_available) {
		bool enableGPU = command_line->HasSwitch("enable-gpu");
		CefString type = command_line->GetSwitchValue("type");

		if (!enableGPU && type.empty()) {
			command_line->AppendSwitch("disable-gpu-compositing");
		}
	}

	if (command_line->HasSwitch("disable-features")) {
		// Don't override existing, as this can break OSR
		std::string disableFeatures =
			command_line->GetSwitchValue("disable-features");
		disableFeatures += ",HardwareMediaKeyHandling";
		command_line->AppendSwitchWithValue("disable-features",
						    disableFeatures);
	} else {
		command_line->AppendSwitchWithValue("disable-features",
						    "HardwareMediaKeyHandling");
	}

	command_line->AppendSwitchWithValue("autoplay-policy",
			"no-user-gesture-required");
	command_line->AppendSwitchWithValue("plugin-policy", "allow");
#ifdef __APPLE__
	command_line->AppendSwitch("use-mock-keychain");
#endif
}

void BrowserApp::OnFocusedNodeChanged(CefRefPtr<CefBrowser> browser,
				      CefRefPtr<CefFrame> frame,
				      CefRefPtr<CefDOMNode> node)
{
	CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create(
		"CefRenderProcessHandler::OnFocusedNodeChanged");

	if (!!node.get() && node->IsEditable()) {
		// Editable node
		msg->GetArgumentList()->SetBool(0, true);
	} else {
		// Empty or non-editable node
		msg->GetArgumentList()->SetBool(0, false);
	}

	SendBrowserProcessMessage(browser, PID_BROWSER, msg);
}

void BrowserApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
				  CefRefPtr<CefFrame>,
				  CefRefPtr<CefV8Context> context)
{
	CefRefPtr<CefV8Value> globalObj = context->GetGlobal();

	CefRefPtr<CefV8Value> obsStudioObj = CefV8Value::CreateObject(0, 0);
	globalObj->SetValue("obsstudio", obsStudioObj,
			    V8_PROPERTY_ATTRIBUTE_NONE);

	CefRefPtr<CefV8Value> pluginVersion =
		CefV8Value::CreateString(OBS_BROWSER_VERSION_STRING);
	obsStudioObj->SetValue("pluginVersion", pluginVersion,
			       V8_PROPERTY_ATTRIBUTE_NONE);

	CefRefPtr<CefV8Value> getCurrentScene =
		CefV8Value::CreateFunction("getCurrentScene", this);
	obsStudioObj->SetValue("getCurrentScene", getCurrentScene,
			       V8_PROPERTY_ATTRIBUTE_NONE);

	CefRefPtr<CefV8Value> getStatus =
		CefV8Value::CreateFunction("getStatus", this);
	obsStudioObj->SetValue("getStatus", getStatus,
			       V8_PROPERTY_ATTRIBUTE_NONE);

	CefRefPtr<CefV8Value> saveReplayBuffer =
		CefV8Value::CreateFunction("saveReplayBuffer", this);
	obsStudioObj->SetValue("saveReplayBuffer", saveReplayBuffer,
			       V8_PROPERTY_ATTRIBUTE_NONE);

#if !ENABLE_WASHIDDEN
	int id = browser->GetIdentifier();
	if (browserVis.find(id) != browserVis.end()) {
		SetDocumentVisibility(browser, browserVis[id]);
	}
#endif

#if ENABLE_CREATE_BROWSER_API
	std::lock_guard<decltype(m_createBrowserArgsMutex)> guard(
		m_createBrowserArgsMutex);

	if (!m_createBrowserArgs.count(browser->GetIdentifier()))
		return;

	if (!m_createBrowserArgs[browser->GetIdentifier()]->HasKey("streamelements"))
		return;

	CefRefPtr<CefDictionaryValue> argsRoot =
		m_createBrowserArgs[browser->GetIdentifier()]->GetDictionary("streamelements");

	if (!argsRoot->HasKey("api"))
		return;

	CefRefPtr<CefDictionaryValue> apiRoot =
		argsRoot->GetDictionary("api");

	if (apiRoot->HasKey("properties")) {
		CefRefPtr<CefDictionaryValue> root =
			apiRoot->GetDictionary("properties");

		CefString containerName = root->HasKey("container")
						  ? root->GetString("container")
						  : "host";

		if (root->HasKey("items")) {
			CefRefPtr<CefDictionaryValue> items =
				root->GetDictionary("items");

			SEBindJavaScriptProperties(globalObj, containerName,
						   items);
		}
	}

	if (apiRoot->HasKey("functions")) {
		CefRefPtr<CefDictionaryValue> root =
			apiRoot->GetDictionary("functions");

		CefString containerName = root->HasKey("container")
						  ? root->GetString("container")
						  : "host";

		if (root->HasKey("items")) {
			CefRefPtr<CefDictionaryValue> items =
				root->GetDictionary("items");

			SEBindJavaScriptFunctions(globalObj, containerName,
						  items);
		}
	}
#else
	///
	// signal CefClient that render process context has been created
	CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("CefRenderProcessHandler::OnContextCreated");
	//CefRefPtr<CefBrowser> browser = CefV8Context::GetCurrentContext()->GetBrowser();
	SendBrowserProcessMessage(browser, PID_BROWSER, msg);
#endif
}

void BrowserApp::ExecuteJSFunction(CefRefPtr<CefBrowser> browser,
				   const char *functionName,
				   CefV8ValueList arguments)
{
	CefRefPtr<CefV8Context> context =
		browser->GetMainFrame()->GetV8Context();

	context->Enter();

	CefRefPtr<CefV8Value> globalObj = context->GetGlobal();
	CefRefPtr<CefV8Value> obsStudioObj = globalObj->GetValue("obsstudio");
	CefRefPtr<CefV8Value> jsFunction = obsStudioObj->GetValue(functionName);

	if (jsFunction && jsFunction->IsFunction())
		jsFunction->ExecuteFunction(NULL, arguments);

	context->Exit();
}

#if !ENABLE_WASHIDDEN
void BrowserApp::SetFrameDocumentVisibility(CefRefPtr<CefBrowser> browser,
					    CefRefPtr<CefFrame> frame,
					    bool isVisible)
{
	UNUSED_PARAMETER(browser);

	CefRefPtr<CefV8Context> context = frame->GetV8Context();

	context->Enter();

	CefRefPtr<CefV8Value> globalObj = context->GetGlobal();

	CefRefPtr<CefV8Value> documentObject = globalObj->GetValue("document");

	if (!!documentObject) {
		documentObject->SetValue("hidden",
					 CefV8Value::CreateBool(!isVisible),
					 V8_PROPERTY_ATTRIBUTE_READONLY);

		documentObject->SetValue(
			"visibilityState",
			CefV8Value::CreateString(isVisible ? "visible"
							   : "hidden"),
			V8_PROPERTY_ATTRIBUTE_READONLY);

		std::string script = "new CustomEvent('visibilitychange', {});";

		CefRefPtr<CefV8Value> returnValue;
		CefRefPtr<CefV8Exception> exception;

		/* Create the CustomEvent object
		 * We have to use eval to invoke the new operator */
		bool success = context->Eval(script, frame->GetURL(), 0,
					     returnValue, exception);

		if (success) {
			CefV8ValueList arguments;
			arguments.push_back(returnValue);

			CefRefPtr<CefV8Value> dispatchEvent =
				documentObject->GetValue("dispatchEvent");

			/* Dispatch visibilitychange event on the document
			 * object */
			dispatchEvent->ExecuteFunction(documentObject,
						       arguments);
		}
	}

	context->Exit();
}

void BrowserApp::SetDocumentVisibility(CefRefPtr<CefBrowser> browser,
				       bool isVisible)
{
	/* This method might be called before OnContextCreated
	 * call is made. We'll save the requested visibility
	 * state here, and use it later in OnContextCreated to
	 * set initial page visibility state. */
	browserVis[browser->GetIdentifier()] = isVisible;

	std::vector<int64> frameIdentifiers;
	/* Set visibility state for every frame in the browser
	 *
	 * According to the Page Visibility API documentation:
	 * https://developer.mozilla.org/en-US/docs/Web/API/Page_Visibility_API
	 *
	 * "Visibility states of an <iframe> are the same as
	 * the parent document. Hiding an <iframe> using CSS
	 * properties (such as display: none;) doesn't trigger
	 * visibility events or change the state of the document
	 * contained within the frame."
	 *
	 * Thus, we set the same visibility state for every frame of the browser.
	 */
	browser->GetFrameIdentifiers(frameIdentifiers);

	for (int64 frameId : frameIdentifiers) {
		CefRefPtr<CefFrame> frame = browser->GetFrame(frameId);

		SetFrameDocumentVisibility(browser, frame, isVisible);
	}
}
#endif

bool BrowserApp::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
#if CHROME_VERSION_BUILD >= 3770
					  CefRefPtr<CefFrame> frame,
#endif
					  CefProcessId source_process,
					  CefRefPtr<CefProcessMessage> message)
{
#if CHROME_VERSION_BUILD >= 3770
	UNUSED_PARAMETER(frame);
#endif
	DCHECK(source_process == PID_BROWSER);

	CefRefPtr<CefListValue> args = message->GetArgumentList();

	if (message->GetName() == "Visibility") {
		CefV8ValueList arguments;
		arguments.push_back(CefV8Value::CreateBool(args->GetBool(0)));

		ExecuteJSFunction(browser, "onVisibilityChange", arguments);

#if !ENABLE_WASHIDDEN
		SetDocumentVisibility(browser, args->GetBool(0));
#endif

	} else if (message->GetName() == "Active") {
		CefV8ValueList arguments;
		arguments.push_back(CefV8Value::CreateBool(args->GetBool(0)));

		ExecuteJSFunction(browser, "onActiveChange", arguments);

	} else if (message->GetName() == "DispatchJSEvent") {
		CefRefPtr<CefV8Context> context =
			browser->GetMainFrame()->GetV8Context();

		context->Enter();

		CefRefPtr<CefV8Value> globalObj = context->GetGlobal();
		
		Json::object json;
		if (args->GetSize() > 1) {
			std::string err;
			json["detail"] = Json::parse(args->GetString(1).ToString(), err);
		}
		std::string jsonString = Json(json).dump();

		jsonString = StringReplaceAll(jsonString, "'", "\\u0027");
		jsonString = StringReplaceAll(jsonString, "\\", "\\\\");

		std::string script;

		script += "new CustomEvent('";
		script += args->GetString(0).ToString();
		script += "', ";
		script += "window.JSON.parse('" + jsonString + "')";
		script += ");";

		CefRefPtr<CefV8Value> returnValue;
		CefRefPtr<CefV8Exception> exception;

		/* Create the CustomEvent object
		 * We have to use eval to invoke the new operator */
		context->Eval(script, browser->GetMainFrame()->GetURL(), 0,
			      returnValue, exception);

		CefV8ValueList arguments;
		arguments.push_back(returnValue);

		CefRefPtr<CefV8Value> dispatchEvent =
			globalObj->GetValue("dispatchEvent");
		dispatchEvent->ExecuteFunction(NULL, arguments);

		context->Exit();

	}
	else if (message->GetName() == "executeCallback") {
		CefRefPtr<CefV8Context> context =
			browser->GetMainFrame()->GetV8Context();
		CefRefPtr<CefV8Value> retval;
		CefRefPtr<CefV8Exception> exception;

		context->Enter();

		CefRefPtr<CefListValue> arguments = message->GetArgumentList();

		if (arguments->GetSize()) {
			int callbackID = arguments->GetInt(0);

			CefRefPtr<CefV8Value> callback = callbackMap[callbackID];

			CefV8ValueList args;

			if (arguments->GetSize() > 1) {
				std::string json = arguments->GetString(1).ToString();

				json = StringReplaceAll(json, "'", "\\u0027");
				json = StringReplaceAll(json, "\\", "\\\\");

				std::string script = "JSON.parse('" + json + "');";

				context->Eval(script, browser->GetMainFrame()->GetURL(),
					0, retval, exception);

				args.push_back(retval);
			}

		if (callback)
			callback->ExecuteFunction(NULL, args);

			callbackMap.erase(callbackID);
		}

		context->Exit();
	}
	else if (message->GetName() == "CefRenderProcessHandler::BindJavaScriptProperties") {
		CefRefPtr<CefV8Context> context =
			browser->GetMainFrame()->GetV8Context();

		context->Enter();

		CefRefPtr<CefV8Value> globalObj = context->GetGlobal();

		CefString containerName = args->GetValue(0)->GetString();
		CefString root_json_string = args->GetValue(1)->GetString();
		CefRefPtr<CefDictionaryValue> root =
			CefParseJSON(root_json_string, JSON_PARSER_ALLOW_TRAILING_COMMAS)->GetDictionary();

		SEBindJavaScriptProperties(globalObj, containerName, root);

		context->Exit();
	}
	else if (message->GetName() == "CefRenderProcessHandler::BindJavaScriptFunctions") {
		CefRefPtr<CefV8Context> context =
			browser->GetMainFrame()->GetV8Context();

		context->Enter();

		CefRefPtr<CefV8Value> globalObj = context->GetGlobal();

		CefString containerName = args->GetValue(0)->GetString();
		CefString root_json_string = args->GetValue(1)->GetString();
		CefRefPtr<CefDictionaryValue> root =
			CefParseJSON(root_json_string, JSON_PARSER_ALLOW_TRAILING_COMMAS)->GetDictionary();

		SEBindJavaScriptFunctions(globalObj, containerName, root);

		context->Exit();
	} else {
		return false;
	}

	return true;
}

bool BrowserApp::Execute(const CefString &name, CefRefPtr<CefV8Value>,
			 const CefV8ValueList &arguments,
			 CefRefPtr<CefV8Value> &, CefString &)
{
	if (name == "getCurrentScene" || name == "getStatus" ||
	    name == "saveReplayBuffer") {
		if (arguments.size() == 1 && arguments[0]->IsFunction()) {
			callbackId++;
			callbackMap[callbackId] = arguments[0];
		}

		CefRefPtr<CefProcessMessage> msg =
			CefProcessMessage::Create(name);
		CefRefPtr<CefListValue> args = msg->GetArgumentList();
		args->SetInt(0, callbackId);

		CefRefPtr<CefBrowser> browser =
			CefV8Context::GetCurrentContext()->GetBrowser();
		SendBrowserProcessMessage(browser, PID_BROWSER, msg);
	} else if (cefClientFunctions.count(name)) {
		/* dynamic API function binding from CefClient, see "CefRenderProcessHandler::BindJavaScriptFunctions"
		   message for more details */

		CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
		CefRefPtr<CefBrowser> browser = context->GetBrowser();

		context->Enter();

		APIFunctionItem item = cefClientFunctions[name];

		CefRefPtr<CefProcessMessage> msg =
			CefProcessMessage::Create(item.message);

		CefRefPtr<CefListValue> args = msg->GetArgumentList();

		// Set header
		args->SetInt(args->GetSize(), 4); // header length, including first arg
		args->SetString(args->GetSize(), item.message);
		args->SetString(args->GetSize(), item.fullName);
		args->SetString(args->GetSize(), name);

		// Convert args except functions to JSON
		for (int i = 0; i < arguments.size() && !arguments[i]->IsFunction(); ++i) {
			CefV8ValueList JSON_value_list;
			JSON_value_list.push_back(arguments[i]);

			// Call global JSON.stringify JS function to stringify JSON value
			CefRefPtr<CefV8Value> JSON_string_value =
				context->GetGlobal()->GetValue("JSON")->GetValue("stringify")->ExecuteFunction(NULL, JSON_value_list);

			args->SetString(args->GetSize(), JSON_string_value->GetStringValue());
		}

		if (arguments.size() > 0 && arguments[arguments.size() - 1]->IsFunction()) {
			callbackId++;
			callbackMap[callbackId] = arguments[arguments.size() - 1];

			args->SetInt(args->GetSize(), callbackId);
		} else {
			args->SetInt(args->GetSize(), -1); // invalid callback ID
		}

		// Send message to CefClient
		SendBrowserProcessMessage(browser, PID_BROWSER, msg);

		context->Exit();

		return true;
	} else {
		/* Function does not exist. */
		return false;
	}

	return true;
}

#ifdef USE_QT_LOOP
Q_DECLARE_METATYPE(MessageTask);
MessageObject messageObject;

void QueueBrowserTask(CefRefPtr<CefBrowser> browser, BrowserFunc func)
{
	std::lock_guard<std::mutex> lock(messageObject.browserTaskMutex);
	messageObject.browserTasks.emplace_back(browser, func);

	QMetaObject::invokeMethod(&messageObject, "ExecuteNextBrowserTask",
				  Qt::QueuedConnection);
}

bool MessageObject::ExecuteNextBrowserTask()
{
	Task nextTask;
	{
		std::lock_guard<std::mutex> lock(browserTaskMutex);
		if (!browserTasks.size())
			return false;

		nextTask = browserTasks[0];
		browserTasks.pop_front();
	}

	nextTask.func(nextTask.browser);
	return true;
}

void MessageObject::ExecuteTask(MessageTask task)
{
	task();
}

void MessageObject::DoCefMessageLoop(int ms)
{
	if (ms)
		QTimer::singleShot((int)ms + 2,
				   []() { CefDoMessageLoopWork(); });
	else
		CefDoMessageLoopWork();
}

void MessageObject::Process()
{
	CefDoMessageLoopWork();
}

void ProcessCef()
{
	QMetaObject::invokeMethod(&messageObject, "DoCefMessageLoop",
				  Qt::QueuedConnection, Q_ARG(int, (int)0));
}

#define MAX_DELAY (1000 / 30)

void BrowserApp::OnScheduleMessagePumpWork(int64 delay_ms)
{
	if (delay_ms < 0)
		delay_ms = 0;
	else if (delay_ms > MAX_DELAY)
		delay_ms = MAX_DELAY;

	if (!frameTimer.isActive()) {
		QObject::connect(&frameTimer, &QTimer::timeout, &messageObject,
				 &MessageObject::Process);
		frameTimer.setSingleShot(false);
		frameTimer.start(33);
	}

	QMetaObject::invokeMethod(&messageObject, "DoCefMessageLoop",
				  Qt::QueuedConnection,
				  Q_ARG(int, (int)delay_ms));
}
#endif

void BrowserApp::SEBindJavaScriptProperties(CefRefPtr<CefV8Value> globalObj,
				CefString containerName,
				CefRefPtr<CefDictionaryValue> root)
{
	CefDictionaryValue::KeyList propsList;
	if (!root->GetKeys(propsList))
		return;

	for (auto propName : propsList) {
		// Get/create function container
		CefRefPtr<CefV8Value> containerObj = nullptr;

		if (!globalObj->HasValue(containerName)) {
			containerObj = CefV8Value::CreateObject(0, 0);

			globalObj->SetValue(containerName, containerObj,
						V8_PROPERTY_ATTRIBUTE_NONE);
		} else {
			containerObj =
				globalObj->GetValue(containerName);
		}

		std::string propFullName = "window.";
		propFullName.append(containerName);
		propFullName.append(".");
		propFullName.append(propName);

		CefRefPtr<CefV8Value> propValue;

		switch (root->GetType(propName)) {
		case VTYPE_NULL:
			propValue = CefV8Value::CreateNull();
			break;
		case VTYPE_BOOL:
			propValue = CefV8Value::CreateBool(
				root->GetBool(propName));
			break;
		case VTYPE_INT:
			propValue = CefV8Value::CreateInt(
				root->GetInt(propName));
			break;
		case VTYPE_DOUBLE:
			propValue = CefV8Value::CreateDouble(
				root->GetDouble(propName));
			break;
		case VTYPE_STRING:
			propValue = CefV8Value::CreateString(
				root->GetString(propName));
			break;
		// case VTYPE_BINARY:
		// case VTYPE_DICTIONARY:
		// case VTYPE_LIST:
		// case VTYPE_INVALID:
		default:
			propValue = CefV8Value::CreateUndefined();
			break;
		}

		// Create function
		containerObj->SetValue(propName, propValue,
					V8_PROPERTY_ATTRIBUTE_NONE);
	}
}

void BrowserApp::SEBindJavaScriptFunctions(CefRefPtr<CefV8Value> globalObj,
					 CefString containerName,
					 CefRefPtr<CefDictionaryValue> root)
{
	CefDictionaryValue::KeyList functionsList;
	if (!root->GetKeys(functionsList))
		return;

	for (auto functionName : functionsList) {
		CefRefPtr<CefDictionaryValue> function =
			root->GetDictionary(functionName);

		auto messageName = function->GetString("message");

		if (!messageName.empty()) {
			// Get/create function container
			CefRefPtr<CefV8Value> containerObj = nullptr;

			if (!globalObj->HasValue(containerName)) {
				containerObj = CefV8Value::CreateObject(0, 0);

				globalObj->SetValue(containerName, containerObj,
						    V8_PROPERTY_ATTRIBUTE_NONE);
			} else {
				containerObj =
					globalObj->GetValue(containerName);
			}

			std::string functionFullName = "window.";
			functionFullName.append(containerName);
			functionFullName.append(".");
			functionFullName.append(functionName);

			// Create function
			containerObj->SetValue(functionName,
					       CefV8Value::CreateFunction(
						       functionFullName, this),
					       V8_PROPERTY_ATTRIBUTE_NONE);

			// Add function name -> metadata map
			APIFunctionItem item;

			item.message = messageName;
			item.fullName = functionFullName;

			cefClientFunctions[functionFullName] = item;
		}
	}
}

#if ENABLE_CREATE_BROWSER_API
void BrowserApp::OnBrowserCreated(CefRefPtr<CefBrowser> browser,
				  CefRefPtr<CefDictionaryValue> extra_info)
{
	std::lock_guard<decltype(m_createBrowserArgsMutex)> guard(
		m_createBrowserArgsMutex);

	// Store info to be later used by OnContextCreated()
	m_createBrowserArgs[browser->GetIdentifier()] = extra_info->Copy(false);
}

void BrowserApp::OnBrowserDestroyed(CefRefPtr<CefBrowser> browser)
{
	std::lock_guard<decltype(m_createBrowserArgsMutex)> guard(
		m_createBrowserArgsMutex);

	// Clear info stored for usage by OnContextCreated()
	m_createBrowserArgs.erase(browser->GetIdentifier());
}
#endif
