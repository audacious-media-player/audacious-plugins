PLUGIN = skins${PLUGIN_SUFFIX}

SRCS = plugin.c \
       skins_cfg.c \
       pixbuf_effects.c \
       ui_skin.c \
       ui_skinned_window.c \
       ui_dock.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS} -DDATA_DIR="/usr/local/share/audacious"
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${CAIRO_CFLAGS} ${PANGOCAIRO_CFLAGS} ${XRENDER_CFLAGS} ${XCOMPOSITE_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${CAIRO_LIBS} ${PANGOCAIRO_LIBS} ${XRENDER_LIBS} ${XCOMPOSITE_LIBS}
