PLUGIN = projectm-1.0${PLUGIN_SUFFIX}
SRCS = main.cxx		\
       video_init.cxx   \
       ConfigFile.cxx   \
       main_visplugin.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := $(plugindir)/${VISUALIZATION_PLUGIN_DIR}

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${ARCH_DEFINES} ${XML_CPPFLAGS} ${SDL_CFLAGS} ${LIBPROJECTM1_CFLAGS} -I../../intl -I../..
CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${SDL_LIBS} ${LIBPROJECTM1_LIBS}
