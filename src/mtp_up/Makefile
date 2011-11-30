PLUGIN = mtp_up${PLUGIN_SUFFIX}

SRCS = mtp.c filetype.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS}  -I../.. ${MTP_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  ${MTP_LIBS}
