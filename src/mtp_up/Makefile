PLUGIN = mtp_up${PLUGIN_SUFFIX}
SRCS = mtp.c filetype.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := $(plugindir)/${GENERAL_PLUGIN_DIR}

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${BEEP_DEFINES} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} -I../../intl -I../.. ${MTP_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${MTP_LIBS}
