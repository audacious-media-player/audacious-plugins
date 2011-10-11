PLUGIN = neon${PLUGIN_SUFFIX}

SRCS = neon.c	\
       rb.c	\
       cert_verification.c

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${TRANSPORT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${MOWGLI_CFLAGS} ${GTK_CFLAGS} ${GLIB_CFLAGS} ${NEON_CFLAGS} -I../.. -D_RB_USE_GLIB
LIBS += ${GTK_LIBS} ${GLIB_LIBS} ${NEON_LIBS}
