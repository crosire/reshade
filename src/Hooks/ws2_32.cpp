#include "HookManager.hpp"

#include <Winsock2.h>

// -----------------------------------------------------------------------------------------------------

EXPORT int WSAAPI send(SOCKET s, const char *buf, int len, int flags)
{
	static const auto trampoline = ReHook::Call(&send);

	return trampoline(s, buf, len, flags);
}
EXPORT int WSAAPI sendto(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
	static const auto trampoline = ReHook::Call(&sendto);

	return trampoline(s, buf, len, flags, to, tolen);
}
EXPORT int WSAAPI recv(SOCKET s, char *buf, int len, int flags)
{
	static const auto trampoline = ReHook::Call(&recv);

	return trampoline(s, buf, len, flags);
}
EXPORT int WSAAPI recvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
	static const auto trampoline = ReHook::Call(&recvfrom);

	return trampoline(s, buf, len, flags, from, fromlen);
}

EXPORT int WSAAPI WSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReHook::Call(&WSASend);

	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
}
EXPORT int WSAAPI WSASendTo(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const struct sockaddr *lpTo, int iToLen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReHook::Call(&WSASendTo);

	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iToLen, lpOverlapped, lpCompletionRoutine);
}
EXPORT int WSAAPI WSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReHook::Call(&WSARecv);

	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);
}
EXPORT int WSAAPI WSARecvEx(SOCKET s, char *buf, int len, int *flags)
{
	static const auto trampoline = ReHook::Call(&WSARecvEx);

	return trampoline(s, buf, len, flags);
}
EXPORT int WSAAPI WSARecvFrom(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, struct sockaddr *lpFrom, LPINT lpFromlen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReHook::Call(&WSARecvFrom);

	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine);
}