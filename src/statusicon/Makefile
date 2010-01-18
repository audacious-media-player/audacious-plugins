PLUGIN = statusicon${PLUGIN_SUFFIX}

SRCS = si.c			\
       si_cfg.c			\
       si_ui.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CPPFLAGS += ${BEEP_DEFINES} ${GTK_CFLAGS}  -I../..
LIBS += ${GTK_LIBS}
