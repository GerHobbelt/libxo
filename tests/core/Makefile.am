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
test_01.c \
test_02.c \
test_03.c \
test_04.c \
test_05.c \
test_06.c \
test_07.c \
test_08.c \
test_09.c \
test_10.c \
test_11.c \
test_12.c \
test_13.c

test_01_test_SOURCES = test_01.c
test_02_test_SOURCES = test_02.c
test_03_test_SOURCES = test_03.c
test_04_test_SOURCES = test_04.c
test_05_test_SOURCES = test_05.c
test_06_test_SOURCES = test_06.c
test_07_test_SOURCES = test_07.c
test_08_test_SOURCES = test_08.c
test_09_test_SOURCES = test_09.c
test_10_test_SOURCES = test_10.c
test_11_test_SOURCES = test_11.c
test_12_test_SOURCES = test_12.c
test_13_test_SOURCES = test_13.c

# TEST_CASES := $(shell cd ${srcdir} ; echo *.c )

noinst_PROGRAMS = ${TEST_CASES:.c=.test}

LDADD = \
    ${top_builddir}/libxo/libxo.la

if HAVE_HUMANIZE_NUMBER
LDADD += -lutil
endif

EXTRA_DIST = \
    ${TEST_CASES} \
    ${addprefix saved/, ${TEST_CASES:.c=.E.err}} \
    ${addprefix saved/, ${TEST_CASES:.c=.E.out}} \
    ${addprefix saved/, ${TEST_CASES:.c=.H.err}} \
    ${addprefix saved/, ${TEST_CASES:.c=.H.out}} \
    ${addprefix saved/, ${TEST_CASES:.c=.HIPx.err}} \
    ${addprefix saved/, ${TEST_CASES:.c=.HIPx.out}} \
    ${addprefix saved/, ${TEST_CASES:.c=.HP.err}} \
    ${addprefix saved/, ${TEST_CASES:.c=.HP.out}} \
    ${addprefix saved/, ${TEST_CASES:.c=.J.err}} \
    ${addprefix saved/, ${TEST_CASES:.c=.J.out}} \
    ${addprefix saved/, ${TEST_CASES:.c=.JP.err}} \
    ${addprefix saved/, ${TEST_CASES:.c=.JP.out}} \
    ${addprefix saved/, ${TEST_CASES:.c=.JPu.err}} \
    ${addprefix saved/, ${TEST_CASES:.c=.JPu.out}} \
    ${addprefix saved/, ${TEST_CASES:.c=.T.err}} \
    ${addprefix saved/, ${TEST_CASES:.c=.T.out}} \
    ${addprefix saved/, ${TEST_CASES:.c=.X.err}} \
    ${addprefix saved/, ${TEST_CASES:.c=.X.out}} \
    ${addprefix saved/, ${TEST_CASES:.c=.XP.err}} \
    ${addprefix saved/, ${TEST_CASES:.c=.XP.out}} \
    ${addprefix saved/, test_01.Ecsv1.out} \
    ${addprefix saved/, test_01.Ecsv1.err} \
    ${addprefix saved/, test_01.Ecsv2.out} \
    ${addprefix saved/, test_01.Ecsv2.err} \
    ${addprefix saved/, test_01.Ecsv3.out} \
    ${addprefix saved/, test_01.Ecsv3.err}

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

TEST_FORMATS = T XP JP JPu HP X J H HIPx

test tests: ${bin_PROGRAMS}
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
	-@ (${TEST_TRACE} test=test_01.c; base=test_01; \
	    (   fmt=Ecsv1; extra=encoder=csv ; \
			${TEST_JIG2} ); \
	    (   fmt=Ecsv2; extra=encoder=csv:path=top-level/data/item:no-header ; \
			${TEST_JIG2} ); \
	    (   fmt=Ecsv3; extra=@csv:path=item:leafs=sku.sold:no-quotes ; \
			${TEST_JIG2} ); \
	)
	-@ (${TEST_TRACE} test=test_12.c; base=test_12; \
	    (   fmt=XPmap; extra=xml,pretty,map-file=${srcdir}/test_12.map ; \
			${TEST_JIG2} ); \
	    (   fmt=JPmap; extra=json,pretty,map-file=${srcdir}/test_12.map ; \
			${TEST_JIG2} ); \
	)

one:
	-@(test=${TEST_CASE}; data=${TEST_DATA}; ${TEST_ONE} ; true)

accept:
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
