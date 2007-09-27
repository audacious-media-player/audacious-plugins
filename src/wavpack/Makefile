PLUGIN = wavpack${PLUGIN_SUFFIX}
SRCS = tags.cxx		\
       ui.cxx		\
       libwavpack.cxx

plugindir = audacious/${INPUT_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${DBUS_CFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${WAVPACK_CFLAGS} -I../../intl -I../..
CXXFLAGS += ${PLUGIN_CFLAGS}
LDFLAGS += -lstdc++
LIBS += ${WAVPACK_LIBS} ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
