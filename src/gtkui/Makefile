PLUGIN = gtkui${PLUGIN_SUFFIX}

SRCS = columns.cc \
       layout.cc \
       menu-ops.cc \
       menus.cc \
       ui_infoarea.cc \
       ui_gtk.cc \
       ui_playlist_widget.cc \
       ui_playlist_notebook.cc \
       ui_statusbar.cc \
       settings.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. ${GTK_CFLAGS}
LIBS += -lm ${GTK_LIBS} -laudgui
