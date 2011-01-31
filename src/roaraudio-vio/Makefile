PLUGIN = roaraudio-vio${PLUGIN_SUFFIX}

SRCS = roaraudio-vio.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${TRANSPORT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${LIBROAR_LIBS}
