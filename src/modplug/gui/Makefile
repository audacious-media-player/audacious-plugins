include ../../../mk/rules.mk
include ../../../mk/init.mk

CFLAGS   += $(PICFLAGS) $(GTK_CFLAGS) -I../../..
CXXFLAGS += $(PICFLAGS) $(GTK_CFLAGS) -I../../..

OBJECTIVE_LIBS_NOINST = libgui.a
SOURCES = main.cxx support.cxx interface.cxx callbacks.cxx

OBJECTS = ${SOURCES:.cxx=.o}

include ../../../mk/objective.mk

libgui.a: $(OBJECTS)
	$(AR) cq $@ $(OBJECTS)
