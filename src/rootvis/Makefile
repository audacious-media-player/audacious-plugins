PLUGIN = rootvis${PLUGIN_SUFFIX}

SRCS = config_backend.c			\
       config_definition.c		\
       config_frontend.c		\
       config_frontend_widgets.c	\
       getroot.c			\
       rootvis.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${VISUALIZATION_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${PANGO_CFLAGS} ${IMLIB2_CFLAGS} -I.
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${PANGO_LIBS} ${IMLIB2_LIBS}
