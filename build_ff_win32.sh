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
--enable-protocol=file \
--enable-muxer=spdif \
--arch=x86 --target-os=mingw32"

./configure --extra-cflags="-march=i686 -mmmx" ${OPTIONS} &&
 
make -j8 &&
cp lib*/*-*.dll ../bin_Win32 &&
cp lib*/*.lib ../bin_Win32/lib &&
cp lib*/*-*.dll ../bin_Win32d &&
cp lib*/*.lib ../bin_Win32d/lib &&

cd ..
