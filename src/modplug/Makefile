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
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${MODPLUG_CFLAGS} -I../..
LIBS += ${GLIB_LIBS} ${MODPLUG_LIBS}
