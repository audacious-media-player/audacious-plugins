include ../../extra.mk

PLUGIN = sexypsf$(PLUGIN_SUFFIX)
plugindir = audacious/$(INPUT_PLUGIN_DIR)

SRCS = \
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

LIBS += -lz $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)

include ../../buildsys.mk

CPPFLAGS += $(MOWGLI_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../.. -Ispu/ -I.
CFLAGS += $(PLUGIN_CFLAGS)

clean-posthook:
	@rm -f spu/*.o
