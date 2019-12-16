#pragma once

#include "cef-headers.hpp"

class StreamElementsBackupManager
{
public:
	StreamElementsBackupManager();
	~StreamElementsBackupManager();

public:
	void CreateLocalBackupPackage(CefRefPtr<CefValue> input,
					 CefRefPtr<CefValue> &output);

	void QueryBackupPackageContent(CefRefPtr<CefValue> input,
				       CefRefPtr<CefValue> &output);

	void RestoreBackupPackageContent(CefRefPtr<CefValue> input,
					 CefRefPtr<CefValue> &output);
};
