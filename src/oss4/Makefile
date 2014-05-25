PLUGIN = oss4${PLUGIN_SUFFIX}

SRCS = plugin.cc     \
       oss.cc        \
       utils.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${OUTPUT_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${OSS_CFLAGS} -I../..
