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
--arch=x86 --cpu=i686 --target-os=mingw32 \
--build-suffix=-lav"

./configure --extra-libs="-lwsock32" --extra-cflags="-mmmx -msse -DPTW32_STATIC_LIB" ${OPTIONS} &&
 
make -j8 &&
cp lib*/*-lav-*.dll ../bin_Win32 &&
cp lib*/*.lib ../bin_Win32/lib &&
cp lib*/*-lav-*.dll ../bin_Win32d &&
cp lib*/*.lib ../bin_Win32d/lib &&

cd ..
