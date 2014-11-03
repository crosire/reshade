#include <wx/string.h>
#include "easylogging++.h"

_INITIALIZE_EASYLOGGINGPP

int main() {
    wxString str = "This is a simple wxString";
    LOG(INFO) << str;
}
