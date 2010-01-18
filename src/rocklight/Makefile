PLUGIN = rocklight${PLUGIN_SUFFIX}

SRCS = rocklight.c	\
       sysled.c		\
       b43led.c		\
       thinklight.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${VISUALIZATION_PLUGIN_DIR}

CPPFLAGS += ${GTK_CFLAGS}  -I../..
LIBS += ${GTK_LIBS}
