PLUGIN = xspf${PLUGIN_SUFFIX}

SRCS = xspf.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${CONTAINER_PLUGIN_DIR}

CPPFLAGS +=  ${GTK_CFLAGS}  ${ARCH_DEFINES} ${XML_CPPFLAGS} -I../..
LIBS += ${GTK_LIBS}  ${XML_LIBS}
