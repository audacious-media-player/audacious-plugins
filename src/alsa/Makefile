PLUGIN = ALSA${PLUGIN_SUFFIX}

SRCS = alsa.c		\
       about.c		\
       audio.c		\
       configure.c	\
       init.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := $(plugindir)/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${ALSA_CFLAGS} -I../../intl -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${ALSA_LIBS}
