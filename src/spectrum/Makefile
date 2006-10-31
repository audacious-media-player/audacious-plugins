include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libspectrum$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(VISUALIZATION_PLUGIN_DIR)

LIBADD = $(GTK_LIBS)

SOURCES = spectrum.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(ARCH_DEFINES) -I../../intl -I../..

include ../../mk/objective.mk
