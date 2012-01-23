PLUGIN = lirc${PLUGIN_SUFFIX}

SRCS = lirc.c \
       configure.c
      

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${MOWGLI_CFLAGS}   -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS}  -llirc_client
