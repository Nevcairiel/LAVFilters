#!/bin/bash 

if [ ! -d bin_x64/lib ]
then
mkdir -p bin_x64/lib
fi

if [ ! -d bin_x64d/lib ]
then
mkdir -p bin_x64d/lib
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
--disable-parser=aac \
--enable-cross-compile \
--cross-prefix=x86_64-w64-mingw32- --arch=x86_64 --target-os=mingw32"

./configure --extra-cflags="-U__STRICT_ANSI__" ${OPTIONS} &&
 
make -j8 &&
cp lib*/*-*.dll ../bin_x64 &&
cp lib*/*.lib ../bin_x64/lib &&
cp lib*/*-*.dll ../bin_x64d &&
cp lib*/*.lib ../bin_x64d/lib &&

cd ..
