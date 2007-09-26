PROG = audacious-arts-helper${PROG_SUFFIX}
SRCS = arts_helper.c

include ../../../buildsys.mk
include ../../../extra.mk

CPPFLAGS += ${ARTSC_CFLAGS} -I../../../intl -I../../..
LIBS += ${ARTSC_LIBS}
LDFLAGS += -Wl,-export-dynamic
