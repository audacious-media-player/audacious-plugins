PLUGIN = modplug${PLUGIN_SUFFIX}

SRCS = archive/arch_raw.cxx \
       archive/archive.cxx \
       archive/open.cxx \
       plugin.cxx \
       modplugbmp.cxx \
       plugin_main.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${MODPLUG_CFLAGS} -I../..
LIBS += ${MOWGLI_LIBS} ${MODPLUG_LIBS} -lstdc++
