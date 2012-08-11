libopus build instructions
---------------------------

x86:
./configure --disable-stack-protector --enable-static
make

x64:
./configure --disable-stack-protector --enable-static --host=x86_64-w64-mingw32

