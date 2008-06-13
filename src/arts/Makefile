PLUGIN = arts${PLUGIN_SUFFIX}

SRCS = arts.c		\
       configure.c	\
       audio.c		\
       convert.c

SUBDIRS = arts_helper

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS}  ${ARTSC_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  ${ARTSC_LIBS} -lpthread
