include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libsapplug$(SHARED_SUFFIX)
SUBDIRS = saplib

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

LIBADD += $(GTK_LIBS) ./saplib/libsap.a

SOURCES = fileinfo.c sapfile.c sap_plug.c

CFLAGS   += $(GTK_CFLAGS) $(PICFLAGS) -I../../intl -I../..
CXXFLAGS += $(GTK_CFLAGS) $(PICFLAGS) -I../../intl -I../..

OBJECTS = ${SOURCES:.c=.o}

include ../../mk/objective.mk
