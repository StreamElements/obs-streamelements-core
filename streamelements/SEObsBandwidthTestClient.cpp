#include "SEObsBandwidthTestClient.hpp"

SEObsBandwidthTestClient::SEObsBandwidthTestClient()
{
}

SEObsBandwidthTestClient::~SEObsBandwidthTestClient()
{
}

void SEObsBandwidthTestClient::OnObsExit()
{
	CancelAll();
}
