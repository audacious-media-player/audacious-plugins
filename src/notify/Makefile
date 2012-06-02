PLUGIN = notify${PLUGIN_SUFFIX}

SRCS = event.c notify.c osd.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CPPFLAGS += -I../.. ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${NOTIFY_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${NOTIFY_LIBS}
