#define _ELPP_NO_DEFAULT_LOG_FILE
#define _ELPP_DISABLE_DEFAULT_CRASH_HANDLING
#define _ELPP_DISABLE_LOG_FILE_FROM_ARG
#define _ELPP_DISABLE_LOGGING_FLAGS_FROM_ARG
#define _ELPP_DISABLE_CUSTOM_FORMAT_SPECIFIERS

#ifdef _DEBUG
	#define _ELPP_DEBUG_ERRORS
	#define _ELPP_DEBUG_ASSERT_FAILURE
#endif

#pragma warning(push)
#pragma warning(disable: 4127)

#include <iomanip>
#include <easylogging++.h>

#pragma warning(pop)