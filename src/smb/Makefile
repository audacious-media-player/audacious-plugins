PLUGIN = smb${PLUGIN_SUFFIX}

SRCS = smb.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${TRANSPORT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${ARCH_DEFINES} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} -lsmbclient
