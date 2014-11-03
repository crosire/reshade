#include <mylib.hpp>
#include "easylogging++.h"

_SHARE_EASYLOGGINGPP(MyLib::getEasyloggingStorage())

int main(int argc, char** argv) {
    LOG(INFO) << "User: " << el::base::utils::s_currentUser;
    LOG(INFO) << "Host: " << el::base::utils::s_currentHost;

    {
        // Braces only for scoping purpose - remove and compare results
        MyLib lib(argc, argv);
        lib.event(1);
    }
    LOG(INFO) << std::boolalpha
              << "Has mylib LOGGER: " << MyLib::getEasyloggingStorage()->registeredLoggers()->has("mylib")
              << std::endl
              << "ARE BOTH STORAGES EQUAL: " << (bool)(MyLib::getEasyloggingStorage() == el::Helpers::storage());
    // See read me
    el::Logger* libLogger = CHECK_NOTNULL(el::Loggers::getLogger("mylib", false));
    libLogger->info("This info log is using logger->info(...) with arg %v and %v", 1, 2);
    CLOG(INFO, "mylib") << "A logger initialized in shared storage";
    CLOG(INFO, "default") << "Default, configuration from shared storage";
    return 0;
}
