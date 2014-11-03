#define _ELPP_AS_DLL // Tells Easylogging++ that it's used for DLL
#define _ELPP_EXPORT_SYMBOLS // Tells Easylogging++ to export symbols
#define MYMATHS_EXPORTS

#include "MyMaths.h"

_INITIALIZE_EASYLOGGINGPP

el::base::type::StoragePointer sharedLoggingRepository() {
	return el::Helpers::storage();
}

namespace Math
{
    int MyMaths::add(int x, int y)
    {
        return x + y;
    }

	void MyMaths::logAdd(int x, int y) {
		LOG(INFO) << add(x, y);
	}
}
