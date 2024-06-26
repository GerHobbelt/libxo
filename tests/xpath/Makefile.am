#
# $Id$
#
# Copyright 2014, Juniper Networks, Inc.
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

AM_CFLAGS = \
    -I${top_srcdir} \
    -I${top_srcdir}/libxo \
    ${WARNINGS}

# Ick: maintained by hand!
TEST_CASES = \
xpath_01.c

xpath_01_test_SOURCES = xpath_01.c

# TEST_CASES := $(shell cd ${srcdir} ; echo *.c )

noinst_PROGRAMS = ${TEST_CASES:.c=.test}

LDADD = \
    ${top_builddir}/libxo/libxo.la

if HAVE_HUMANIZE_NUMBER
LDADD += -lutil
endif

#
# Sadly, pathetically, this list must be maintained by hand.  We can't
# just say 'TEST_FILES=`sed "s:^:saved:" ${srcdir}/saved/files.txt`.
#
TEST_FILES= \
    xpath_01_01.1.fullpath.out \
    xpath_01_01.1.fullpath.err \
    xpath_01_01.2.fullpath.out \
    xpath_01_01.2.fullpath.err \
    xpath_01_02.1.fullpath.out \
    xpath_01_02.1.fullpath.err \
    xpath_01_02.2.fullpath.out \
    xpath_01_02.2.fullpath.err \
    xpath_01_03.1.fullpath.out \
    xpath_01_03.1.fullpath.err \
    xpath_01_03.2.fullpath.out \
    xpath_01_03.2.fullpath.err \
    xpath_01_04.1.fullpath.out \
    xpath_01_04.1.fullpath.err \
    xpath_01_04.2.fullpath.out \
    xpath_01_04.2.fullpath.err \
    xpath_01_05.1.fullpath.out \
    xpath_01_05.1.fullpath.err \
    xpath_01_05.2.fullpath.out \
    xpath_01_05.2.fullpath.err \
    xpath_01_06.1.fullpath.out \
    xpath_01_06.1.fullpath.err \
    xpath_01_06.2.fullpath.out \
    xpath_01_06.2.fullpath.err \
    xpath_01_07.1.fullpath.out \
    xpath_01_07.1.fullpath.err \
    xpath_01_07.2.fullpath.out \
    xpath_01_07.2.fullpath.err \
    xpath_01_08.1.fullpath.out \
    xpath_01_08.1.fullpath.err \
    xpath_01_08.2.fullpath.out \
    xpath_01_08.2.fullpath.err

EXTRA_DIST = \
    ${TEST_CASES} \
    ${addprefix saved/, ${TEST_FILES}}

S2O = | ${SED} '1,/@@/d'

all:

valgrind:
	@echo '## Running the regression tests under Valgrind'
	${MAKE} CHECKER='valgrind -q' tests

#TEST_TRACE = set -x ; 

TEST_JIG = \
      ${CHECKER} ./$$base.test --libxo$$xoopts ${TEST_OPTS} \
      > out/$$base.$$fmt.out 2> out/$$base.$$fmt.err ; \
 ${DIFF} -Nu ${srcdir}/saved/$$base.$$fmt.out out/$$base.$$fmt.out ${S2O} ; \
 ${DIFF} -Nu ${srcdir}/saved/$$base.$$fmt.err out/$$base.$$fmt.err ${S2O}

TEST_JIG2 = \
echo "... $$test ... $$fmt ..."; \
xoopts==warn,$$extra ; \
${TEST_JIG}; true;

#TEST_FORMATS = T XP JP JPu HP X J H HIPx
TEST_FORMATS = XP

RUN_TESTS = ${srcdir}/../run-tests.sh -d ${srcdir}

RUN_TESTS_CMD = ${BASH} ${RUN_TESTS} -T fullpath

test tests: ${bin_PROGRAMS}
	@${MKDIR} -p out
	${RUN_TESTS_CMD} -l warn run ${TEST_CASES:.c=.test}

one:

accept:
	@${MKDIR} -p ${srcdir}/saved
	@${RUN_TESTS_CMD} accept ${TEST_CASES}

old-test old-tests: ${bin_PROGRAMS}
	@${MKDIR} -p out
	-@ ${TEST_TRACE} (for test in ${TEST_CASES} ; do \
	    base=`${BASENAME} $$test .c` ; \
            (for fmt in ${TEST_FORMATS}; do \
	        echo "... $$test ... $$fmt ..."; \
                xoopts=:W$$fmt ; \
	        ${TEST_JIG}; \
                true; \
            done) ; \
            (for fmt in E; do \
	        echo "... $$test ... $$fmt ..."; \
                xoopts==warn,encoder=test ; \
	        ${TEST_JIG}; \
                true; \
            done) \
	done)

one:
	-@(test=${TEST_CASE}; data=${TEST_DATA}; ${TEST_ONE} ; true)

old-accept:
	-@(for test in ${TEST_CASES} ; do \
	    base=`${BASENAME} $$test .c` ; \
            (for fmt in ${TEST_FORMATS} E ; do \
	        echo "... $$test ... $$fmt ..."; \
	        ${CP} out/$$base.$$fmt.out ${srcdir}/saved/$$base.$$fmt.out ; \
	        ${CP} out/$$base.$$fmt.err ${srcdir}/saved/$$base.$$fmt.err ; \
	    done) \
	done)
	-@(test=test_01.c; base=test_01; for fmt in Ecsv1 Ecsv2 Ecsv3 ; do \
	        echo "... $$test ... $$fmt ..."; \
	        ${CP} out/$$base.$$fmt.out ${srcdir}/saved/$$base.$$fmt.out ; \
	        ${CP} out/$$base.$$fmt.err ${srcdir}/saved/$$base.$$fmt.err ; \
	done)
	-@(test=test_01.c; base=test_12; for fmt in XPmap JPmap ; do \
	        echo "... $$test ... $$fmt ..."; \
	        ${CP} out/$$base.$$fmt.out ${srcdir}/saved/$$base.$$fmt.out ; \
	        ${CP} out/$$base.$$fmt.err ${srcdir}/saved/$$base.$$fmt.err ; \
	done)

.c.test:
	$(AM_V_CC)$(LTCOMPILE) -MT $@ -MD -MP -MF $(DEPDIR)/$*.Tpo -o $@ $<

CLEANFILES = ${TEST_CASES:.c=.test}
CLEANDIRS = out

clean-local:
	rm -rf ${CLEANDIRS}

echo:
	echo ${${VAR}}
