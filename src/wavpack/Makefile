PLUGIN = wavpack${PLUGIN_SUFFIX}

SRCS = tags.cxx		\
       ui.cxx		\
       libwavpack.cxx

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${DBUS_CFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${WAVPACK_CFLAGS} -I../../intl -I../..
CXXFLAGS += ${PLUGIN_CFLAGS}
LDFLAGS += -lstdc++
LIBS += ${WAVPACK_LIBS} ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
