PLUGIN = neon${PLUGIN_SUFFIX}

SRCS = neon.cc	\
       cert_verification.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${TRANSPORT_PLUGIN_DIR}

LD = ${CXX}

CFLAGS += ${PLUGIN_CFLAGS}
CPPFLAGS += ${PLUGIN_CPPFLAGS} ${GLIB_CFLAGS} ${NEON_CFLAGS} -I../..
LIBS += ${GLIB_LIBS} ${NEON_LIBS}
