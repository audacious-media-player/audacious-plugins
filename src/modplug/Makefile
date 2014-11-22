PLUGIN = modplug${PLUGIN_SUFFIX}

SRCS = archive/arch_raw.cc \
       archive/archive.cc \
       archive/open.cc \
       modplugbmp.cc \
       plugin_main.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MODPLUG_CFLAGS} -I../..
LIBS += ${MODPLUG_LIBS}
