PLUGIN = cdaudio-ng${PLUGIN_SUFFIX}

SRCS = cdaudio-ng.c	\
       configure.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CPPFLAGS +=  ${GTK_CFLAGS}  ${CDIO_CFLAGS} ${CDDB_CFLAGS} ${ARCH_DEFINES} -I../..
LIBS += ${GTK_LIBS}  ${CDIO_LIBS} ${CDDB_LIBS}
