#pragma once

namespace ReShade
{
	class Network
	{
	public:
		static void OnSend(const char *buffer, unsigned int length);
		static void OnRecieve(char *buffer, unsigned int length);
	};
}