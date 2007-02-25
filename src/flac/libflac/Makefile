include ../../../mk/rules.mk
include ../../../mk/init.mk

OBJECTIVE_LIBS_NOINST = libaudflac.a

SOURCES = \
	  bitmath.c			\
	  bitreader.c			\
	  bitwriter.c			\
	  cpu.c				\
	  crc.c				\
	  fixed.c			\
	  float.c			\
	  format.c			\
	  lpc.c				\
	  md5.c				\
	  memory.c			\
	  metadata_iterators.c		\
	  metadata_object.c		\
	  ogg_decoder_aspect.c		\
	  ogg_encoder_aspect.c		\
	  ogg_helper.c			\
	  ogg_mapping.c			\
	  stream_decoder.c		\
	  stream_encoder.c		\
	  stream_encoder_framing.c	\
	  window.c

OBJECTS = ${SOURCES:.c=.o}

CFLAGS += $(PICFLAGS) -I. -IFLAC $(OGG_VORBIS_CFLAGS)

libaudflac.a: ${OBJECTS}
	${AR} cq $@ ${OBJECTS}

include ../../../mk/objective.mk
