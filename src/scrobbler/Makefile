PLUGIN = scrobbler${PLUGIN_SUFFIX}
SRCS = fmt.c			\
       configure.c		\
       gtkstuff.c		\
       md5.c			\
       scrobbler.c		\
       gerpok.c			\
       plugin.c

DATA = images/audioscrobbler.png	\
       images/audioscrobbler_badge.png

plugindir = audacious/${GENERAL_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

PACKAGE = audacious

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${BEEP_DEFINES} ${CURL_CFLAGS} -I../../intl -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${CURL_LIBS} ${MUSICBRAINZ_LIBS}
