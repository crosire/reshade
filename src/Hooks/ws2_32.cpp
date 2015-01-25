#include "Network.hpp"
#include "HookManager.hpp"

#include <Winsock2.h>

// -----------------------------------------------------------------------------------------------------

namespace
{
	class CriticalSection
	{
	public:
		struct Lock
		{
			Lock(CriticalSection &cs) : CS(cs)
			{
				EnterCriticalSection(&this->CS.mCS);
			}
			~Lock()
			{
				LeaveCriticalSection(&this->CS.mCS);
			}

			CriticalSection &CS;

		private:
			void operator =(const Lock &);
		};

	public:
		CriticalSection()
		{
			InitializeCriticalSection(&this->mCS);
		}
		~CriticalSection()
		{
			DeleteCriticalSection(&this->mCS);
		}

	private:
		CRITICAL_SECTION mCS;
	} sCS;
}

// -----------------------------------------------------------------------------------------------------

EXPORT int WSAAPI send(SOCKET s, const char *buf, int len, int flags)
{
	DWORD sent = 0;
	WSABUF data = { len, const_cast<CHAR *>(buf) };

	if (WSASend(s, &data, 1, &sent, flags, nullptr, nullptr) == SOCKET_ERROR)
	{
		return SOCKET_ERROR;
	}

	return static_cast<int>(sent);
}
EXPORT int WSAAPI sendto(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
	DWORD sent = 0;
	WSABUF data = { len, const_cast<CHAR *>(buf) };

	if (WSASendTo(s, &data, 1, &sent, flags, to, tolen, nullptr, nullptr) == SOCKET_ERROR)
	{
		return SOCKET_ERROR;
	}

	return static_cast<int>(sent);
}
EXPORT int WSAAPI recv(SOCKET s, char *buf, int len, int flags)
{
	DWORD recieved = 0;
	WSABUF data = { len, buf };

	if (WSARecv(s, &data, 1, &recieved, reinterpret_cast<LPDWORD>(&flags), nullptr, nullptr) == SOCKET_ERROR)
	{
		return SOCKET_ERROR;
	}

	return static_cast<int>(recieved);
}
EXPORT int WSAAPI recvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
	DWORD recieved = 0;
	WSABUF data = { len, buf };

	if (WSARecvFrom(s, &data, 1, &recieved, reinterpret_cast<LPDWORD>(&flags), from, fromlen, nullptr, nullptr) == SOCKET_ERROR)
	{
		return SOCKET_ERROR;
	}

	return static_cast<int>(recieved);
}

EXPORT int WSAAPI WSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReShade::Hooks::Call(&WSASend);

	{
		CriticalSection::Lock lock(sCS);

		for (DWORD i = 0; i < dwBufferCount; ++i)
		{
			ReShade::Network::OnSend(lpBuffers[i].buf, lpBuffers[i].len);
		}
	}

	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
}
EXPORT int WSAAPI WSASendTo(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const struct sockaddr *lpTo, int iToLen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReShade::Hooks::Call(&WSASendTo);

	{
		CriticalSection::Lock lock(sCS);

		for (DWORD i = 0; i < dwBufferCount; ++i)
		{
			ReShade::Network::OnSend(lpBuffers[i].buf, lpBuffers[i].len);
		}
	}

	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iToLen, lpOverlapped, lpCompletionRoutine);
}
EXPORT int WSAAPI WSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReShade::Hooks::Call(&WSARecv);

	DWORD recieved = 0;

	const int status = trampoline(s, lpBuffers, dwBufferCount, &recieved, lpFlags, lpOverlapped, lpCompletionRoutine);

	if (lpNumberOfBytesRecvd != nullptr)
	{
		*lpNumberOfBytesRecvd = recieved;
	}

	if (status == 0 && recieved > 0)
	{
		CriticalSection::Lock lock(sCS);

		for (DWORD i = 0; i < dwBufferCount; recieved -= lpBuffers[i++].len)
		{
			ReShade::Network::OnRecieve(lpBuffers[i].buf, std::min(recieved, lpBuffers[i].len));
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
		CriticalSection::Lock lock(sCS);

		ReShade::Network::OnRecieve(buf, recieved);
	}

	return recieved;
}
EXPORT int WSAAPI WSARecvFrom(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, struct sockaddr *lpFrom, LPINT lpFromlen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReShade::Hooks::Call(&WSARecvFrom);

	DWORD recieved = 0;

	const int status = trampoline(s, lpBuffers, dwBufferCount, &recieved, lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine);

	if (lpNumberOfBytesRecvd != nullptr)
	{
		*lpNumberOfBytesRecvd = recieved;
	}

	if (status == 0 && recieved > 0)
	{
		CriticalSection::Lock lock(sCS);

		for (DWORD i = 0; i < dwBufferCount; recieved -= lpBuffers[i++].len)
		{
			ReShade::Network::OnRecieve(lpBuffers[i].buf, std::min(recieved, lpBuffers[i].len));
		}
	}

	return status;
}