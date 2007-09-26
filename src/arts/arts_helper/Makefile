PROG = audacious-arts-helper${PROG_SUFFIX}
SRCS = arts_helper.c

include ../../../buildsys.mk
include ../../../extra.mk

LDFLAGS += -Wl,-export-dynamic
LIBS += ${ARTSC_LIBS}

CPPFLAGS += ${ARTSC_CFLAGS} -I../../../intl -I../../..
