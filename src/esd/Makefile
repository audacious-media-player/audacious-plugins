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

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS}  ${ESD_CFLAGS} -I../.. -I..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  ${ESD_LIBS} ${OSS_LIBS}
