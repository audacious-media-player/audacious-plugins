PLUGIN = projectm${PLUGIN_SUFFIX}
SRCS = main.c		\
       video_init.c

plugindir = audacious/${VISUALIZATION_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${ARCH_DEFINES} ${XML_CPPFLAGS} ${SDL_CFLAGS} ${LIBPROJECTM_CFLAGS} -I../../intl -I../..
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${SDL_LIBS} ${LIBPROJECTM_LIBS} -lprojectM
