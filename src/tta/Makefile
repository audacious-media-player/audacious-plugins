PLUGIN = tta${PLUGIN_SUFFIX}

SRCS = libtta.c	\
       ttadec.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS}  -I../..
LDFLAGS += ${AUDLDFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  ${MOWGLI_LIBS} -laudid3tag
