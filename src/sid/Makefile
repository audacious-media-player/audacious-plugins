include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libsid$(SHARED_SUFFIX)

###
### Source targets
###
EXTRA_DIST = xmms-sid-logo.xpm

# Generals
TMPFLAGS = $(PICFLAGS) -D_REENTRANT $(PLUGIN_CFLAGS) $(SIDPLAY1_INCLUDES) $(SIDPLAY2_INCLUDES) $(BUILDERS_INCLUDES) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) -I../../intl -I../.. -DAUDACIOUS_PLUGIN
CFLAGS   += $(TMPFLAGS)
CXXFLAGS  += $(TMPFLAGS)

# Generated libraries

LIBDIR = $(plugindir)/$(INPUT_PLUGIN_DIR)

LDFLAGS += $(BUILDERS_LDFLAGS)
LIBADD  = $(SIDPLAY1_LDADD) $(SIDPLAY2_LDADD) $(RESID_LDADD) $(HARDSID_LDADD) $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) -lstdc++
SOURCES =	\
	xs_init.c	\
	xs_about.c	\
	xs_support.c	\
	xs_config.c	\
	xs_md5.c	\
	xs_length.c	\
	xs_genui.c	\
	xs_glade.c	\
	xs_interface.c	\
	xs_stil.c	\
	xs_fileinfo.c	\
	xs_filter.c	\
	xs_sidplay1.cc	\
	xs_sidplay2.cc	\
	xs_curve.c	\
	xs_slsup.c	\
	xmms-sid.c

OBJECTS1 = ${SOURCES:.c=.o}
OBJECTS = ${OBJECTS1:.cc=.o}

include ../../mk/objective.mk

