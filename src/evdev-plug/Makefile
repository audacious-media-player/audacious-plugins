PLUGIN = evdev-plug${PLUGIN_SUFFIX}

SRCS = ed.c			\
       ed_bindings_store.c	\
       ed_internals.c		\
       ed_ui.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS}  -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}
