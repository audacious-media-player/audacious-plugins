include ../../extra.mk

PLUGIN = wav${PLUGIN_SUFFIX}

SRCS = wav${WAV_SNDFILE}.c

include ../../buildsys.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${DBUS_CFLAGS} -I../../intl -I../.. 
LIBS += ${MOWGLI_LIBS} ${SNDFILE_LIBS} ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
