#
# $Id$
#
# Copyright 2015, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.

if LIBXO_WARNINGS_HIGH
LIBXO_WARNINGS = HIGH
endif
if HAVE_GCC
GCC_WARNINGS = yes
endif
include ${top_srcdir}/warnings.mk

enc_fullpathincdir = ${includedir}/libxo

AM_CFLAGS = \
    -I${top_srcdir}/libxo \
    -I${top_builddir}/libxo \
    ${WARNINGS}

LIBNAME = libenc_fullpath
pkglib_LTLIBRARIES = libenc_fullpath.la
LIBS = \
    -L${top_builddir}/libxo -lxo

LDADD = ${top_builddir}/libxo/libxo.la

libenc_fullpath_la_SOURCES = \
    enc_fullpath.c

pkglibdir = ${XO_ENCODERDIR}

UGLY_NAME = fullpath.enc

install-exec-hook:
	@DLNAME=`sh -c '. ./libenc_fullpath.la ; echo $$dlname'` ; \
		if [ x"$$DLNAME" = x ]; \
                    then DLNAME=${LIBNAME}.${XO_LIBEXT}; fi ; \
		if [ "$(build_os)" = "cygwin" ]; \
		    then DLNAME="../bin/$$DLNAME"; fi ; \
		echo Install link $$DLNAME "->" ${UGLY_NAME} "..." ; \
		mkdir -p ${DESTDIR}${XO_ENCODERDIR} ; \
		cd ${DESTDIR}${XO_ENCODERDIR} \
		&& chmod +w . \
		&& rm -f ${UGLY_NAME} \
		&& ${LN_S} $$DLNAME ${UGLY_NAME}
