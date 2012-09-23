libopus build instructions
---------------------------

x86:
./configure --enable-static
make

x64:
./configure --enable-static --host=x86_64-w64-mingw32
make
