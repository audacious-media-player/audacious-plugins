PLUGIN = CoreAudio${PLUGIN_SUFFIX}
SRCS = about.c		\
       configure.c	\
       coreaudio.c	\
       init.c		\
       audio.c		\
       convert.c	\
       dbconvert.c	\
       mixer.c

plugindir = audacious/${OUTPUT_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} -I../../intl -I../.. ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS}
