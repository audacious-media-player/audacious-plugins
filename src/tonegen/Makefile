include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libtonegen$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)
LIBADD = $(AUDACIOUS_LIBS)

SOURCES = tonegen.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(AUDACIOUS_CFLAGS) -I../../intl -I../..

include ../../mk/objective.mk
