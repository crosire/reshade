## Helper script for build_all.sh

FILE=$1

macro="$macro -D_ELPP_DEBUG_ERRORS"
macro="$macro -D_ELPP_THREAD_SAFE"
macro="$macro -D_ELPP_STL_LOGGING"
macro="$macro -D_ELPP_LOG_UNORDERED_SET"
macro="$macro -D_ELPP_LOG_UNORDERED_MAP"
macro="$macro -D_ELPP_STACKTRACE_ON_CRASH"
macro="$macro -D_ELPP_LOGGING_FLAGS_FROM_ARG"
# macro="$macro -D_ELPP_DEFAULT_LOG_FILE=\"/a/path/that/does/not/exist/f.log\""

if [ "$2" = "" ];then
  COMPILER=g++
else
  COMPILER=$2
fi

CXX_STD='-std=c++0x -pthread'

if [ "$FILE" = "" ]; then
  echo "Please provide filename to compile"
  exit
fi

echo "Compiling... [$FILE]"

COMPILE_LINE="$COMPILER $FILE -o bin/$FILE.bin $macro $CXX_STD -Wall -Wextra -pedantic -pedantic-errors -Werror -Wfatal-errors"

echo "    $COMPILE_LINE"

$($COMPILE_LINE)

echo "    DONE! [./bin/$FILE.bin]"
echo
echo
