include ../../../mk/rules.mk
include ../../../mk/init.mk

CFLAGS   += $(PICFLAGS) -I../../intl -I../..
CXXFLAGS += $(PICFLAGS) -I../../intl -I../..

OBJECTIVE_LIBS_NOINST = libsap.a
SOURCES = pokey0.cxx pokey1.cxx sapCpu.cxx sapEngine.cxx sapPokey.cxx

OBJECTS = ${SOURCES:.cxx=.o}

include ../../../mk/objective.mk

libsap.a: $(OBJECTS)
	$(AR) cq $@ $(OBJECTS)
