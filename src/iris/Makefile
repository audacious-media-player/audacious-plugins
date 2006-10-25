include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libiris$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(VISUALIZATION_PLUGIN_DIR)

LIBADD = $(GTK_LIBS) -lGL $(XXF86VM_LIBS)

SOURCES = \
	3Dstuff.c		\
	color.c			\
	config.c		\
	iris.c			\
	theme.c			\
	theme_flash.c		\
	theme_float.c		\
	theme_fountain.c	\
	theme_knot.c		\
	theme_original.c	\
	theme_pinwheel.c	\
	theme_pipes.c		\
	theme_pyramid.c		\
	theme_spectrotoy.c	\
	theme_spectrum.c	\
	theme_squarefield.c	\
	theme_waves.c		\
	transition.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) -D_REENTRANT $(GTK_CFLAGS) $(ARCH_DEFINES) -I../../intl -I../.. $(XXF86VM_CFLAGS)

include ../../mk/objective.mk
