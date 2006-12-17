include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libalac$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)
LIBADD = $(AUDACIOUS_LIBS)

SOURCES = alac.c demux.c plugin.c stream.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(AUDACIOUS_CFLAGS) -I../../intl -I../.. -I../..

include ../../mk/objective.mk
