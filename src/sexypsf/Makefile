include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libsexypsf$(SHARED_SUFFIX)

LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

SOURCES = \
	PsxBios.c \
	PsxCounters.c \
	PsxDma.c \
	Spu.c \
	PsxMem.c \
	PsxHw.c \
	Misc.c \
	R3000A.c \
	PsxInterpreter.c \
	PsxHLE.c \
	spu/spu.c \
	plugin.c

LIBADD = -lz

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../.. -Ispu/ -I.

include ../../mk/objective.mk

clean-posthook:
	@rm -f spu/*.o
