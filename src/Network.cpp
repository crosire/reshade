#include "Network.hpp"
#include "Runtime.hpp"

namespace ReShade
{
	void Network::OnSend(const char *buffer, unsigned int length)
	{
		Runtime::sNetworkUpload += length;
	}
	void Network::OnRecieve(char *buffer, unsigned int length)
	{
		Runtime::sNetworkDownload += length;
	}
}