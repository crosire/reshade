#define _ELPP_STL_LOGGING
// #define _ELPP_STACKTRACE_ON_CRASH -- Stack trace not available for MinGW GCC
#define _ELPP_PERFORMANCE_MICROSECONDS
#define _ELPP_LOG_STD_ARRAY
#define _ELPP_LOG_UNORDERED_MAP
#define _ELPP_UNORDERED_SET
#include "easylogging++.h"

_INITIALIZE_EASYLOGGINGPP

TIMED_SCOPE(appTimer, "myapplication");


int main(int argc, const char* argv[]) {
    el::Helpers::removeFlag(el::LoggingFlag::AllowVerboseIfModuleNotSpecified);

    TIMED_BLOCK(itr, "write-simple") {
        LOG(INFO) << "Test " << __FILE__;
    }
    VLOG(3) << "Test";
}
