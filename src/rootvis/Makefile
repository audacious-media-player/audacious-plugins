PLUGIN = rootvis${PLUGIN_SUFFIX}
SRCS = config_backend.c			\
       config_definition.c		\
       config_frontend.c		\
       config_frontend_widgets.c	\
       getroot.c			\
       rootvis.c

plugindir = audacious/${VISUALIZATION_PLUGIN_DIR}

include ../../buildsys.mk
include ../../extra.mk

CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} -I. ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${IMLIB2_CFLAGS}
CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${IMLIB2_LIBS}
