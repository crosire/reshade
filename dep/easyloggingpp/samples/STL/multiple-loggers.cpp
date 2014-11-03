 //
 // This file is part of Easylogging++ samples
 // Very basic sample - log using multiple loggers
 //
 // Revision 1.0
 // @author mkhan3189
 //

// You can also uncomment this line and achieve same result using LOG(..) macro
// #define _LOGGER "default", "network"

#include "easylogging++.h"

_INITIALIZE_EASYLOGGINGPP

int main(void) {

    el::Loggers::addFlag(el::LoggingFlag::MultiLoggerSupport); // Enables support for multiple loggers

    el::Loggers::getLogger("network"); // Register 'network' logger
    
    CLOG(INFO, "default", "network") << "My first log message that writes with network and default loggers";

    return 0;
}
