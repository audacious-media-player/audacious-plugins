PLUGIN = aosd${PLUGIN_SUFFIX}

SRCS = aosd.c		\
       aosd_osd.c	\
       aosd_style.c	\
       aosd_trigger.c	\
       aosd_ui.c	\
       aosd_cfg.c	\
       ghosd.c		\
       ghosd-main.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${CAIRO_CFLAGS} ${PANGOCAIRO_CFLAGS} ${XRENDER_CFLAGS} ${XCOMPOSITE_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${CAIRO_LIBS} ${PANGOCAIRO_LIBS} ${XRENDER_LIBS} ${XCOMPOSITE_LIBS}
