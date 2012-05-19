PLUGIN = vorbis${PLUGIN_SUFFIX}

SRCS = vcupdate.c \
       vcedit.c		\
       vorbis.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${VORBIS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS}  -I../..
LIBS += ${VORBIS_LIBS} ${GTK_LIBS} ${GLIB_LIBS}
