PLUGIN = scrobbler${PLUGIN_SUFFIX}

SRCS = fmt.c			\
       configure.c		\
       gtkstuff.c		\
       scrobbler.c		\
       gerpok.c			\
       plugin.c

DATA = images/audioscrobbler.png	\
       images/audioscrobbler_badge.png

include ../../buildsys.mk
include ../../extra.mk

PACKAGE = audacious

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CPPFLAGS +=  ${GTK_CFLAGS}  ${BEEP_DEFINES} ${CURL_CFLAGS} -I../..
LIBS += ${GTK_LIBS}  ${CURL_LIBS}
