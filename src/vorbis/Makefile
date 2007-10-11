PLUGIN = vorbis${PLUGIN_SUFFIX}

SRCS = configure.c	\
       fileinfo.c	\
       vcedit.c		\
       vorbis.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${OGG_VORBIS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} -I../../intl -I../..
LIBS += ${OGG_VORBIS_LIBS} ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
