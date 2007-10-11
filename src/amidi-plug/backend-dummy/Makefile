PLUGIN = ap-dummy${PLUGIN_SUFFIX}

SRCS = b-dummy.c

include ../../../buildsys.mk
include ../../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}/${AMIDIPLUG_BACKEND_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} -I../../..
LIBS += ../pcfg/libpcfg.a
