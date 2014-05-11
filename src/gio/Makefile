PLUGIN = gio${PLUGIN_SUFFIX}

SRCS = gio.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${TRANSPORT_PLUGIN_DIR}

LD = ${CXX}

CPPFLAGS += -I../.. ${GIO_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GIO_LIBS}
