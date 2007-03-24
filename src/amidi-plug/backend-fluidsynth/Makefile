include ../../../mk/rules.mk
include ../../../mk/init.mk

noinst_HEADERS = b-fluidsynth.h b-fluidsynth-config.h backend-fluidsynth-icon.xpm

CFLAGS += $(PICFLAGS) $(GLIB_CFLAGS) $(FLUIDSYNTH_CFLAGS) -I../../..

libdir = $(AMIDIPLUGBACKENDDIR)

OBJECTIVE_LIBS = ap-fluidsynth$(SHARED_SUFFIX)

LIBADD = -lm $(GLIB_LIBS) $(FLUIDSYNTH_LIBS) ../pcfg/libpcfg.a
SOURCES = b-fluidsynth.c

OBJECTS = ${SOURCES:.c=.o}

LIBDEP =  ../pcfg/libpcfg.a

include ../../../mk/objective.mk
