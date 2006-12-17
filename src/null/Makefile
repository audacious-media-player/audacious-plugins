include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libnull${SHARED_SUFFIX}

LIBDIR = ${plugindir}/${OUTPUT_PLUGIN_DIR}
LIBADD = $(AUDACIOUS_LIBS)

SOURCES = null.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += ${PICFLAGS} $(AUDACIOUS_CFLAGS) \
	-I../..

include ../../mk/objective.mk
