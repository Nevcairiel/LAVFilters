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
--enable-w32threads \
--enable-runtime-cpudetect \
--enable-asm \
--enable-zlib \
--enable-swscale \
--enable-avfilter \
--disable-filters \
--enable-filter=yadif \
--enable-filter=buffer \
--enable-filter=buffersink \
--enable-filter=scale \
--disable-protocols \
--enable-protocol=file \
--enable-protocol=rtsp \
--disable-muxers \
--enable-muxer=spdif \
--disable-hwaccels \
--enable-hwaccel=h264_dxva2 \
--enable-hwaccel=vc1_dxva2 \
--enable-hwaccel=wmv3_dxva2 \
--enable-hwaccel=mpeg2_dxva2 \
--disable-swresample \
--disable-postproc \
--disable-static \
--disable-altivec \
--disable-encoders \
--disable-debug \
--disable-ffplay \
--disable-ffserver \
--disable-ffmpeg \
--disable-avconv \
--disable-ffprobe \
--disable-devices \
--disable-avdevice \
--disable-bsfs \
--arch=x86 --cpu=i686 --target-os=mingw32 \
--build-suffix=-lav"

./configure --extra-cflags="-I../common/includes/dxva2 -mmmx -msse" ${OPTIONS} &&
 
make -j8 &&
cp lib*/*-lav-*.dll ../bin_Win32 &&
cp lib*/*.lib ../bin_Win32/lib &&
cp lib*/*-lav-*.dll ../bin_Win32d &&
cp lib*/*.lib ../bin_Win32d/lib &&

cd ..
