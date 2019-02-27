#!/bin/sh

arch=x86
archdir=Win32
clean_build=true
debug=true

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
    release)
            debug=false
            ;;
    *)
            echo "Unknown Option $opt"
            exit 1
    esac
done

make_dirs() (
  mkdir -p bin_${archdir}d/lib
  mkdir -p bin_${archdir}/lib
)

copy_libs() (
  if $debug ; then
      cp -u lib*/*-lav-*.dll ../bin_${archdir}d
      cp -u lib*/*-lav-*.pdb ../bin_${archdir}d
      cp -u lib*/*.lib ../bin_${archdir}d/lib
  else
      cp -u lib*/*-lav-*.dll ../bin_${archdir}
      cp -u lib*/*-lav-*.pdb ../bin_${archdir}
      cp -u lib*/*.lib ../bin_${archdir}/lib
  fi
)

clean() (
  make distclean > /dev/null 2>&1
)

configure() (
  OPTIONS="
    --enable-shared                 \
    --disable-static                \
    --enable-gpl                    \
    --enable-version3               \
    --enable-w32threads             \
    --disable-demuxer=matroska      \
    --disable-filters               \
    --enable-filter=scale,yadif,w3fdif \
    --disable-protocol=async,cache,concat,httpproxy,icecast,md5,subfile \
    --disable-muxers                \
    --enable-muxer=spdif            \
    --disable-bsfs                  \
    --enable-bsf=extract_extradata,vp9_superframe_split \
    --disable-cuda                  \
    --disable-cuvid                 \
    --disable-nvenc                 \
    --enable-avresample             \
    --enable-avisynth               \
    --disable-avdevice              \
    --disable-postproc              \
    --disable-swresample            \
    --disable-encoders              \
    --disable-devices               \
    --disable-programs              \
    --disable-doc                   \
    --build-suffix=-lav             \
    --arch=${arch}"

  EXTRA_CFLAGS="-D_WIN32_WINNT=0x0600 -DWINVER=0x0600 -Zo -GS-"
  EXTRA_LDFLAGS=""

  if $debug ; then
    OPTIONS="${OPTIONS} --enable-debug"
    EXTRA_CFLAGS="${EXTRA_CFLAGS} -MDd"
    EXTRA_LDFLAGS="${EXTRA_LDFLAGS} -NODEFAULTLIB:libcmt"
  else
    EXTRA_CFLAGS="${EXTRA_CFLAGS} -MD"
    EXTRA_LDFLAGS="${EXTRA_LDFLAGS} -NODEFAULTLIB:libcmt"
  fi

  sh configure --toolchain=msvc --x86asmexe=yasm --extra-cflags="${EXTRA_CFLAGS}" --extra-ldflags="${EXTRA_LDFLAGS}" ${OPTIONS}
)

build() (
  make -j$NUMBER_OF_PROCESSORS
)

echo Building ffmpeg in MSVC Debug config...

make_dirs

cd ffmpeg

if $clean_build ; then
    clean

    ## run configure, redirect to file because of a msys bug
    configure > ffbuild/config.out 2>&1
    CONFIGRETVAL=$?

    ## show configure output
    cat ffbuild/config.out
fi

## Only if configure succeeded, actually build
if ! $clean_build || [ ${CONFIGRETVAL} -eq 0 ]; then
  build &&
  copy_libs
fi

cd ..
