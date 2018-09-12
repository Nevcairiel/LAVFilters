#!/bin/sh
pkg-config --define-variable=prefix=../thirdparty/ $*
exit $?
