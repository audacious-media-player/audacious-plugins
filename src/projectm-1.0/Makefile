PLUGIN = projectm-1.0${PLUGIN_SUFFIX}

SRCS = gtk_projectm_impl.cxx \
       main.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${VISUALIZATION_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${ARCH_DEFINES} ${GTKGLEXT_CFLAGS} ${LIBPROJECTM1_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${GTKGLEXT_LIBS} ${LIBPROJECTM1_LIBS}
