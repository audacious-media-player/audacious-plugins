include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = liba52dec$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

SOURCES = a52dec.c

LIBADD = -L/usr/local/lib -la52

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) -I../../intl -I../..

OBJECTS = ${SOURCES:.c=.o}

include ../../mk/objective.mk
