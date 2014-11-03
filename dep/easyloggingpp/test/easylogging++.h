// Header for test that sub-includes original header from src/ folder
#ifndef EASYLOGGING_FOR_TEST_H
#define EASYLOGGING_FOR_TEST_H

// We define these macros here for travis to pick up
#define _ELPP_STOP_ON_FIRST_ASSERTION
#define _ELPP_STL_LOGGING
#define _ELPP_FORCE_ENV_VAR_FROM_BASH
#define _ELPP_ENABLE_THREADING
#define _ELPP_STACKTRACE_ON_CRASH
#define _ELPP_SYSLOG
#include "../src/easylogging++.h"
#endif // EASYLOGGING_FOR_SAMPLES_H
