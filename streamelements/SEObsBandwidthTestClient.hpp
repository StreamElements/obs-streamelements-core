#pragma once

#include "SEBandwidthTestClient.hpp"
#include "SEObsAppMonitor.hpp"

class SEObsBandwidthTestClient :
	public SEBandwidthTestClient,
	public SEObsAppMonitor
{
public:
	SEObsBandwidthTestClient();
	virtual ~SEObsBandwidthTestClient();

protected:
	virtual void OnObsExit() override;
};
