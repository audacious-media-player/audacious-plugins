include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = liblirc$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(GENERAL_PLUGIN_DIR)

LIBADD = -llirc_client $(AUDACIOUS_LIBS)
SOURCES = about.c lirc.c

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(AUDACIOUS_CFLAGS) -I../../intl -I../..

OBJECTS = ${SOURCES:.c=.o}

include ../../mk/objective.mk
