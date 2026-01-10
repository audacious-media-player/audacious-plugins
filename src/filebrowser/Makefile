PLUGIN = filebrowser${PLUGIN_SUFFIX}

SRCS = filebrowser.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. ${GIO_CFLAGS} ${GTK_CFLAGS}
LIBS += ${GIO_LIBS} ${GTK_LIBS} -laudgui
