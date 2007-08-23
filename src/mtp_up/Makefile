include ../../mk/rules.mk
include ../../mk/init.mk

SUBDIRS = 

OBJECTIVE_LIBS = libmtp_up$(SHARED_SUFFIX)

LIBDIR = $(plugindir)/$(GENERAL_PLUGIN_DIR)

LIBADD += $(MTP_LIBS) $(GTK_LIBS) $(GLIB_LIBS) $(PANGO_LIBS) 
SOURCES = mtp.c filetype.c

CFLAGS += $(PICFLAGS) $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(BEEP_DEFINES) -I../../intl -I../..

CFLAGS += -Wall  -pedantic -std=c99 $(GCC42_CFLAGS) -D_BSD_SOURCE

OBJECTS = ${SOURCES:.c=.o}

include ../../mk/objective.mk
