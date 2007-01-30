include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libxspf$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(CONTAINER_PLUGIN_DIR)

SOURCES = xspf.c base64.c urlencode.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(ARCH_DEFINES) $(XML_CPPFLAGS) -I../../intl -I../..

CXXFLAGS = $(CFLAGS)

LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(XML_LIBS)

include ../../mk/objective.mk
