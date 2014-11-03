#define _ELPP_STL_LOGGING
#include "../../src/easylogging++.h"

_INITIALIZE_EASYLOGGINGPP

int main(int argc, char** argv) {
	LOG(INFO) << "Hello Dev C++!";
	return 0;
}
