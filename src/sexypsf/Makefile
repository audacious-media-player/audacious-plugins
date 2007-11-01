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

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} -I../../intl -I../.. -Ispu/ -I.
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${MOWGLI_LIBS} -lz
