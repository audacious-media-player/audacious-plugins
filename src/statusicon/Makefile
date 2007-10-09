PLUGIN = statusicon${PLUGIN_SUFFIX}
SRCS = gtktrayicon-x11.c	\
       si.c			\
       si_cfg.c			\
       si_ui.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := $(plugindir)/${GENERAL_PLUGIN_DIR}

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${BEEP_DEFINES} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} -I../../intl -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
