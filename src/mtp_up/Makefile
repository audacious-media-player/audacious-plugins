PLUGIN = mtp_up${PLUGIN_SUFFIX}

SRCS = mtp.c filetype.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CPPFLAGS += ${BEEP_DEFINES} ${GTK_CFLAGS}  -I../.. ${MTP_CFLAGS}
LIBS += ${GTK_LIBS}  ${MTP_LIBS}
