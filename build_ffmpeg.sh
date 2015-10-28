#!/bin/sh

arch=x86
archdir=Win32
clean_build=true
cross_prefix=

for opt in "$@"
do
    case "$opt" in
    x86)
            ;;
    x64 | amd64)
            arch=x86_64
            archdir=x64
            cross_prefix=x86_64-w64-mingw32-
            ;;
    quick)
            clean_build=false
            ;;
    *)
            echo "Unknown Option $opt"
            exit 1
    esac
done

BASEDIR=$(pwd)
THIRDPARTYPREFIX=${BASEDIR}/bin_${archdir}/thirdparty
export PKG_CONFIG_PATH="${THIRDPARTYPREFIX}/lib/pkgconfig/"

make_dirs() (
  mkdir -p bin_${archdir}/lib
  mkdir -p bin_${archdir}d/lib
)

copy_libs() (
  # install -s --strip-program=${cross_prefix}strip lib*/*-lav-*.dll ../bin_${archdir}
  cp lib*/*-lav-*.dll ../bin_${archdir}
  ${cross_prefix}strip ../bin_${archdir}/*-lav-*.dll
  cp -u lib*/*.lib ../bin_${archdir}/lib

  cp lib*/*-lav-*.dll ../bin_${archdir}d
  ${cross_prefix}strip ../bin_${archdir}d/*-lav-*.dll
  cp -u lib*/*.lib ../bin_${archdir}d/lib
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
    --disable-protocol=async,cache,concat,httpproxy,icecast,md5,subfile \
    --disable-muxers                \
    --enable-muxer=spdif            \
    --disable-hwaccels              \
    --enable-hwaccel=h264_dxva2     \
    --enable-hwaccel=hevc_dxva2     \
    --enable-hwaccel=vc1_dxva2      \
    --enable-hwaccel=wmv3_dxva2     \
    --enable-hwaccel=mpeg2_dxva2    \
    --disable-decoder=dca           \
    --enable-libdcadec              \
    --enable-libspeex               \
    --enable-libopencore-amrnb      \
    --enable-libopencore-amrwb      \
    --enable-avresample             \
    --enable-avisynth               \
    --disable-avdevice              \
    --disable-postproc              \
    --disable-swresample            \
    --disable-encoders              \
    --disable-bsfs                  \
    --disable-devices               \
    --disable-programs              \
    --disable-debug                 \
    --disable-doc                   \
    --build-suffix=-lav             \
    --arch=${arch}"

  EXTRA_CFLAGS="-D_WIN32_WINNT=0x0502 -DWINVER=0x0502 -I../thirdparty/include"
  EXTRA_LDFLAGS=""
  if [ "${arch}" == "x86_64" ]; then
    OPTIONS="${OPTIONS} --enable-cross-compile --cross-prefix=${cross_prefix} --target-os=mingw32 --pkg-config=pkg-config"
    EXTRA_LDFLAGS="${EXTRA_LDFLAGS} -L../thirdparty/lib64"
  else
    OPTIONS="${OPTIONS} --cpu=i686"
    EXTRA_CFLAGS="${EXTRA_CFLAGS} -mmmx -msse -mfpmath=sse"
    EXTRA_LDFLAGS="${EXTRA_LDFLAGS} -L../thirdparty/lib32"
  fi

  sh configure --extra-ldflags="${EXTRA_LDFLAGS}" --extra-cflags="${EXTRA_CFLAGS}" ${OPTIONS}
)

build() (
  make -j$NUMBER_OF_PROCESSORS
)

build_dcadec() (
  mkdir -p "${THIRDPARTYPREFIX}/dcadec"
  cd "${THIRDPARTYPREFIX}/dcadec"
  if $clean_build ; then
    make -f "${BASEDIR}/thirdparty/dcadec/Makefile" CONFIG_WINDOWS=1 clean
  fi
  make -f "${BASEDIR}/thirdparty/dcadec/Makefile" -j$NUMBER_OF_PROCESSORS CONFIG_WINDOWS=1 CONFIG_SMALL=1 CC=${cross_prefix}gcc AR=${cross_prefix}ar PREFIX="${THIRDPARTYPREFIX}" install-lib

  cd "${BASEDIR}"
)

make_dirs

echo Building dcadec
echo

build_dcadec

echo
echo Building ffmpeg in GCC ${arch} Release config...
echo

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
