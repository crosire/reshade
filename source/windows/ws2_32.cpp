/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hook_manager.hpp"
#include <Windows.h>
#include <Winsock2.h>

#if RESHADE_ADDON == 1
#include <ws2ipdef.h>

// Do not count network traffic for local sockets (localhost), so to avoid blocking occuring in games running local servers in single player too
static bool is_local_socket(SOCKET s)
{
	struct network_state_guard
	{
		int error;
		int wsa_error;

		network_state_guard() : error(errno), wsa_error(WSAGetLastError())
		{
		}

		~network_state_guard()
		{
			errno = error;
			WSASetLastError(wsa_error);
		}
	} error_guard;

	// Get the target address information of the socket
	int address_size = sizeof(SOCKADDR_STORAGE);
	SOCKADDR_STORAGE peer_address = {};
	if (getpeername(s, reinterpret_cast<PSOCKADDR>(&peer_address), &address_size) == SOCKET_ERROR)
		return false;

	static const int32_t ipv4_loopback = htonl(INADDR_LOOPBACK); // 127.0.0.1
	static constexpr uint8_t ipv6_loopback[16] = IN6ADDR_LOOPBACK_INIT; // ::1
	static constexpr uint8_t ipv6_ipv4_loopback[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 127, 0, 0, 1 }; // ::ffff:127.0.0.1

	static_assert(sizeof(ipv4_loopback) == sizeof(IN_ADDR) && sizeof(ipv6_loopback) == sizeof(IN6_ADDR));

	// Check if the target address matches the loopback address, in which case the connection is local
	switch (peer_address.ss_family)
	{
	case AF_UNIX:
		// Unix sockets are used for inter-process communication and thus can always be considered local
		return true;
	case AF_INET:
		return std::memcmp(&reinterpret_cast<PSOCKADDR_IN >(&peer_address)->sin_addr, &ipv4_loopback, sizeof(IN_ADDR)) == 0;
	case AF_INET6:
		return std::memcmp(&reinterpret_cast<PSOCKADDR_IN6>(&peer_address)->sin6_addr, ipv6_loopback, sizeof(IN6_ADDR)) == 0 ||
		       std::memcmp(&reinterpret_cast<PSOCKADDR_IN6>(&peer_address)->sin6_addr, ipv6_ipv4_loopback, sizeof(IN6_ADDR)) == 0;
	default:
		return false;
	}
}

volatile long g_network_traffic = 0;
#endif

extern "C" int WSAAPI HookWSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = reshade::hooks::call(HookWSASend);
	const auto status = trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);

#if RESHADE_ADDON == 1
	if (status == 0 && !is_local_socket(s))
		for (DWORD i = 0; i < dwBufferCount; ++i)
			InterlockedAdd(&g_network_traffic, lpBuffers[i].len);
#endif

	return status;
}
extern "C" int WSAAPI HookWSASendTo(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const struct sockaddr *lpTo, int iToLen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = reshade::hooks::call(HookWSASendTo);
	const auto status = trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iToLen, lpOverlapped, lpCompletionRoutine);

#if RESHADE_ADDON == 1
	if (status == 0 && !is_local_socket(s))
		for (DWORD i = 0; i < dwBufferCount; ++i)
			InterlockedAdd(&g_network_traffic, lpBuffers[i].len);
#endif

	return status;
}
extern "C" int WSAAPI HookWSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = reshade::hooks::call(HookWSARecv);
	const auto status = trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);

#if RESHADE_ADDON == 1
	if (status == 0 && lpNumberOfBytesRecvd != nullptr && !is_local_socket(s))
		InterlockedAdd(&g_network_traffic, *lpNumberOfBytesRecvd);
#endif

	return status;
}
extern "C" int WSAAPI HookWSARecvFrom(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, struct sockaddr *lpFrom, LPINT lpFromlen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = reshade::hooks::call(HookWSARecvFrom);
	const auto status = trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine);

#if RESHADE_ADDON == 1
	if (status == 0 && lpNumberOfBytesRecvd != nullptr && !is_local_socket(s))
		InterlockedAdd(&g_network_traffic, *lpNumberOfBytesRecvd);
#endif

	return status;
}

extern "C" int WSAAPI HookSend(SOCKET s, const char *buf, int len, int flags)
{
	static const auto trampoline = reshade::hooks::call(HookSend);
	const auto num_bytes_sent = trampoline(s, buf, len, flags);

#if RESHADE_ADDON == 1
	if (num_bytes_sent != SOCKET_ERROR && !is_local_socket(s))
		InterlockedAdd(&g_network_traffic, num_bytes_sent);
#endif

	return num_bytes_sent;
}
extern "C" int WSAAPI HookSendTo(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
	static const auto trampoline = reshade::hooks::call(HookSendTo);
	const auto num_bytes_sent = trampoline(s, buf, len, flags, to, tolen);

#if RESHADE_ADDON == 1
	if (num_bytes_sent != SOCKET_ERROR && !is_local_socket(s))
		InterlockedAdd(&g_network_traffic, num_bytes_sent);
#endif

	return num_bytes_sent;
}
extern "C" int WSAAPI HookRecv(SOCKET s, char *buf, int len, int flags)
{
	static const auto trampoline = reshade::hooks::call(HookRecv);
	const auto num_bytes_recieved = trampoline(s, buf, len, flags);

#if RESHADE_ADDON == 1
	if (num_bytes_recieved != SOCKET_ERROR && !is_local_socket(s))
		InterlockedAdd(&g_network_traffic, num_bytes_recieved);
#endif

	return num_bytes_recieved;
}
extern "C" int WSAAPI HookRecvFrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
	static const auto trampoline = reshade::hooks::call(HookRecvFrom);
	const auto num_bytes_recieved = trampoline(s, buf, len, flags, from, fromlen);

#if RESHADE_ADDON == 1
	if (num_bytes_recieved != SOCKET_ERROR && !is_local_socket(s))
		InterlockedAdd(&g_network_traffic, num_bytes_recieved);
#endif

	return num_bytes_recieved;
}
