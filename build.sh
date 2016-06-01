git submodule init
git submodule update

mkdir build
cd build

cmake ..
make -j4

gcc ../demo.c -I.. -L. -Lextlib/stxxl/lib -lstxxl_writer -lstxxl_relwithdebinfo -lstdc++ -lm -lpthread -o demo