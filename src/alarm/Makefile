include ../../mk/rules.mk
include ../../mk/init.mk

SUBDIRS = 

OBJECTIVE_LIBS = libalarm$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(GENERAL_PLUGIN_DIR)

LIBADD += $(GTK_LIBS)

SOURCES = alarm.c interface.c

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(BEEP_DEFINES) -I../../intl -I../.. -DGTK_ENABLE_BROKEN

OBJECTS = ${SOURCES:.c=.o}

include ../../mk/objective.mk
