PLUGIN = skins${PLUGIN_SUFFIX}

SRCS = plugin.c \
       skins_cfg.c \
       pixbuf_effects.c \
       dnd.c \
       ui_fileopener.c \
       ui_skin.c \
       ui_skinned_window.c \
       ui_dock.c \
       util.c \
       ui_vis.c \
       ui_svis.c \
       ui_skinned_menurow.c \
       ui_skinned_button.c \
       ui_skinned_textbox.c \
       ui_skinned_playstatus.c \
       ui_skinned_monostereo.c \
       ui_skinned_number.c \
       ui_skinned_horizontal_slider.c \
       ui_skinned_equalizer_graph.c \
       ui_skinned_equalizer_slider.c \
       ui_skinned_playlist.c \
       ui_skinned_playlist_slider.c \
       ui_main.c \
       ui_equalizer.c \
       ui_playlist.c \
       ui_main_evlisteners.c \
       ui_playlist_evlisteners.c \
       ui_manager.c \
       ui_hints.c \
       icons-stock.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS} ${BEEP_DEFINES}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${CAIRO_CFLAGS} ${PANGOCAIRO_CFLAGS} ${XRENDER_CFLAGS} ${XCOMPOSITE_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${CAIRO_LIBS} ${PANGOCAIRO_LIBS} ${XRENDER_LIBS} ${XCOMPOSITE_LIBS} ${MOWGLI_LIBS}
