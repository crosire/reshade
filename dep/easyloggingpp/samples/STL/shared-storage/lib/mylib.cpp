#include "mylib.hpp"
#include "easylogging++.h"

_INITIALIZE_EASYLOGGINGPP

int MyLib::runOnceHelper = runOnce();

int MyLib::runOnce() {
    LOG(INFO) << "Registering logger [mylib]";
    el::Loggers::getLogger("mylib");
    return 0;
}

MyLib::MyLib() {
    LOG(INFO) << "---MyLib Constructor () ---";
}

MyLib::MyLib(int argc, char** argv) {
    _START_EASYLOGGINGPP(argc, argv);
    LOG(INFO) << "---MyLib Constructor(int, char**) ---";
}

MyLib::~MyLib() {
    LOG(INFO) << "---MyLib Destructor---";
}


void MyLib::event(int a) {
    VLOG(1) << "MyLib::event start";
    LOG(INFO) << "Processing event [" << a << "]";
    VLOG(1) << "MyLib::event end";
}

el::base::type::StoragePointer MyLib::getEasyloggingStorage() {
        return el::Helpers::storage();
}
