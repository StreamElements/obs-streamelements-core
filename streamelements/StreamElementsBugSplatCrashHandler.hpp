#pragma once

#include "StreamElementsCrashHandler.hpp"

//
// BugSplat crash-reporting backend. Two entirely separate implementations sit
// behind this declaration: StreamElementsBugSplatCrashHandler.cpp drives
// BugSplat's native MiniDmpSender on Windows, and
// StreamElementsBugSplatCrashHandler.mm drives the BugSplatMac framework.
//
class StreamElementsBugSplatCrashHandler : public StreamElementsCrashHandler {
public:
	StreamElementsBugSplatCrashHandler();
	virtual ~StreamElementsBugSplatCrashHandler();

public:
	virtual void StopAsyncHangDetection() override;
};
