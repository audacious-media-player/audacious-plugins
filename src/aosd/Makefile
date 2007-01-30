include ../../mk/rules.mk
include ../../mk/init.mk

OBJECTIVE_LIBS = libaosd$(SHARED_SUFFIX)

noinst_HEADERS = aosd.h aosd_osd.h aosd_style.h aosd_style_private.h aosd_ui.h aosd_cfg.h aosd_common.h ghosd.h ghosd-text.h ghosd-internal.h

LIBDIR = $(plugindir)/$(GENERAL_PLUGIN_DIR)

LIBADD = $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS)
SOURCES = aosd.c aosd_osd.c aosd_style.c aosd_ui.c aosd_cfg.c ghosd.c ghosd-text.c ghosd-main.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) \
	-I../../intl -I../.. -I..

include ../../mk/objective.mk
