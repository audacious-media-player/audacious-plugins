PLUGIN = xsf${PLUGIN_SUFFIX}

SRCS = corlett.cc \
       plugin.cc \
       vio2sf.cc \
       desmume/armcpu.cc            desmume/bios.cc  desmume/FIFO.cc  desmume/matrix.cc  desmume/MMU.cc        desmume/SPU.cc \
       desmume/arm_instructions.cc  desmume/cp15.cc  desmume/GPU.cc   desmume/mc.cc      desmume/NDSSystem.cc  desmume/thumb_instructions.cc \

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

LD = ${CXX}

CXXFLAGS += ${PLUGIN_CFLAGS} -Wno-sign-compare -Wno-shift-negative-value -Wno-bool-operation
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. -Ispu/
LIBS += -lm -lz
