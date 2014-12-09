#include "Runtime.hpp"
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
			inline Lock(CriticalSection &cs) : CS(cs)
			{
				this->CS.Enter();
			}
			inline ~Lock()
			{
				this->CS.Leave();
			}

			CriticalSection &CS;

		private:
			void operator =(const Lock &);
		};

	public:
		inline CriticalSection()
		{
			::InitializeCriticalSection(&this->mCS);
		}
		inline ~CriticalSection()
		{
			::DeleteCriticalSection(&this->mCS);
		}

		inline void Enter()
		{
			::EnterCriticalSection(&this->mCS);
		}
		inline void Leave()
		{
			::LeaveCriticalSection(&this->mCS);
		}

	private:
		CRITICAL_SECTION mCS;
	};

	CriticalSection sCS;
	
	void NetworkUpload(const char *buf, unsigned int len)
	{
		CriticalSection::Lock lock(sCS);

		ReShade::Runtime::NetworkTrafficUpload += len;
	}
	void NetworkDownload(unsigned int len)
	{
		CriticalSection::Lock lock(sCS);

		ReShade::Runtime::NetworkTrafficDownload += len;
	}
}

// -----------------------------------------------------------------------------------------------------

EXPORT int WSAAPI send(SOCKET s, const char *buf, int len, int flags)
{
	static const auto trampoline = ReShade::Hooks::Call(&send);

	NetworkUpload(buf, len);

	return trampoline(s, buf, len, flags);
}
EXPORT int WSAAPI sendto(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
	static const auto trampoline = ReShade::Hooks::Call(&sendto);

	NetworkUpload(buf, len);

	return trampoline(s, buf, len, flags, to, tolen);
}
EXPORT int WSAAPI recv(SOCKET s, char *buf, int len, int flags)
{
	static const auto trampoline = ReShade::Hooks::Call(&recv);

	const int recieved = trampoline(s, buf, len, flags);

	if (recieved > 0)
	{
		NetworkDownload(recieved);
	}

	return recieved;
}
EXPORT int WSAAPI recvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen)
{
	static const auto trampoline = ReShade::Hooks::Call(&recvfrom);

	const int recieved = trampoline(s, buf, len, flags, from, fromlen);

	if (recieved > 0)
	{
		NetworkDownload(recieved);
	}

	return recieved;
}

EXPORT int WSAAPI WSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReShade::Hooks::Call(&WSASend);

	for (DWORD i = 0; i < dwBufferCount; ++i)
	{
		NetworkUpload(lpBuffers[i].buf, lpBuffers[i].len);
	}

	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpOverlapped, lpCompletionRoutine);
}
EXPORT int WSAAPI WSASendTo(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, const struct sockaddr *lpTo, int iToLen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReShade::Hooks::Call(&WSASendTo);

	for (DWORD i = 0; i < dwBufferCount; ++i)
	{
		NetworkUpload(lpBuffers[i].buf, lpBuffers[i].len);
	}

	return trampoline(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags, lpTo, iToLen, lpOverlapped, lpCompletionRoutine);
}
EXPORT int WSAAPI WSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReShade::Hooks::Call(&WSARecv);

	DWORD recieved = 0;
	const int result = trampoline(s, lpBuffers, dwBufferCount, &recieved, lpFlags, lpOverlapped, lpCompletionRoutine);

	if (lpNumberOfBytesRecvd != nullptr)
	{
		*lpNumberOfBytesRecvd = recieved;
	}

	if (lpOverlapped == nullptr)
	{
		if (result == SOCKET_ERROR)
		{
			return SOCKET_ERROR;
		}
	}
	else
	{
		recieved = 0;

		for (DWORD i = 0; i < dwBufferCount; ++i)
		{
			recieved += lpBuffers[i].len;
		}
	}

	if (recieved != 0)
	{
		NetworkDownload(recieved);
	}

	return result;
}
EXPORT int WSAAPI WSARecvEx(SOCKET s, char *buf, int len, int *flags)
{
	static const auto trampoline = ReShade::Hooks::Call(&WSARecvEx);

	const int recieved = trampoline(s, buf, len, flags);

	if (recieved > 0)
	{
		NetworkDownload(recieved);
	}

	return recieved;
}
EXPORT int WSAAPI WSARecvFrom(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, struct sockaddr *lpFrom, LPINT lpFromlen, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	static const auto trampoline = ReShade::Hooks::Call(&WSARecvFrom);

	DWORD recieved = 0;
	const int result = trampoline(s, lpBuffers, dwBufferCount, &recieved, lpFlags, lpFrom, lpFromlen, lpOverlapped, lpCompletionRoutine);

	if (lpNumberOfBytesRecvd != nullptr)
	{
		*lpNumberOfBytesRecvd = recieved;
	}

	if (lpOverlapped == nullptr)
	{
		if (result == SOCKET_ERROR)
		{
			return SOCKET_ERROR;
		}
	}
	else
	{
		recieved = 0;

		for (DWORD i = 0; i < dwBufferCount; ++i)
		{
			recieved += lpBuffers[i].len;
		}
	}

	if (recieved != 0)
	{
		NetworkDownload(recieved);
	}

	return result;
}