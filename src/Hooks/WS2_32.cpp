#include "Runtime.hpp"
#include "HookManager.hpp"

#include <Winsock2.h>

// -----------------------------------------------------------------------------------------------------

EXPORT int WSAAPI HookWSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReShade::Hooks::Call(&HookWSASend);

	for (DWORD i = 0; i < dwBufferCount; ++i)
	{
		_InterlockedExchangeAdd(&ReShade::Runtime::sNetworkUpload, lpBuffers[i].len);
	}

	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
}
EXPORT int WSAAPI HookWSASendTo(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const struct sockaddr *lpTo, int iToLen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReShade::Hooks::Call(&HookWSASendTo);

	for (DWORD i = 0; i < dwBufferCount; ++i)
	{
		_InterlockedExchangeAdd(&ReShade::Runtime::sNetworkUpload, lpBuffers[i].len);
	}

	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iToLen, lpOverlapped, lpCompletionRoutine);
}
EXPORT int WSAAPI HookWSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReShade::Hooks::Call(&HookWSARecv);

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
EXPORT int WSAAPI HookWSARecvEx(SOCKET s, char *buf, int len, int *flags)
{
	static const auto trampoline = ReShade::Hooks::Call(&HookWSARecvEx);

	const int recieved = trampoline(s, buf, len, flags);

	if (recieved > 0)
	{
		_InterlockedExchangeAdd(&ReShade::Runtime::sNetworkDownload, recieved);
	}

	return recieved;
}
EXPORT int WSAAPI HookWSARecvFrom(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, struct sockaddr *lpFrom, LPINT lpFromlen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReShade::Hooks::Call(&HookWSARecvFrom);

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

EXPORT int WSAAPI HookPOSIXSend(SOCKET s, const char *buf, int len, int flags)
{
	DWORD sent = 0;
	WSABUF data = { len, const_cast<CHAR *>(buf) };

	return HookWSASend(s, &data, 1, &sent, flags, nullptr, nullptr) + static_cast<int>(sent);
}
EXPORT int WSAAPI HookPOSIXSendTo(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
	DWORD sent = 0;
	WSABUF data = { len, const_cast<CHAR *>(buf) };

	return HookWSASendTo(s, &data, 1, &sent, flags, to, tolen, nullptr, nullptr) + static_cast<int>(sent);
}
EXPORT int WSAAPI HookPOSIXRecv(SOCKET s, char *buf, int len, int flags)
{
	DWORD recieved = 0;
	WSABUF data = { len, buf };

	return HookWSARecv(s, &data, 1, &recieved, reinterpret_cast<LPDWORD>(&flags), nullptr, nullptr) + static_cast<int>(recieved);
}
EXPORT int WSAAPI HookPOSIXRecvFrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
	DWORD recieved = 0;
	WSABUF data = { len, buf };

	return HookWSARecvFrom(s, &data, 1, &recieved, reinterpret_cast<LPDWORD>(&flags), from, fromlen, nullptr, nullptr) + static_cast<int>(recieved);
}