include ../../extra.mk

PLUGIN = wav${PLUGIN_SUFFIX}
SRCS = wav${WAV_SNDFILE}.c

plugindir = audacious/${INPUT_PLUGIN_DIR}

include ../../buildsys.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} -I../../intl -I../.. 
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${MOWGLI_LIBS} ${SNDFILE_LIBS} ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
