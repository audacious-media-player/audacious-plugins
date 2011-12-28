PLUGIN = gtkui${PLUGIN_SUFFIX}

SRCS = columns.c \
       layout.c \
       menus.c \
       ui_infoarea.c \
       ui_gtk.c \
       ui_playlist_widget.c \
       ui_playlist_notebook.c \
       ui_statusbar.c \
       playlist_util.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. ${GTK_CFLAGS}
LIBS += -lm ${GTK_LIBS}
