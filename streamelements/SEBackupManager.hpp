#pragma once

#include "cef-headers.hpp"
#include <mutex>

class SEBackupManager
{
public:
	SEBackupManager();
	~SEBackupManager();

public:
	void
	QueryLocalBackupPackageReferencedFiles(CefRefPtr<CefValue> input,
					       CefRefPtr<CefValue> &output);

	void CreateLocalBackupPackage(CefRefPtr<CefValue> input,
					 CefRefPtr<CefValue> &output);

	void QueryBackupPackageContent(CefRefPtr<CefValue> input,
				       CefRefPtr<CefValue> &output);

	void RestoreBackupPackageContent(CefRefPtr<CefValue> input,
					 CefRefPtr<CefValue> &output);

private:
	std::recursive_mutex m_mutex;
};
