#!/bin/bash 

if [ ! -d bin_Win32/lib ]
then
mkdir -p bin_Win32/lib
fi

if [ ! -d bin_Win32d/lib ]
then
mkdir -p bin_Win32d/lib
fi

cd ffmpeg

make distclean

OPTIONS="
--enable-shared \
--enable-memalign-hack \
--enable-gpl \
--enable-w32threads \
--enable-runtime-cpudetect \
--enable-asm \
--disable-postproc \
--enable-zlib \
--disable-static \
--disable-altivec \
--disable-muxers \
--disable-encoders \
--disable-debug \
--disable-ffplay \
--disable-ffserver \
--disable-ffmpeg \
--disable-ffprobe \
--disable-devices \
--disable-filters \
--disable-avfilter \
--disable-swscale \
--disable-avdevice \
--disable-hwaccels \
--disable-bsfs \
--disable-network \
--disable-protocols \
--arch=x86 --target-os=mingw32"

./configure --extra-cflags="-U__STRICT_ANSI__ -march=i686 -mmmx -DFF_API_MAX_STREAMS=0" ${OPTIONS} &&
 
make -j8 &&
cp lib*/*-*.dll ../bin_Win32 &&
cp lib*/*.lib ../bin_Win32/lib &&
cp lib*/*-*.dll ../bin_Win32d &&
cp lib*/*.lib ../bin_Win32d/lib &&

cd ..

cd libbluray
autoreconf -vf &&
./configure &&
echo "#undef HAVE_LIBXML2" >> config.h
echo "#undef __STRICT_ANSI__" >> config.h
make -j8 &> /dev/null &&

cd src/.libs &&
gcc -shared -o libbluray.dll *.o -Wl,--out-implib,libbluray.dll.a -Wl,--output-def,libbluray.def &&
lib.exe /machine:x86 /def:libbluray.def /out:libbluray.lib &&

cp libbluray.dll ../../../bin_Win32 &&
cp libbluray.dll ../../../bin_Win32d &&
cp libbluray.lib ../../../bin_Win32/lib &&
cp libbluray.lib ../../../bin_Win32d/lib

cd ../../..
