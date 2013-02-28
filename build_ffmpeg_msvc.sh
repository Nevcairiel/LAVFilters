#!/bin/sh

if [ "${1}" == "x64" ]; then
  arch=x86_64
  archdir=x64
else
  arch=x86
  archdir=Win32
fi

make_dirs() (
  if [ ! -d bin_${archdir}d/lib ]; then
    mkdir -p bin_${archdir}d/lib
  fi
)

copy_libs() (
  cp lib*/*-lav-*.dll ../bin_${archdir}d
  cp lib*/*-lav-*.pdb ../bin_${archdir}d
  cp lib*/*.lib ../bin_${archdir}d/lib
)

clean() (
  make distclean > /dev/null 2>&1
)

configure() (
  OPTIONS="
    --enable-shared \
    --enable-gpl \
    --enable-version3 \
    --enable-w32threads \
    --enable-runtime-cpudetect \
    --enable-demuxers \
    --disable-demuxer=matroska \
    --disable-filters \
    --enable-filter=yadif \
    --enable-filter=scale \
    --disable-protocols \
    --enable-protocol=file \
    --enable-protocol=mmsh \
    --enable-protocol=mmst \
    --enable-protocol=rtp \
    --enable-protocol=http \
    --disable-muxers \
    --enable-muxer=spdif \
    --enable-dxva2 \
    --disable-hwaccels \
    --enable-hwaccel=h264_dxva2 \
    --enable-hwaccel=vc1_dxva2 \
    --enable-hwaccel=wmv3_dxva2 \
    --enable-hwaccel=mpeg2_dxva2 \
    --enable-avresample \
    --disable-avdevice \
    --disable-postproc \
    --disable-swresample \
    --disable-static \
    --disable-encoders \
    --disable-bsfs \
    --disable-devices \
    --disable-ffplay \
    --disable-ffserver \
    --disable-ffmpeg \
    --disable-ffprobe \
    --build-suffix=-lav \
    --arch=${arch}"

  EXTRA_CFLAGS="-D_WIN32_WINNT=0x0502 -DWINVER=0x0502"

  sh configure --toolchain=msvc --enable-debug --extra-cflags="${EXTRA_CFLAGS}" ${OPTIONS}
)

build() (
  make -j8
)

echo Building ffmpeg in MSVC Debug config...

make_dirs

cd ffmpeg

clean

## run configure, redirect to file because of a msys bug
configure > config.out 2>&1
CONFIGRETVAL=$?

## show configure output
cat config.out

## Only if configure succeeded, actually build
if [ ${CONFIGRETVAL} -eq 0 ]; then
  build &&
  copy_libs
fi

cd ..
