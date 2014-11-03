#include "easylogging++.h"
#include <glibmm/ustring.h>

_INITIALIZE_EASYLOGGINGPP
 
int main(int, char**){

    Glib::ustring s("My GTK");
    LOG(INFO) << s; 

    return 0;
}
