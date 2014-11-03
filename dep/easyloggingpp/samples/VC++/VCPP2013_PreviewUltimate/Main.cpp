#include "easylogging++.h"

_INITIALIZE_EASYLOGGINGPP

TIMED_SCOPE(appTimer, "myapplication");


int main(int argc, const char* argv []) {
    el::Helpers::removeFlag(el::LoggingFlag::AllowVerboseIfModuleNotSpecified);


    TIMED_BLOCK(itr, "write-simple") {
        LOG(INFO) << "Test " << __FILE__;
    }
    
    VLOG(3) << "Test";
    system("pause");
}
