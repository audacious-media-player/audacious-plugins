include ../../../mk/rules.mk
include ../../../mk/init.mk

CXXFLAGS += $(PICFLAGS) -I../../.. $(GTK_CFLAGS) $(GLIB_CFLAGS) $(PANGO_CFLAGS) $(AUDACIOUS_CFLAGS)

OBJECTIVE_LIBS_NOINST = libarchive.a

SOURCES = archive.cxx     \
          open.cxx        \
          arch_raw.cxx    \
          arch_gzip.cxx   \
          arch_zip.cxx    \
          arch_rar.cxx    \
          arch_bz2.cxx

OBJECTS = ${SOURCES:.cxx=.o}

include ../../../mk/objective.mk

libarchive.a: $(OBJECTS)
	$(AR) cq $@ $(OBJECTS)
