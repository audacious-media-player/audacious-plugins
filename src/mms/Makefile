PLUGIN = mms${PLUGIN_SUFFIX}

SRCS = mms.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${TRANSPORT_PLUGIN_DIR}

CPPFLAGS +=  ${GTK_CFLAGS}  ${ARCH_DEFINES} ${XML_CPPFLAGS} ${MMS_CFLAGS} -I../.. -Wall
LIBS += ${GTK_LIBS}  ${XML_LIBS} ${MMS_LIBS}
