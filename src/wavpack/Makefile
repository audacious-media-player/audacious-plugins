PLUGIN = wavpack${PLUGIN_SUFFIX}

SRCS = ui.c \
       wavpack.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CPPFLAGS += ${GTK_CFLAGS} ${WAVPACK_CFLAGS} -I../..
LIBS += ${WAVPACK_LIBS} ${GTK_LIBS}
