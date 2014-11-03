#include <wx/vector.h>
#include "easylogging++.h"

_INITIALIZE_EASYLOGGINGPP

int main() {
    wxVector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    LOG(INFO) << vec;
}
