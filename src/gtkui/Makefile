PLUGIN = gtkui${PLUGIN_SUFFIX}

SRCS = columns.c \
       gtkui_cfg.c \
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

CFLAGS += -std=gnu99 ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../..
LIBS += -lm
