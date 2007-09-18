include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libgnomeshortcuts$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(GENERAL_PLUGIN_DIR)

LIBADD = $(GLIB_LIBS) $(DBUS_LIBS)
SOURCES = gnomeshortcuts.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GLIB_CFLAGS) $(DBUS_CFLAGS) \
	-I../../intl -I../.. -I..

include ../../mk/objective.mk
