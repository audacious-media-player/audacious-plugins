PLUGIN = delete-files${PLUGIN_SUFFIX}

SRCS = delete-files.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CPPFLAGS += -I../.. ${GIO_CFLAGS} ${GTK_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GIO_LIBS} ${GTK_LIBS} -laudgui
