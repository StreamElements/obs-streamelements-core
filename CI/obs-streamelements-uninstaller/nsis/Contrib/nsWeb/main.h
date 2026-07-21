#define PLUGINFUNCTION(name) void __declspec(dllexport) name(HWND hWndParent, int string_size,char *variables, stack_t **stacktop,extra_parameters *extra)

// Convert ANSI string to Unicode
BSTR WINAPI CreateUnicodeStr(const char* s)
{
	int lenW = MultiByteToWideChar(CP_ACP, 0, s, -1, 0, 0);
	BSTR bSTR = SysAllocStringLen(0, lenW);
	MultiByteToWideChar(CP_ACP, 0, s, -1, bSTR, lenW);
	return bSTR;
}

// Free the Unicode string
VOID WINAPI FreeUnicodeStr(BSTR bStr)
{
	SysFreeString(bStr);
	return;
}

// Adapted "atoi" function from NSIS & NSIS plugins
int _atoi(char* s)
{
	int v=0;
	if (!s)
	{
		return 0;
	}
	if (*s == '0' && (s[1] == 'x' || s[1] == 'X'))
	{
		s++;
		for (;;)
		{
			int c=*(++s);
			if (c >= '0' && c <= '9')
			{
				c-='0';
			}
			else if (c >= 'a' && c <= 'f')
			{
				c-='a'-10;
			}
			else if (c >= 'A' && c <= 'F')
			{
				c-='A'-10;
			}
			else
			{
				break;
			}
			v<<=4;
			v+=c;
		}
	}
	else if (*s == '0' && s[1] <= '7' && s[1] >= '0')
	{
		for (;;)
		{
			int c=*(++s);
			if (c >= '0' && c <= '7') c-='0';
			else
			{
				break;
			}
			v<<=3;
			v+=c;
		}
	}
	else
	{
		int sign=0;
		if (*s == '-')
		{
			sign++; 
		}
		else
		{
			s--;
		}
		for (;;)
		{
			int c=*(++s) - '0';
			if (c < 0 || c > 9)
			{
				break;
			}
			v*=10;
			v+=c;
		}
		if (sign)
		{
			v = -v;
		}
	}
	if (*s == '|') 
	{
		v |= _atoi(s+1);
	}
	return v;
}
