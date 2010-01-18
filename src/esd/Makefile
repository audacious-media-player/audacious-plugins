PLUGIN = ESD${PLUGIN_SUFFIX}

SRCS = esd.c		\
       mixer.c		\
       about.c		\
       configure.c	\
       audio.c		\
       init.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CPPFLAGS +=  ${GTK_CFLAGS}  ${ESD_CFLAGS} -I../.. -I..
LIBS += ${GTK_LIBS}  ${ESD_LIBS} ${OSS_LIBS}
