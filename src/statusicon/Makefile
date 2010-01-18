PLUGIN = statusicon${PLUGIN_SUFFIX}

SRCS = si.c			\
       si_cfg.c			\
       si_ui.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${BEEP_DEFINES} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS}  -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} 
