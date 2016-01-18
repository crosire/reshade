#pragma once

#include <Windows.h>

namespace reshade
{
	namespace utils
	{
		class critical_section
		{
		public:
			struct Lock
			{
				Lock(critical_section &cs) : _cs(cs)
				{
					EnterCriticalSection(&_cs._cs);
				}
				~Lock()
				{
					LeaveCriticalSection(&_cs._cs);
				}

				critical_section &_cs;

			private:
				Lock(const Lock &);
				void operator=(const Lock &);
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
	}
}
