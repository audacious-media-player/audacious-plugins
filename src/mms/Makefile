include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libmms$(SHARED_SUFFIX)

# XXX
LIBDIR = $(plugindir)/$(CONTAINER_PLUGIN_DIR)

SOURCES = mms.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(MMS_CFLAGS) $(ARCH_DEFINES) -I../../intl -I../..

CXXFLAGS = $(CFLAGS)

LIBADD = $(GTK_LIBS) $(MMS_LIBS)

include ../../mk/objective.mk
