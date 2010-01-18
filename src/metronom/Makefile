PLUGIN = metronom${PLUGIN_SUFFIX}

SRCS = metronom.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CPPFLAGS += ${GTK_CFLAGS}  ${ARCH_DEFINES} -I../..
LIBS += ${GTK_LIBS}
