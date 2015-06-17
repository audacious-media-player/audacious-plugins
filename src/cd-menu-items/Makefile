PLUGIN = cd-menu-items${PLUGIN_SUFFIX}

SRCS = cd-menu-items.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}
CPPFLAGS += -I../..
CFLAGS += ${PLUGIN_CFLAGS}
