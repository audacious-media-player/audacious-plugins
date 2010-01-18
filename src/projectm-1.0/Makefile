PLUGIN = projectm-1.0${PLUGIN_SUFFIX}

SRCS = gtk_projectm_impl.cxx \
       main.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${VISUALIZATION_PLUGIN_DIR}

CPPFLAGS +=  ${GTK_CFLAGS}  ${ARCH_DEFINES} ${XML_CPPFLAGS} ${GTKGLEXT_CFLAGS} ${LIBPROJECTM1_CFLAGS} -I../..
LIBS += ${GTK_LIBS}  ${GTKGLEXT_LIBS} ${LIBPROJECTM1_LIBS}
