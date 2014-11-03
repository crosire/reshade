#ifdef MYMATHS_EXPORTS
#define MYMATHDLL_EXPORT __declspec(dllexport) 
#else
#define MYMATHDLL_EXPORT __declspec(dllimport) 
#endif
#include "../../../../src/easylogging++.h"

MYMATHDLL_EXPORT el::base::type::StoragePointer sharedLoggingRepository();

namespace Math {
	class MyMaths {
	public:
		static MYMATHDLL_EXPORT int add(int x, int y);
		static MYMATHDLL_EXPORT void logAdd(int x, int y);
	};
}