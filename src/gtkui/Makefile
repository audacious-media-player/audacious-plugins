PLUGIN = gtkui${PLUGIN_SUFFIX}

SRCS = gtkui_cfg.c \
       ui_gtk.c \
       ui_manager.c \
       ui_playlist_widget.c \
       ui_playlist_manager.c \
       ui_playlist_model.c \
       util.c \
       actions.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CFLAGS += ${BEEP_DEFINES}
CPPFLAGS +=  ${GTK_CFLAGS} ${XRENDER_CFLAGS} ${XCOMPOSITE_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${XRENDER_LIBS} ${XCOMPOSITE_LIBS} -laudgui
