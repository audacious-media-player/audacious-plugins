PLUGIN = aac${PLUGIN_SUFFIX}

SRCS = itunes-cover.c \
       libmp4.c \
       mp4_utils.c		\
       tagging_mp4.c		\
       mp4ff/mp4ff.c		\
       mp4ff/mp4atom.c		\
       mp4ff/mp4meta.c		\
       mp4ff/mp4sample.c	\
       mp4ff/mp4util.c		\
       mp4ff/mp4tagupdate.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${FAAD_CFLAGS} -I../.. -I. -Imp4ff -DUSE_TAGGING=1
LIBS += ${GLIB_LIBS} ${FAAD_LIBS} -lm
