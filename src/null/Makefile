include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libnull${SHARED_SUFFIX}

LIBDIR = ${plugindir}/${OUTPUT_PLUGIN_DIR}

SOURCES = null.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += ${PICFLAGS} \
	-I../..

include ../../mk/objective.mk
