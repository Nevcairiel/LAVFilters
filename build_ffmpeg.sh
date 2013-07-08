#!/bin/sh

if [ "${1}" == "x64" ]; then
  arch=x86_64
  archdir=x64
else
  arch=x86
  archdir=Win32
fi

make_dirs() (
  if [ ! -d bin_${archdir}/lib ]; then
    mkdir -p bin_${archdir}/lib
  fi

  if [ ! -d bin_${archdir}d/lib ]; then
    mkdir -p bin_${archdir}d/lib
  fi
)

strip_libs() {
  if [ "${arch}" == "x86_64" ]; then
    x86_64-w64-mingw32-strip lib*/*-lav-*.dll
  else
    strip lib*/*-lav-*.dll
  fi
}

copy_libs() (
  cp lib*/*-lav-*.dll ../bin_${archdir}
  cp lib*/*.lib ../bin_${archdir}/lib
  cp lib*/*-lav-*.dll ../bin_${archdir}d
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
    --enable-pthreads \
    --enable-runtime-cpudetect \
    --enable-demuxers \
    --disable-demuxer=matroska \
    --disable-filters \
    --enable-filter=yadif \
    --enable-filter=scale \
    --disable-protocols \
    --enable-protocol=file \
    --enable-protocol=pipe \
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
    --enable-libspeex \
    --enable-libopencore-amrnb \
    --enable-libopencore-amrwb \
    --enable-libopus \
    --enable-avresample \
    --disable-avdevice \
    --disable-postproc \
    --disable-swresample \
    --disable-static \
    --disable-debug \
    --disable-encoders \
    --disable-bsfs \
    --disable-devices \
    --disable-ffplay \
    --disable-ffserver \
    --disable-ffmpeg \
    --disable-ffprobe \
    --build-suffix=-lav \
    --arch=${arch}"

  EXTRA_CFLAGS="-D_WIN32_WINNT=0x0502 -DWINVER=0x0502 -I../thirdparty/include -idirafter../common/includes/dxva2 -DPTW32_STATIC_LIB"
  EXTRA_LDFLAGS=""
  if [ "${arch}" == "x86_64" ]; then
    OPTIONS="${OPTIONS} --enable-cross-compile --cross-prefix=x86_64-w64-mingw32- --target-os=mingw32"
    EXTRA_LDFLAGS="${EXTRA_LDFLAGS} -L../thirdparty/lib64"
  else
    OPTIONS="${OPTIONS} --cpu=i686"
    EXTRA_CFLAGS="${EXTRA_CFLAGS} -mmmx -msse -mfpmath=sse"
    EXTRA_LDFLAGS="${EXTRA_LDFLAGS} -L../thirdparty/lib32"
  fi

  sh configure --extra-ldflags="${EXTRA_LDFLAGS}" --extra-cflags="${EXTRA_CFLAGS}" ${OPTIONS}
)

build() (
  make -j8
)

echo Building ffmpeg in GCC ${arch} Release config...

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
  strip_libs &&
  copy_libs
fi

cd ..
