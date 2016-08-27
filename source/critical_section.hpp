#pragma once

#include <Windows.h>

class critical_section
{
public:
	struct lock
	{
		lock(critical_section &cs) : _cs(cs)
		{
			EnterCriticalSection(&_cs._cs);
		}
		~lock()
		{
			LeaveCriticalSection(&_cs._cs);
		}

		critical_section &_cs;

	private:
		lock(const lock &) = delete;
		void operator=(const lock &) = delete;
	};

	critical_section()
	{
		InitializeCriticalSection(&_cs);
	}
	~critical_section()
	{
		DeleteCriticalSection(&_cs);
	}

private:
	CRITICAL_SECTION _cs;
};
