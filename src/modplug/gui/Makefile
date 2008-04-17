STATIC_LIB_NOINST = libgui.a

SRCS = main.cxx		\
       support.cxx	\
       interface.cxx	\
       callbacks.cxx

include ../../../buildsys.mk
include ../../../extra.mk

CFLAGS   += ${PLUGIN_CFLAGS}
CXXFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${MOWGLI_CFLAGS} ${DBUS_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS}  -I../../..
