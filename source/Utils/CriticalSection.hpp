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
		};
	}
}
