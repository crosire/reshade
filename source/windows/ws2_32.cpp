/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "hook_manager.hpp"
#include <Windows.h>
#include <Winsock2.h>
#include <ws2ipdef.h>

// Do not count network traffic for local sockets (localhost), so to avoid blocking occuring in games running local servers in single player too
static bool is_local_socket(SOCKET s)
{
	// Get the target address information of the socket
	int address_size = sizeof(SOCKADDR_STORAGE);
	SOCKADDR_STORAGE peer_address = {};
	if (getpeername(s, reinterpret_cast<PSOCKADDR>(&peer_address), &address_size) == SOCKET_ERROR)
		return false;

	const int32_t ipv4_loopback = INADDR_LOOPBACK; // 127.0.0.1
	const uint8_t ipv6_loopback[16] = IN6ADDR_LOOPBACK_INIT; // ::1
	const uint8_t ipv6_ipv4_loopback[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 127, 0, 0, 1 }; // ::ffff:127.0.0.1
	static_assert(sizeof(ipv4_loopback) == sizeof(IN_ADDR) && sizeof(ipv6_loopback) == sizeof(IN6_ADDR));

	// Check if the target address matches the loopback address, in which case the connection is local
	switch (peer_address.ss_family)
	{
	default:
		return false;
	case AF_INET:
		return std::memcmp(&reinterpret_cast<PSOCKADDR_IN >(&peer_address)->sin_addr, &ipv4_loopback, sizeof(IN_ADDR)) == 0;
	case AF_INET6:
		return std::memcmp(&reinterpret_cast<PSOCKADDR_IN6>(&peer_address)->sin6_addr, ipv6_loopback, sizeof(IN6_ADDR)) == 0 ||
		       std::memcmp(&reinterpret_cast<PSOCKADDR_IN6>(&peer_address)->sin6_addr, ipv6_ipv4_loopback, sizeof(IN6_ADDR)) == 0;
	}
}

volatile long g_network_traffic = 0;

HOOK_EXPORT int WSAAPI HookWSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	if (!is_local_socket(s))
		for (DWORD i = 0; i < dwBufferCount; ++i)
			InterlockedAdd(&g_network_traffic, lpBuffers[i].len);

	static const auto trampoline = reshade::hooks::call(HookWSASend);
	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
}
HOOK_EXPORT int WSAAPI HookWSASendTo(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const struct sockaddr *lpTo, int iToLen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	if (!is_local_socket(s))
		for (DWORD i = 0; i < dwBufferCount; ++i)
			InterlockedAdd(&g_network_traffic, lpBuffers[i].len);

	static const auto trampoline = reshade::hooks::call(HookWSASendTo);
	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iToLen, lpOverlapped, lpCompletionRoutine);
}
HOOK_EXPORT int WSAAPI HookWSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = reshade::hooks::call(HookWSARecv);
	const auto status = trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);

	if (status == 0 && lpNumberOfBytesRecvd != nullptr && !is_local_socket(s))
		InterlockedAdd(&g_network_traffic, *lpNumberOfBytesRecvd);

	return status;
}
HOOK_EXPORT int WSAAPI HookWSARecvFrom(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, struct sockaddr *lpFrom, LPINT lpFromlen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = reshade::hooks::call(HookWSARecvFrom);
	const auto status = trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine);

	if (status == 0 && lpNumberOfBytesRecvd != nullptr && !is_local_socket(s))
		InterlockedAdd(&g_network_traffic, *lpNumberOfBytesRecvd);

	return status;
}

HOOK_EXPORT int WSAAPI HookSend(SOCKET s, const char *buf, int len, int flags)
{
	static const auto trampoline = reshade::hooks::call(HookSend);
	const auto num_bytes_send = trampoline(s, buf, len, flags);

	if (num_bytes_send != SOCKET_ERROR && !is_local_socket(s))
		InterlockedAdd(&g_network_traffic, num_bytes_send);

	return num_bytes_send;
}
HOOK_EXPORT int WSAAPI HookSendTo(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
	static const auto trampoline = reshade::hooks::call(HookSendTo);
	const auto num_bytes_send = trampoline(s, buf, len, flags, to, tolen);

	if (num_bytes_send != SOCKET_ERROR && !is_local_socket(s))
		InterlockedAdd(&g_network_traffic, num_bytes_send);

	return num_bytes_send;
}
HOOK_EXPORT int WSAAPI HookRecv(SOCKET s, char *buf, int len, int flags)
{
	static const auto trampoline = reshade::hooks::call(HookRecv);
	const auto num_bytes_recieved = trampoline(s, buf, len, flags);

	if (num_bytes_recieved != SOCKET_ERROR && !is_local_socket(s))
		InterlockedAdd(&g_network_traffic, num_bytes_recieved);

	return num_bytes_recieved;
}
HOOK_EXPORT int WSAAPI HookRecvFrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
	static const auto trampoline = reshade::hooks::call(HookRecvFrom);
	const auto num_bytes_recieved = trampoline(s, buf, len, flags, from, fromlen);

	if (num_bytes_recieved != SOCKET_ERROR && !is_local_socket(s))
		InterlockedAdd(&g_network_traffic, num_bytes_recieved);

	return num_bytes_recieved;
}
