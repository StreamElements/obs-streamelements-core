#pragma once

// Include the definitions of the script library and the add-ons we'll use.
// The project settings may need to be configured to let the compiler where
// to find these headers. Don't forget to add the source modules for the
// add-ons to your project as well so that they will be compiled into the
// application.
#include <angelscript.h>
#include <scriptbuilder/scriptbuilder.h>

#include <functional>

class ScriptEngine {
public:
	ScriptEngine();
	~ScriptEngine();

public:
	bool ExecuteString(const char *script);
	bool ExecuteFile(const char *path);

public:
	const char *GetLastErrorText() { return m_lastErrorText.c_str(); }

private:
	asIScriptEngine *m_engine;
	std::string m_lastErrorText;
};
