 //
 // This file is part of Easylogging++ samples
 // Demonstration of auto spacing functionality
 //
 // Revision 1.1
 // @author mkhan3189
 //

#include "easylogging++.h"

_INITIALIZE_EASYLOGGINGPP

int main(void) {

    LOG(INFO) << "this" << "is" << "a" << "message";
 
    el::Loggers::addFlag(el::LoggingFlag::AutoSpacing);
    LOG(INFO) << "this" << "is" << "a" << "message";    

    return 0;
}
