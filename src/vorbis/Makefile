PLUGIN = vorbis${PLUGIN_SUFFIX}

SRCS = configure.c	\
       vcupdate.c	\
       vcedit.c		\
       vorbis.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CPPFLAGS +=  ${OGG_VORBIS_CFLAGS} ${GTK_CFLAGS}  -I../..
LIBS += ${OGG_VORBIS_LIBS} ${GTK_LIBS}
