#include "hook_manager.hpp"

#include <Windows.h>
#include <Winsock2.h>

volatile long g_network_traffic = 0;

HOOK_EXPORT int WSAAPI HookWSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = reshade::hooks::call(&HookWSASend);

	for (DWORD i = 0; i < dwBufferCount; ++i)
	{
		InterlockedAdd(&g_network_traffic, lpBuffers[i].len);
	}

	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
}
HOOK_EXPORT int WSAAPI HookWSASendTo(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const struct sockaddr *lpTo, int iToLen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = reshade::hooks::call(&HookWSASendTo);

	for (DWORD i = 0; i < dwBufferCount; ++i)
	{
		InterlockedAdd(&g_network_traffic, lpBuffers[i].len);
	}

	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iToLen, lpOverlapped, lpCompletionRoutine);
}
HOOK_EXPORT int WSAAPI HookWSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = reshade::hooks::call(&HookWSARecv);

	const auto status = trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);

	if (status == 0 && lpNumberOfBytesRecvd != nullptr)
	{
		InterlockedAdd(&g_network_traffic, *lpNumberOfBytesRecvd);
	}

	return status;
}
HOOK_EXPORT int WSAAPI HookWSARecvEx(SOCKET s, char *buf, int len, int *flags)
{
	static const auto trampoline = reshade::hooks::call(&HookWSARecvEx);

	const int recieved = trampoline(s, buf, len, flags);

	if (recieved > 0)
	{
		InterlockedAdd(&g_network_traffic, recieved);
	}

	return recieved;
}
HOOK_EXPORT int WSAAPI HookWSARecvFrom(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, struct sockaddr *lpFrom, LPINT lpFromlen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = reshade::hooks::call(&HookWSARecvFrom);

	const auto status = trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine);

	if (status == 0 && lpNumberOfBytesRecvd != nullptr)
	{
		InterlockedAdd(&g_network_traffic, *lpNumberOfBytesRecvd);
	}

	return status;
}

HOOK_EXPORT int WSAAPI HookSend(SOCKET s, const char *buf, int len, int flags)
{
	DWORD result = 0;
	WSABUF buffer = { static_cast<ULONG>(len), const_cast<CHAR *>(buf) };

	return HookWSASend(s, &buffer, 1, &result, flags, nullptr, nullptr) == 0 ? static_cast<int>(result) : SOCKET_ERROR;
}
HOOK_EXPORT int WSAAPI HookSendTo(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
	DWORD result = 0;
	WSABUF buffer = { static_cast<ULONG>(len), const_cast<CHAR *>(buf) };

	return HookWSASendTo(s, &buffer, 1, &result, flags, to, tolen, nullptr, nullptr) == 0 ? static_cast<int>(result) : SOCKET_ERROR;
}
HOOK_EXPORT int WSAAPI HookRecv(SOCKET s, char *buf, int len, int flags)
{
	DWORD result = 0, flags2 = flags;
	WSABUF buffer = { static_cast<ULONG>(len), buf };

	return HookWSARecv(s, &buffer, 1, &result, &flags2, nullptr, nullptr) == 0 ? static_cast<int>(result) : SOCKET_ERROR;
}
HOOK_EXPORT int WSAAPI HookRecvFrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
	DWORD result = 0, flags2 = flags;
	WSABUF buffer = { static_cast<ULONG>(len), buf };

	return HookWSARecvFrom(s, &buffer, 1, &result, &flags2, from, fromlen, nullptr, nullptr) == 0 ? static_cast<int>(result) : SOCKET_ERROR;
}
