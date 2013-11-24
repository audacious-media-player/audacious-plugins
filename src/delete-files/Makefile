PLUGIN = delete-files${PLUGIN_SUFFIX}

SRCS = delete-files.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CPPFLAGS += -I../.. ${GIO_CFLAGS} ${GTK_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GIO_LIBS} ${GTK_LIBS}
