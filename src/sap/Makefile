include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libsapplug$(SHARED_SUFFIX)

SAPLIB = saplib/pokey0.o saplib/pokey1.o saplib/sapCpu.o saplib/sapEngine.o saplib/sapPokey.o

LINKER_FLAGS = -shared -Wl,-soname -Wl,$(TARGET) \
	-Wl,-retain-symbols-file -Wl,syms

TARGET = libsapplug.so

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR) $(SAPLIB)

LIBADD += $(GTK_LIBS)

SOURCES = fileinfo.c sapfile.c sap_plug.c

CFLAGS   += $(GTK_CFLAGS) $(LINKER_FLAGS) -I../../intl -I../..
CXXFLAGS += $(GTK_CFLAGS) $(LINKER_FLAGS) -I../../intl -I../..

OBJECTS = ${SOURCES:.c=.o} $(SAPLIB)

all: library 

library: 
	cd saplib && make 

include ../../mk/objective.mk
