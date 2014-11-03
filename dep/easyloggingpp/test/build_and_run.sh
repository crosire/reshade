
# Builds all tests into bin/ and runs

[ -d "bin" ] || mkdir "bin"

cd bin
echo "Building..."
qmake-qt4 ../qt-gtest-proj-intel.pro
make
echo "Running..."
./qt-gtest-proj-intel
cd ..
echo "Completed!"
