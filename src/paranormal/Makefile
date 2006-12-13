include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libparanormal$(SHARED_SUFFIX)

SUBDIRS = libcalc presets

LIBDIR = $(plugindir)/$(VISUALIZATION_PLUGIN_DIR)

LIBADD = $(GTK_LIBS) $(XML_LIBS) $(SDL_LIBS) libcalc/libcalc.a

SOURCES =			\
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

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(ARCH_DEFINES) $(XML_CPPFLAGS) $(SDL_CFLAGS) -I../../intl -I../..

include ../../mk/objective.mk
