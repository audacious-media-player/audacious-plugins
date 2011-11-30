PLUGIN = CoreAudio${PLUGIN_SUFFIX}

SRCS = about.c		\
       configure.c	\
       coreaudio.c	\
       init.c		\
       audio.c		\
       convert.c	\
       dbconvert.c	\
       mixer.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. ${GTK_CFLAGS} ${GLIB_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS}
