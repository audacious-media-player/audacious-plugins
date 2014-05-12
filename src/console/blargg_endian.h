// CPU Byte Order Utilities

#ifndef BLARGG_ENDIAN
#define BLARGG_ENDIAN

#define WANT_AUD_BSWAP
#include <libaudcore/audio.h>

#include "blargg_common.h"

// BLARGG_CPU_CISC: Defined if CPU has very few general-purpose registers (< 16)
#if defined (__i386__) || defined (__x86_64__) || defined (_M_IX86) || defined (_M_X64)
	#define BLARGG_CPU_X86 1
	#define BLARGG_CPU_CISC 1
#endif

#if defined (__powerpc__) || defined (__ppc__) || defined (__ppc64__) || \
		defined (__POWERPC__) || defined (__powerc)
	#define BLARGG_CPU_POWERPC 1
	#define BLARGG_CPU_RISC 1
#endif

#ifdef WORDS_BIGENDIAN
	#define BLARGG_BIG_ENDIAN 1
#else
	#define BLARGG_LITTLE_ENDIAN 1
#endif

inline unsigned GET_LE16 (const void * p) { return FROM_LE16 (* (uint16_t *) p); }
inline unsigned GET_BE16 (const void * p) { return FROM_BE16 (* (uint16_t *) p); }
inline unsigned GET_LE32 (const void * p) { return FROM_LE32 (* (uint32_t *) p); }
inline unsigned GET_BE32 (const void * p) { return FROM_BE32 (* (uint32_t *) p); }

inline void SET_LE16 (void * p, uint16_t n) { * (uint16_t *) p = TO_LE16 (n); }
inline void SET_BE16 (void * p, uint16_t n) { * (uint16_t *) p = TO_BE16 (n); }
inline void SET_LE32 (void * p, uint32_t n) { * (uint32_t *) p = TO_LE32 (n); }
inline void SET_BE32 (void * p, uint32_t n) { * (uint32_t *) p = TO_BE32 (n); }

// auto-selecting versions

inline void set_le (uint16_t * p, uint16_t n) { SET_LE16 (p, n); }
inline void set_le (uint32_t * p, uint32_t n) { SET_LE32 (p, n); }
inline void set_be (uint16_t * p, uint16_t n) { SET_BE16 (p, n); }
inline void set_be (uint32_t * p, uint32_t n) { SET_BE32 (p, n); }

inline uint16_t get_le (const uint16_t * p) { return GET_LE16 (p); }
inline uint32_t get_le (const uint32_t * p) { return GET_LE32 (p); }
inline uint16_t get_be (const uint16_t * p) { return GET_BE16 (p); }
inline uint32_t get_be (const uint32_t * p) { return GET_BE32 (p); }

#endif
