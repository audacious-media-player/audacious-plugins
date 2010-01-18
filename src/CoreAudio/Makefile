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

CPPFLAGS += -I../.. ${GTK_CFLAGS}
LIBS += ${GTK_LIBS}
