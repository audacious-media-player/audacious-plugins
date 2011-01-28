PLUGIN = gtkui${PLUGIN_SUFFIX}

SRCS = columns.c \
       gtkui_cfg.c \
       layout.c \
       ui_infoarea.c \
       ui_gtk.c \
       ui_manager.c \
       ui_playlist_widget.c \
       ui_playlist_notebook.c \
       ui_statusbar.c \
       util.c \
       playlist_util.c \
       actions.c

include ../../buildsys.mk
include ../../extra.mk

# These override variables in buildsys.mk and so must come after it.
PACKAGE = audacious
DATA = ui/player.ui

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CFLAGS += -std=gnu99 ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${CAIRO_CFLAGS} ${PANGOCAIRO_CFLAGS} ${XRENDER_CFLAGS} ${XCOMPOSITE_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${CAIRO_LIBS} ${PANGOCAIRO_LIBS} ${XRENDER_LIBS} ${XCOMPOSITE_LIBS} ${MOWGLI_LIBS} -laudgui -lm
