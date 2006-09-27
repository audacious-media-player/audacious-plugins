include ../../../mk/rules.mk
include ../../../mk/init.mk

OBJECTIVE_LIBS_NOINST = libalac.a

SOURCES = alac.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) -I../../..

include ../../../mk/objective.mk

libalac.a: $(OBJECTS)
	$(AR) cq $@ $(OBJECTS)
