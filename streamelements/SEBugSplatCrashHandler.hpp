#pragma once

#include "SECrashHandler.hpp"

//
// BugSplat crash-reporting backend. Two entirely separate implementations sit
// behind this declaration: SEBugSplatCrashHandler.cpp drives
// BugSplat's native MiniDmpSender on Windows, and
// SEBugSplatCrashHandler.mm drives the BugSplatMac framework.
//
class SEBugSplatCrashHandler : public SECrashHandler {
public:
	SEBugSplatCrashHandler(std::shared_ptr<SEConfig> config);
	virtual ~SEBugSplatCrashHandler();

public:
	virtual void StopAsyncHangDetection() override;

protected:
	virtual void OnReportingServicesSet() override;
};
