PLUGIN = paranormal$(PLUGIN_SUFFIX)
plugindir = audacious/$(VISUALIZATION_PLUGIN_DIR)

SRCS =			\
	actuators.c		\
	beatdetect.c		\
	builtins.c		\
	cfg.c			\
	cmaps.c			\
	containers.c		\
	drawing.c		\
	freq.c			\
	general.c		\
	misc.c			\
	paranormal.c		\
	plugin.c		\
	presets.c		\
	wave.c			\
	xform.c

EXT_DEPS = libcalc/libcalc.a

include ../../buildsys.mk
include ../../extra.mk

SUBDIRS = libcalc presets

CFLAGS += $(PLUGIN_CFLAGS)
CPPFLAGS += $(MOWGLI_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(ARCH_DEFINES) $(XML_CPPFLAGS) $(SDL_CFLAGS) -I../../intl \
	-I../.. -I.
LIBS += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(XML_LIBS) $(SDL_LIBS) libcalc/libcalc.a
