include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libdisk_writer$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(OUTPUT_PLUGIN_DIR)

LIBADD = $(GTK_LIBS) $(AUDACIOUS_LIBS)
SOURCES = disk_writer.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(AUDACIOUS_CFLAGS) -I../../intl -I../..

include ../../mk/objective.mk
