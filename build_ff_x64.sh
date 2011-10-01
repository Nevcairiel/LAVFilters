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
--enable-gpl \
--enable-version2 \
--enable-pthreads \
--enable-runtime-cpudetect \
--enable-asm \
--disable-postproc \
--enable-zlib \
--enable-swscale \
--disable-swresample \
--disable-static \
--disable-altivec \
--disable-muxers \
--disable-encoders \
--disable-debug \
--disable-ffplay \
--disable-ffserver \
--disable-ffmpeg \
--disable-avconv \
--disable-ffprobe \
--disable-devices \
--disable-filters \
--disable-avfilter \
--disable-avdevice \
--disable-hwaccels \
--disable-bsfs \
--disable-network \
--disable-protocols \
--enable-protocol=file \
--enable-muxer=spdif \
--enable-cross-compile \
--cross-prefix=x86_64-w64-mingw32- --arch=x86_64 --target-os=mingw32 \
--build-suffix=-lav"

./configure --extra-libs="-lwsock32" --extra-cflags="-DPTW32_STATIC_LIB" ${OPTIONS} &&
 
make -j8 &&
cp lib*/*-lav-*.dll ../bin_x64 &&
cp lib*/*.lib ../bin_x64/lib &&
cp lib*/*-lav-*.dll ../bin_x64d &&
cp lib*/*.lib ../bin_x64d/lib &&

cd ..
