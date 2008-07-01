PLUGIN = streambrowser${PLUGIN_SUFFIX}

SRCS = streambrowser.c	\
       streamdir.c \
       shoutcast.c \
       gui/about_win.c \
       gui/streambrowser_win.c

DATA = images/shoutcast.png

include ../../buildsys.mk
include ../../extra.mk

PACKAGE = audacious

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS} ${BEEP_DEFINES}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${XML_CPPFLAGS} ${ARCH_DEFINES} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${MOWGLI_LIBS} ${XML_LIBS}

