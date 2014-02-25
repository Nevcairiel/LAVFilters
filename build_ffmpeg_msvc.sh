#!/bin/sh

arch=x86
archdir=Win32
clean_build=true

for opt in "$@"
do
    case "$opt" in
    x86)
            ;;
    x64 | amd64)
            arch=x86_64
            archdir=x64
            ;;
    quick)
            clean_build=false
            ;;
    *)
            echo "Unknown Option $opt"
            exit 1
    esac
done

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
    --enable-shared                 \
    --disable-static                \
    --enable-version3               \
    --enable-w32threads             \
    --disable-demuxer=matroska      \
    --disable-filters               \
    --enable-filter=yadif           \
    --enable-filter=scale           \
    --disable-protocols             \
    --enable-protocol=file          \
    --enable-protocol=pipe          \
    --enable-protocol=mmsh          \
    --enable-protocol=mmst          \
    --enable-protocol=rtp           \
    --enable-protocol=http          \
    --disable-muxers                \
    --enable-muxer=spdif            \
    --disable-hwaccels              \
    --enable-hwaccel=h264_dxva2     \
    --enable-hwaccel=vc1_dxva2      \
    --enable-hwaccel=wmv3_dxva2     \
    --enable-hwaccel=mpeg2_dxva2    \
    --enable-avresample             \
    --enable-avisynth               \
    --disable-avdevice              \
    --disable-postproc              \
    --disable-swresample            \
    --disable-encoders              \
    --disable-bsfs                  \
    --disable-devices               \
    --disable-programs              \
    --enable-debug                  \
    --disable-doc                   \
    --build-suffix=-lav             \
    --arch=${arch}"

  EXTRA_CFLAGS="-D_WIN32_WINNT=0x0502 -DWINVER=0x0502 -d2Zi+ -MDd"
  EXTRA_LDFLAGS="-NODEFAULTLIB:libcmt"

  sh configure --toolchain=msvc --enable-debug --extra-cflags="${EXTRA_CFLAGS}" --extra-ldflags="${EXTRA_LDFLAGS}" ${OPTIONS}
)

build() (
  make -j8
)

echo Building ffmpeg in MSVC Debug config...

make_dirs

cd ffmpeg

if $clean_build ; then
    clean

    ## run configure, redirect to file because of a msys bug
    configure > config.out 2>&1
    CONFIGRETVAL=$?

    ## show configure output
    cat config.out
fi

## Only if configure succeeded, actually build
if ! $clean_build || [ ${CONFIGRETVAL} -eq 0 ]; then
  build &&
  copy_libs
fi

cd ..
