echo "Compiling unit tests..."
clang++ main.cc -lgtest -std=c++11 -lpthread -o unit_test -D_ELPP_DEFAULT_LOG_FILE='"logs/el.gtest.log"'
echo "Running unit tests..."
./unit_test -v
result=$?
rm -r unit_test logs
echo "Unit tests completed : $result"
exit $result
