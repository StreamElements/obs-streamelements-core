#pragma once

#include "cef-headers.hpp"

#include <memory>
#include <mutex>

//
// Forward declaration, deliberately: including the real header here pulls
// windows.h in ahead of winsock2.h and redefines every socket struct. A
// shared_ptr member only needs a complete type where the destructor is
// emitted, which is the .cpp.
//
class SECleanupManager;
class SELocalFilesystemHttpServer;

class SEBackupManager
{
public:
	//
	// Takes the cleanup manager it schedules temporary paths with, rather
	// than reaching for it through the global singleton. The reference
	// keeps it alive for exactly as long as this object needs it.
	//
	// The local filesystem HTTP server is taken for the same reason: the
	// URLs this manager hands a page are signed against that server's
	// base url, and that was its last use of global state.
	//
	SEBackupManager(
		std::shared_ptr<SECleanupManager> cleanupManager,
		std::shared_ptr<SELocalFilesystemHttpServer> httpServer);
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
	std::shared_ptr<SECleanupManager> m_cleanupManager;
	std::shared_ptr<SELocalFilesystemHttpServer> m_httpServer;

	std::recursive_mutex m_mutex;
};
