#include "main.hpp"

#include <stdio.h>

int main(int argc, char **argv)
{
	if (argc != 2) {
		printf("Usage: %s --|path\n", argv[0]);

		return 255;
	}

	std::string filePath(argv[1]);

	ScriptEngine engine;
	bool result = false;

	if (filePath == "--") {
		std::string script;

		char buffer[32768 + 1];

		while (!feof(stdin) &&
		       fgets(buffer, sizeof(buffer) - 1, stdin)) {
			buffer[sizeof(buffer) - 1] = 0;
			script += buffer;
		}

		result = engine.ExecuteString(script.c_str());
	} else {
		result = engine.ExecuteFile(argv[1]);
	}

	return result ? 0 : 1;
}

#include <windows.h>
#include <codecvt>

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine,
	    int nShowCmd)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

	WCHAR *wCommandLine = GetCommandLineW();

	int argc;
	LPWSTR *wArgv = CommandLineToArgvW(wCommandLine, &argc);

	std::vector<std::string> stdArgv;
	for (int i = 0; i < argc; ++i) {
		stdArgv.emplace_back(myconv.to_bytes(wArgv[i]));
	}

	LocalFree(wArgv);

	char **argv = (char **)malloc(sizeof(char *) * argc);

	for (int i = 0; i < stdArgv.size(); ++i) {
		argv[i] = (char*)stdArgv[i].data();
	}

	int retVal = main(argc, argv);

	free(argv);

	return retVal;
}
