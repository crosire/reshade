#include <boost/container/string.hpp>

#include "easylogging++.h"

_INITIALIZE_EASYLOGGINGPP

int main(void) {
   boost::container::string s = "This is boost::container::string";
   LOG(INFO) << s;
}
