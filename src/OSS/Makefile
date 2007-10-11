PLUGIN = OSS${PLUGIN_SUFFIX}

SRCS = OSS.c		\
       mixer.c		\
       about.c		\
       configure.c	\
       audio.c		\
       init.c		\
       convert.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${OSS_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} -I../../intl -I../.. -I.
LIBS += ${OSS_LIBS} ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
