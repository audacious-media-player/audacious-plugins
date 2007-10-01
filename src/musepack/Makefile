PLUGIN = musepack${PLUGIN_SUFFIX}
SRCS = libmpc.cxx

plugindir = audacious/${INPUT_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${TAGLIB_CFLAGS} -I../../intl -I../..
CXXFLAGS += ${PLUGIN_CFLAGS}
LIBS += -lmpcdec ${TAGLIB_LIBS} ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
