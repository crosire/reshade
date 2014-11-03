#include <wx/longlong.h>
#include "easylogging++.h"

_INITIALIZE_EASYLOGGINGPP

int main() {
    wxLongLong l = 264375895;
    LOG(INFO) << l;
}
