/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "hook_manager.hpp"
#include <Windows.h>
#include <Winsock2.h>

volatile long g_network_traffic = 0;

HOOK_EXPORT int WSAAPI HookWSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	for (DWORD i = 0; i < dwBufferCount; ++i)
		InterlockedAdd(&g_network_traffic, lpBuffers[i].len);

	static const auto trampoline = reshade::hooks::call(HookWSASend);
	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
}
HOOK_EXPORT int WSAAPI HookWSASendTo(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const struct sockaddr *lpTo, int iToLen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	for (DWORD i = 0; i < dwBufferCount; ++i)
		InterlockedAdd(&g_network_traffic, lpBuffers[i].len);

	static const auto trampoline = reshade::hooks::call(HookWSASendTo);
	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iToLen, lpOverlapped, lpCompletionRoutine);
}
HOOK_EXPORT int WSAAPI HookWSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = reshade::hooks::call(HookWSARecv);
	const auto status = trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);

	if (status == 0 && lpNumberOfBytesRecvd != nullptr)
		InterlockedAdd(&g_network_traffic, *lpNumberOfBytesRecvd);

	return status;
}
HOOK_EXPORT int WSAAPI HookWSARecvFrom(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, struct sockaddr *lpFrom, LPINT lpFromlen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = reshade::hooks::call(HookWSARecvFrom);
	const auto status = trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine);

	if (status == 0 && lpNumberOfBytesRecvd != nullptr)
		InterlockedAdd(&g_network_traffic, *lpNumberOfBytesRecvd);

	return status;
}

HOOK_EXPORT int WSAAPI HookSend(SOCKET s, const char *buf, int len, int flags)
{
	static const auto trampoline = reshade::hooks::call(HookSend);
	const auto num_bytes_send = trampoline(s, buf, len, flags);

	if (num_bytes_send != SOCKET_ERROR)
		InterlockedAdd(&g_network_traffic, num_bytes_send);

	return num_bytes_send;
}
HOOK_EXPORT int WSAAPI HookSendTo(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
	static const auto trampoline = reshade::hooks::call(HookSendTo);
	const auto num_bytes_send = trampoline(s, buf, len, flags, to, tolen);

	if (num_bytes_send != SOCKET_ERROR)
		InterlockedAdd(&g_network_traffic, num_bytes_send);

	return num_bytes_send;
}
HOOK_EXPORT int WSAAPI HookRecv(SOCKET s, char *buf, int len, int flags)
{
	static const auto trampoline = reshade::hooks::call(HookRecv);
	const auto num_bytes_recieved = trampoline(s, buf, len, flags);

	if (num_bytes_recieved != SOCKET_ERROR)
		InterlockedAdd(&g_network_traffic, num_bytes_recieved);

	return num_bytes_recieved;
}
HOOK_EXPORT int WSAAPI HookRecvFrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
	static const auto trampoline = reshade::hooks::call(HookRecvFrom);
	const auto num_bytes_recieved = trampoline(s, buf, len, flags, from, fromlen);

	if (num_bytes_recieved != SOCKET_ERROR)
		InterlockedAdd(&g_network_traffic, num_bytes_recieved);

	return num_bytes_recieved;
}
