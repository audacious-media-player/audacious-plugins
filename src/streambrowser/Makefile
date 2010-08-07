PLUGIN = streambrowser${PLUGIN_SUFFIX}

SRCS = streambrowser.c	\
       streamdir.c \
       shoutcast.c \
       xiph.c \
       bookmarks.c \
       gui/streambrowser_win.c

DATA = images/shoutcast.png \
       images/xiph.png \
       images/bookmarks.png \
       images/streambrowser-16x16.png \
       images/streambrowser-64x64.png

include ../../buildsys.mk
include ../../extra.mk

PACKAGE = audacious

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS} ${BEEP_DEFINES}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${XML_CFLAGS} ${ARCH_DEFINES} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${MOWGLI_LIBS} ${XML_LIBS}

