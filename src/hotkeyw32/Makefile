PLUGIN = hotkeyw32${PLUGIN_SUFFIX}

SRCS = ../hotkey/gui.cc ../hotkey/plugin.cc w32_hotkey.cc ../hotkey/hotkey_api_common.cc windows_window.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} -I../.. -I..
LIBS += ${GLIB_LIBS} ${GTK_LIBS} -laudgui