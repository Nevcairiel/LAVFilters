#!/bin/bash 

cd ffmpeg

if [ -d .libs ]
then
rm -r .libs
fi

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
--disable-bsfs"

./configure --extra-cflags="-U__STRICT_ANSI__ -fno-strict-aliasing -fno-common" ${OPTIONS} &&
 
make -j3 && 
mkdir .libs &&
cp lib*/*-*.dll .libs/ &&
cp lib*/*.def .libs/ &&
cp lib*/*.lib .libs/ &&
cp lib*/*.exp .libs/ &&
cp .libs/*  ../lib
cp .libs/*.dll ../bin
cp libavutil/avconfig.h include/libavutil/

cd ..
