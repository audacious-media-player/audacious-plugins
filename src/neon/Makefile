include ../../mk/rules.mk
include ../../mk/init.mk

SUBDIRS = 

OBJECTIVE_LIBS = libneonsrc$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(TRANSPORT_PLUGIN_DIR)

LIBADD += $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) $(NEON_LIBS)

SOURCES = neon.c rb.c

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(BEEP_DEFINES) $(NEON_CFLAGS) -I../../intl -I../..

#CFLAGS += -Wpointer-arith -Wimplicit -Wnested-externs -Wcast-align \
#-Wcast-qual -Wstrict-prototypes -Wmissing-prototypes \
#-Wmissing-declarations -W -Wno-unused -Wshadow -Wmissing-noreturn \
#-Wundef -Wpacked -Wnested-externs -Wbad-function-cast -Wredundant-decls \
#-Wfloat-equal -Wdisabled-optimization -pedantic -std=c99

OBJECTS = ${SOURCES:.c=.o}

include ../../mk/objective.mk
