#pragma once

#include "cef-headers.hpp"

//
// What the host API needs from the backup manager: the four calls
// behind the backup and restore methods.
//
// Each takes a page-supplied description and writes its answer through
// output rather than returning one, which is the shape most of this
// surface uses.
//
class ISEBackupManager {
public:
	virtual ~ISEBackupManager() {}

	//
	// Packages the referenced local files into a backup archive.
	//
	virtual void CreateLocalBackupPackage(CefRefPtr<CefValue> input,
					      CefRefPtr<CefValue> &output) = 0;

	//
	// Reads what an archive contains, without restoring it.
	//
	virtual void QueryBackupPackageContent(CefRefPtr<CefValue> input,
					       CefRefPtr<CefValue> &output) = 0;

	//
	// Lists the local files a backup would have to carry.
	//
	virtual void
	QueryLocalBackupPackageReferencedFiles(CefRefPtr<CefValue> input,
					       CefRefPtr<CefValue> &output) = 0;

	//
	// Restores an archive's content into the current profile.
	//
	virtual void
	RestoreBackupPackageContent(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output) = 0;
};
