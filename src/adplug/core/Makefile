include ../../../mk/rules.mk
include ../../../mk/init.mk

OBJECTIVE_LIBS_NOINST = libadplugcore.a

C_SOURCES = fmopl.c debug.c adlibemu.c
CXX_SOURCES = adplug.cxx emuopl.cxx diskopl.cxx fprovide.cxx \
player.cxx database.cxx hsc.cxx sng.cxx imf.cxx players.cxx protrack.cxx \
a2m.cxx adtrack.cxx amd.cxx bam.cxx d00.cxx dfm.cxx dmo.cxx hsp.cxx ksm.cxx \
mad.cxx mid.cxx mkj.cxx cff.cxx dtm.cxx fmc.cxx mtk.cxx rad.cxx raw.cxx \
sa2.cxx s3m.cxx xad.cxx flash.cxx bmf.cxx hybrid.cxx hyp.cxx psi.cxx rat.cxx \
u6m.cxx rol.cxx xsm.cxx dro.cxx lds.cxx temuopl.cxx msc.cxx rix.cxx adl.cxx \
jbm.cxx

SOURCES = $(C_SOURCES) $(CXX_SOURCES)

noinst_HEADERS = adplug.h emuopl.h fmopl.h silentopl.h opl.h diskopl.h \
a2m.h amd.h bam.h d00.h dfm.h hsc.h hsp.h imf.h ksm.h lds.h mid.h mkj.h mtk.h \
protrack.h rad.h raw.h sa2.h sng.h u6m.h player.h fmc.h mad.h xad.h bmf.h \
flash.h hyp.h psi.h rat.h hybrid.h rol.h adtrack.h cff.h dtm.h fprovide.h \
database.h players.h xsm.h adlibemu.h kemuopl.h dro.h dmo.h s3m.h temuopl.h \
msc.h rix.h adl.h jbm.h

CXXFLAGS += $(PICFLAGS) $(BINIO_CFLAGS) -I../../../../intl -I../../../.. -Dstricmp=strcasecmp $(BEEP_DEFINES)
CFLAGS += $(PICFLAGS) $(BINIO_CFLAGS) -I../../../../intl -I../../../.. -Dstricmp=strcasecmp $(BEEP_DEFINES)

OBJECTS = ${C_SOURCES:.c=.o} ${CXX_SOURCES:.cxx=.o}

include ../../../mk/objective.mk
