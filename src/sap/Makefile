include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libsapplug$(SHARED_SUFFIX)

SAPLIB = saplib/pokey0.cxx saplib/pokey1.cxx saplib/sapCpu.cxx saplib/sapEngine.cxx saplib/sapPokey.cxx

TARGET = libsapplug.so

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR) $(SAPLIB)

LIBADD += $(GTK_LIBS)

SOURCES = fileinfo.c sapfile.c sap_plug.c

CFLAGS   += $(GTK_CFLAGS) $(PICFLAGS) -I../../intl -I../..
CXXFLAGS += $(GTK_CFLAGS) $(PICFLAGS) -I../../intl -I../..

OBJECTS = ${SOURCES:.c=.o} $(SAPLIB:.cxx=.o)

all:

include ../../mk/objective.mk
