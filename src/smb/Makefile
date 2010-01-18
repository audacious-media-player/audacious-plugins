PLUGIN = smb${PLUGIN_SUFFIX}

SRCS = smb.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${TRANSPORT_PLUGIN_DIR}

CPPFLAGS +=  ${GTK_CFLAGS}  ${ARCH_DEFINES} ${XML_CPPFLAGS} -I../..
LIBS += ${GTK_LIBS}  ${XML_LIBS} -lsmbclient
