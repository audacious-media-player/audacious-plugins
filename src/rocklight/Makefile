include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = librocklight$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(VISUALIZATION_PLUGIN_DIR)

LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

SOURCES = rocklight.c thinklight.c sysled.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(ARCH_DEFINES) -I../../intl -I../..

include ../../mk/objective.mk
