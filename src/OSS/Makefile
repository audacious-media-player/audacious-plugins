PLUGIN = OSS${PLUGIN_SUFFIX}

SRCS = OSS.c		\
       mixer.c		\
       configure.c	\
       audio.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${OSS_CFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS}  -I../.. -I.
LIBS += ${OSS_LIBS} ${GTK_LIBS} ${GLIB_LIBS}
