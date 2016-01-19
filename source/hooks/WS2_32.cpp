#include "runtime.hpp"
#include "hook_manager.hpp"

#include <Winsock2.h>

EXPORT int WSAAPI HookWSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = reshade::hooks::call(&HookWSASend);

	for (DWORD i = 0; i < dwBufferCount; ++i)
	{
		InterlockedExchangeAdd(&reshade::s_network_upload, lpBuffers[i].len);
	}

	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
}
EXPORT int WSAAPI HookWSASendTo(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const struct sockaddr *lpTo, int iToLen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = reshade::hooks::call(&HookWSASendTo);

	for (DWORD i = 0; i < dwBufferCount; ++i)
	{
		InterlockedExchangeAdd(&reshade::s_network_upload, lpBuffers[i].len);
	}

	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iToLen, lpOverlapped, lpCompletionRoutine);
}

EXPORT int WSAAPI HookSend(SOCKET s, const char *buf, int len, int flags)
{
	DWORD result = 0;
	WSABUF buffer = { static_cast<ULONG>(len), const_cast<CHAR *>(buf) };

	return HookWSASend(s, &buffer, 1, &result, flags, nullptr, nullptr) == 0 ? static_cast<int>(result) : SOCKET_ERROR;
}
EXPORT int WSAAPI HookSendTo(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
	DWORD result = 0;
	WSABUF buffer = { static_cast<ULONG>(len), const_cast<CHAR *>(buf) };

	return HookWSASendTo(s, &buffer, 1, &result, flags, to, tolen, nullptr, nullptr) == 0 ? static_cast<int>(result) : SOCKET_ERROR;
}
