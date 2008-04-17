PLUGIN = madplug${PLUGIN_SUFFIX}

SRCS = configure.c	\
       input.c		\
       replaygain.c	\
       decoder.c	\
       tuple.c	        \
       plugin.c		\
       xing.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS}  ${MOWGLI_CFLAGS} ${ARCH_DEFINES} ${SIMD_CFLAGS} ${MAD_CFLAGS} -I../..
LDFLAGS += ${AUDLDFLAGS}
LIBS += ${MAD_LIBS} -laudid3tag ${GTK_LIBS} ${GLIB_LIBS}  ${MOWGLI_LIBS}
