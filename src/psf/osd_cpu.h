/*******************************************************************************
*																			   *
*	Define size independent data types and operations.						   *
*																			   *
*   The macro names for the arithmetic operations are composed as follows:     *
*																			   *
*   XXX_R_A_B, where XXX - 3 letter operation code (ADD, SUB, etc.)			   *
*					 R   - The type	of the result							   *
*					 A   - The type of operand 1							   *
*			         B   - The type of operand 2 (if binary operation)		   *
*																			   *
*				     Each type is one of: U8,8,U16,16,U32,32,U64,64			   *
*																			   *
*******************************************************************************/


#ifndef OSD_CPU_H
#define OSD_CPU_H

#include <stdint.h>

/* Combine two 32-bit integers into a 64-bit integer */
#define COMBINE_64_32_32(A,B)     ((((uint64_t)(A))<<32) | (uint32_t)(B))
#define COMBINE_U64_U32_U32(A,B)  COMBINE_64_32_32(A,B)

/* Return upper 32 bits of a 64-bit integer */
#define HI32_32_64(A)		  (((uint64_t)(A)) >> 32)
#define HI32_U32_U64(A)		  HI32_32_64(A)

/* Return lower 32 bits of a 64-bit integer */
#define LO32_32_64(A)		  ((A) & 0xffffffff)
#define LO32_U32_U64(A)		  LO32_32_64(A)

#define DIV_64_64_32(A,B)	  ((A)/(B))
#define DIV_U64_U64_U32(A,B)  ((A)/(uint32_t)(B))

#define MOD_32_64_32(A,B)	  ((A)%(B))
#define MOD_U32_U64_U32(A,B)  ((A)%(uint32_t)(B))

#define MUL_64_32_32(A,B)	  ((A)*(int64_t)(B))
#define MUL_U64_U32_U32(A,B)  ((A)*(uint64_t)(uint32_t)(B))


/******************************************************************************
 * Union of uint8_t, uint16_t and uint32_t in native endianess of the target
 * This is used to access bytes and words in a machine independent manner.
 * The upper bytes h2 and h3 normally contain zero (16 bit CPU cores)
 * thus PAIR.d can be used to pass arguments to the memory system
 * which expects 'int' really.
 ******************************************************************************/
typedef union {
#if LSB_FIRST
	struct { uint8_t l,h,h2,h3; } b;
	struct { uint16_t l,h; } w;
#else
	struct { uint8_t h3,h2,h,l; } b;
	struct { uint16_t h,l; } w;
#endif
	uint32_t d;
}	PAIR;

#endif	/* defined OSD_CPU_H */
