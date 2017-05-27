PLUGIN = adplug${PLUGIN_SUFFIX}

SRCS = adplug-xmms.cc		\
       binio/binfile.cc		\
       binio/binio.cc		\
       binio/binstr.cc		\
       core/fmopl.cc		\
       core/adlibemu.cc		\
       core/adplug.cc		\
       core/emuopl.cc		\
       core/fprovide.cc		\
       core/player.cc		\
       core/database.cc		\
       core/hsc.cc		\
       core/sng.cc		\
       core/imf.cc		\
       core/players.cc		\
       core/protrack.cc		\
       core/a2m.cc		\
       core/adtrack.cc		\
       core/amd.cc		\
       core/bam.cc		\
       core/cmf.cc		\
       core/d00.cc		\
       core/dfm.cc		\
       core/dmo.cc		\
       core/hsp.cc		\
       core/ksm.cc		\
       core/mad.cc		\
       core/mid.cc		\
       core/mkj.cc		\
       core/cff.cc		\
       core/dtm.cc		\
       core/fmc.cc		\
       core/mtk.cc		\
       core/rad.cc		\
       core/raw.cc		\
       core/sa2.cc		\
       core/s3m.cc		\
       core/xad.cc		\
       core/flash.cc		\
       core/bmf.cc		\
       core/hybrid.cc		\
       core/hyp.cc		\
       core/psi.cc		\
       core/rat.cc		\
       core/u6m.cc		\
       core/rol.cc		\
       core/xsm.cc		\
       core/dro.cc		\
       core/dro2.cc		\
       core/lds.cc		\
       core/temuopl.cc		\
       core/msc.cc		\
       core/rix.cc		\
       core/adl.cc		\
       core/jbm.cc

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}
CFLAGS += ${PLUGIN_CFLAGS}
# FIXME: Turning off warnings for now; this code is awful
CXXFLAGS += ${PLUGIN_CFLAGS} -Wno-sign-compare -Wno-shift-negative-value
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. -I./core -I./binio
