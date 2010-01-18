PLUGIN = hotkey${PLUGIN_SUFFIX}

SRCS = plugin.c gui.c grab.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CPPFLAGS += ${GTK_CFLAGS}  -I../.. -I..
LIBS +=  ${GTK_LIBS} -lX11
