PLUGIN = xsf${PLUGIN_SUFFIX}

SRCS = corlett.c \
       plugin.c \
       vio2sf.c \
       desmume/armcpu.c            desmume/bios.c  desmume/FIFO.c  desmume/matrix.c  desmume/MMU.c        desmume/SPU.c \
       desmume/arm_instructions.c  desmume/cp15.c  desmume/GPU.c   desmume/mc.c      desmume/NDSSystem.c  desmume/thumb_instructions.c \

include ../../buildsys.mk
include ../../extra.mk

plugindir := ${plugindir}/${INPUT_PLUGIN_DIR}

CFLAGS += ${PLUGIN_CFLAGS} -O0
CPPFLAGS += ${PLUGIN_CPPFLAGS} -I../.. -Ispu/
LIBS += -lm -lz
