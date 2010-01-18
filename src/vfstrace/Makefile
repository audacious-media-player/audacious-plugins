PLUGIN = vfstrace${PLUGIN_SUFFIX}

SRCS = vfstrace.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CPPFLAGS += ${GTK_CFLAGS} ${ARCH_DEFINES} ${XML_CPPFLAGS} -I../..
LIBS += ${GTK_LIBS}  ${XML_LIBS}
