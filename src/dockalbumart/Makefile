include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libdockalbumart$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(GENERAL_PLUGIN_DIR)

LIBADD = $(GTK_LIBS) $(GDKX11_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
SOURCES = dockalbumart.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) \
	-I../../intl -I../.. -I..

include ../../mk/objective.mk
