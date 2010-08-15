#!/bin/bash 

if [ ! -d bin_x64/lib ]
then
mkdir -p bin_x64/lib
fi

cd ffmpeg

make distclean

OPTIONS="
--enable-shared \
--enable-memalign-hack \
--enable-gpl \
--enable-w32threads \
--enable-runtime-cpudetect \
--disable-postproc \
--enable-zlib \
--disable-static \
--disable-altivec \
--disable-muxers \
--disable-encoders \
--disable-debug \
--enable-ffplay \
--disable-ffserver \
--disable-ffmpeg \
--enable-ffprobe \
--disable-devices \
--disable-filters \
--disable-avfilter \
--disable-swscale \
--disable-avdevice \
--disable-hwaccels \
--disable-bsfs \
--enable-cross-compile \
--cross-prefix=x86_64-w64-mingw32- --arch=x86_64 --target-os=mingw32"

./configure --extra-cflags="-U__STRICT_ANSI__ -fno-strict-aliasing -fno-common" ${OPTIONS} &&
 
make -j4 &&
cp lib*/*-*.dll ../bin_x64 &&
cp lib*/*.lib ../bin_x64/lib &&
cp ffprobe.exe ../bin_x64

cd ..
