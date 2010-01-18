PLUGIN = m3u${PLUGIN_SUFFIX}

SRCS = m3u.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${CONTAINER_PLUGIN_DIR}

CPPFLAGS +=  ${GTK_CFLAGS}  ${ARCH_DEFINES} ${XML_CPPFLAGS} -I../.. -Wall
LIBS += ${GTK_LIBS}  ${XML_LIBS}
