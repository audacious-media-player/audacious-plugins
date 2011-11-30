PLUGIN = smb${PLUGIN_SUFFIX}

SRCS = smb.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${TRANSPORT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} -lsmbclient
