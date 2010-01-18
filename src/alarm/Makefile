PLUGIN = alarm${PLUGIN_SUFFIX}

SRCS = alarm.c		\
       interface.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CPPFLAGS += ${BEEP_DEFINES} ${GTK_CFLAGS}  -I../..
LIBS += ${GTK_LIBS} -lpthread
