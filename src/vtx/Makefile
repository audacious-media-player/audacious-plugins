PLUGIN = vtx${PLUGIN_SUFFIX}

SRCS = about.c		\
       ay8912.c		\
       info.c		\
       lh5dec.c		\
       vtx.c		\
       vtxfile.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CPPFLAGS += ${GTK_CFLAGS}  ${ARCH_DEFINES} -I../.. -I.
LIBS += ${GTK_LIBS}
