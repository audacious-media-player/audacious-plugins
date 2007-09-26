PLUGIN = sexypsf${PLUGIN_SUFFIX}
SRCS = PsxBios.c 	\
       PsxCounters.c	\
       PsxDma.c		\
       Spu.c		\
       PsxMem.c		\
       PsxHw.c		\
       Misc.c		\
       R3000A.c		\
       PsxInterpreter.c	\
       PsxHLE.c		\
       spu/spu.c	\
       plugin.c

plugindir = audacious/${INPUT_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} -I../../intl -I../.. -Ispu/ -I.
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += -lz ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
