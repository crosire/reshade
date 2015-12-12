#pragma once

#include <Windows.h>

namespace ReShade
{
	namespace Utils
	{
		class CriticalSection
		{
		public:
			struct Lock
			{
				Lock(CriticalSection &cs) : _cs(cs)
				{
					EnterCriticalSection(&_cs._cs);
				}
				~Lock()
				{
					LeaveCriticalSection(&_cs._cs);
				}

				CriticalSection &_cs;

			private:
				Lock(const Lock &);
				void operator=(const Lock &);
			};

			CriticalSection()
			{
				InitializeCriticalSection(&_cs);
			}
			~CriticalSection()
			{
				DeleteCriticalSection(&_cs);
			}

		private:
			CRITICAL_SECTION _cs;
		};
	}
}
