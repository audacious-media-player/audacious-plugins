include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libhotkey$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(GENERAL_PLUGIN_DIR)

LIBADD = $(GTK_LIBS) $(GLIB_LIBS)
SOURCES = plugin.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) \
	-I../../intl -I../.. -I..

include ../../mk/objective.mk
