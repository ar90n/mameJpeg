curl -o catch.hpp https://raw.githubusercontent.com/philsquared/Catch/master/single_include/catch.hpp

mkdir build
cd build
cmake ..
make
cd ..
