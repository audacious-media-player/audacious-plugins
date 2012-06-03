PLUGIN = oss4${PLUGIN_SUFFIX}

SRCS = plugin.c     \
       oss.c        \
       utils.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${OSS_CFLAGS} -I../..
