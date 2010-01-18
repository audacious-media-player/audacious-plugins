PLUGIN = lirc${PLUGIN_SUFFIX}

SRCS = about.c \
       lirc.c \
       lirc_cfg.c \
       callbacks.c \
       interface.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CPPFLAGS += ${GTK_CFLAGS}   -I../..
LIBS += ${GTK_LIBS}  -llirc_client
