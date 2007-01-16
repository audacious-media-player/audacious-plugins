include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libstatusicon$(SHARED_SUFFIX)

noinst_HEADERS = si.h si_ui.h si_audacious.h si_common.h gtktrayicon.h si.xpm si2.xpm

LIBDIR = $(plugindir)/$(GENERAL_PLUGIN_DIR)

LIBADD = $(GTK_LIBS) $(GDKX11_LIBS)
SOURCES = si.c si_ui.c gtktrayicon-x11.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GDKX11_CFLAGS) \
	-I../../intl -I../.. -I..

include ../../mk/objective.mk
