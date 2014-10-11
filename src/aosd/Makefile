PLUGIN = aosd${PLUGIN_SUFFIX}

SRCS = aosd.cc		\
       aosd_osd.cc	\
       aosd_style.cc	\
       aosd_trigger.cc	\
       aosd_ui.cc	\
       aosd_cfg.cc	\
       ghosd.c		\
       ghosd-main.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${GENERAL_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS}  ${GTK_CFLAGS} ${GLIB_CFLAGS} ${XRENDER_CFLAGS} ${XCOMPOSITE_CFLAGS} -I../..
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${XRENDER_LIBS} ${XCOMPOSITE_LIBS} -lm
