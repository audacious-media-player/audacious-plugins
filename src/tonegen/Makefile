PLUGIN = tonegen${PLUGIN_SUFFIX}

SRCS = tonegen.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CPPFLAGS += ${GTK_CFLAGS}  -I../..
LIBS += ${GTK_LIBS}
