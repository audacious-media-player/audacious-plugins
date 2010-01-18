PLUGIN = evdev-plug${PLUGIN_SUFFIX}

SRCS = ed.c			\
       ed_bindings_store.c	\
       ed_internals.c		\
       ed_ui.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CPPFLAGS += ${BEEP_DEFINES} ${GTK_CFLAGS}  -I../..
LIBS += ${GTK_LIBS}
