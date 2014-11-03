#include "easylogging++.h"

class MyLib {
public:
    MyLib();
    MyLib(int, char**);
    ~MyLib();
    void event(int a);
    static el::base::type::StoragePointer getEasyloggingStorage();
private:
    static int runOnceHelper;
    static int runOnce();
};
