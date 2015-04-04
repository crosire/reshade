#include "Runtime.hpp"
#include "HookManager.hpp"

#include <Winsock2.h>

// -----------------------------------------------------------------------------------------------------

EXPORT int WSAAPI send(SOCKET s, const char *buf, int len, int flags)
{
	DWORD sent = 0;
	WSABUF data = { len, const_cast<CHAR *>(buf) };

	return WSASend(s, &data, 1, &sent, flags, nullptr, nullptr) + static_cast<int>(sent);
}
EXPORT int WSAAPI sendto(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
	DWORD sent = 0;
	WSABUF data = { len, const_cast<CHAR *>(buf) };

	return WSASendTo(s, &data, 1, &sent, flags, to, tolen, nullptr, nullptr) + static_cast<int>(sent);
}
EXPORT int WSAAPI recv(SOCKET s, char *buf, int len, int flags)
{
	DWORD recieved = 0;
	WSABUF data = { len, buf };

	return WSARecv(s, &data, 1, &recieved, reinterpret_cast<LPDWORD>(&flags), nullptr, nullptr) + static_cast<int>(recieved);
}
EXPORT int WSAAPI recvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
	DWORD recieved = 0;
	WSABUF data = { len, buf };

	return WSARecvFrom(s, &data, 1, &recieved, reinterpret_cast<LPDWORD>(&flags), from, fromlen, nullptr, nullptr) + static_cast<int>(recieved);
}

EXPORT int WSAAPI WSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReShade::Hooks::Call(&WSASend);

	for (DWORD i = 0; i < dwBufferCount; ++i)
	{
		_InterlockedExchangeAdd(&ReShade::Runtime::sNetworkUpload, lpBuffers[i].len);
	}

	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
}
EXPORT int WSAAPI WSASendTo(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const struct sockaddr *lpTo, int iToLen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReShade::Hooks::Call(&WSASendTo);

	for (DWORD i = 0; i < dwBufferCount; ++i)
	{
		_InterlockedExchangeAdd(&ReShade::Runtime::sNetworkUpload, lpBuffers[i].len);
	}

	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iToLen, lpOverlapped, lpCompletionRoutine);
}
EXPORT int WSAAPI WSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReShade::Hooks::Call(&WSARecv);

	DWORD recieved = 0;

	const int status = trampoline(s, lpBuffers, dwBufferCount, &recieved, lpFlags, lpOverlapped, lpCompletionRoutine);

	if (status == 0)
	{
		if (lpNumberOfBytesRecvd != nullptr)
		{
			*lpNumberOfBytesRecvd = recieved;
		}

		for (DWORD i = 0; i < dwBufferCount; recieved -= lpBuffers[i++].len)
		{
			_InterlockedExchangeAdd(&ReShade::Runtime::sNetworkDownload, std::min(recieved, lpBuffers[i].len));
		}
	}

	return status;
}
EXPORT int WSAAPI WSARecvEx(SOCKET s, char *buf, int len, int *flags)
{
	static const auto trampoline = ReShade::Hooks::Call(&WSARecvEx);

	const int recieved = trampoline(s, buf, len, flags);

	if (recieved > 0)
	{
		_InterlockedExchangeAdd(&ReShade::Runtime::sNetworkDownload, recieved);
	}

	return recieved;
}
EXPORT int WSAAPI WSARecvFrom(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, struct sockaddr *lpFrom, LPINT lpFromlen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReShade::Hooks::Call(&WSARecvFrom);

	DWORD recieved = 0;

	const int status = trampoline(s, lpBuffers, dwBufferCount, &recieved, lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine);

	if (status == 0)
	{
		if (lpNumberOfBytesRecvd != nullptr)
		{
			*lpNumberOfBytesRecvd = recieved;
		}

		for (DWORD i = 0; i < dwBufferCount; recieved -= lpBuffers[i++].len)
		{
			_InterlockedExchangeAdd(&ReShade::Runtime::sNetworkDownload, std::min(recieved, lpBuffers[i].len));
		}
	}

	return status;
}