PLUGIN = gtkui${PLUGIN_SUFFIX}

SRCS = gtkui_cfg.c \
       ui_infoarea.c \
       ui_gtk.c \
       ui_manager.c \
       ui_playlist_widget.c \
       ui_playlist_model.c \
       util.c \
       dnd.c \
       multidrag.c \
       playlist_util.c \
       actions.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CFLAGS += -std=gnu99 ${PLUGIN_CFLAGS} ${BEEP_DEFINES}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${CAIRO_CFLAGS} ${PANGOCAIRO_CFLAGS} ${XRENDER_CFLAGS} ${XCOMPOSITE_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${CAIRO_LIBS} ${PANGOCAIRO_LIBS} ${XRENDER_LIBS} ${XCOMPOSITE_LIBS} ${MOWGLI_LIBS} -laudgui
