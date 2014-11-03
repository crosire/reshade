 //
 // This file is part of Easylogging++ samples
 //
 // Demonstration on how locale gives output
 //
 // Revision 1.1
 // @author mkhan3189
 //
#ifndef _ELPP_UNICODE
#   define _ELPP_UNICODE
#endif

#include "easylogging++.h"

_INITIALIZE_EASYLOGGINGPP

int main(int argc, const char** argv) {
    _START_EASYLOGGINGPP(argc, argv);

    LOG(INFO) << L"世界，你好";
    return 0;
}
