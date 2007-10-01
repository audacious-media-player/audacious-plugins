PLUGIN = ESD${PLUGIN_SUFFIX}
SRCS = esd.c		\
       mixer.c		\
       about.c		\
       configure.c	\
       audio.c		\
       init.c

plugindir = audacious/${OUTPUT_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${ESD_CFLAGS} -I../../intl -I../.. -I..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${ESD_LIBS} ${OSS_LIBS}
