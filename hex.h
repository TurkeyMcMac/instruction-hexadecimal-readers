#ifndef HEX_INCLUDED
#define HEX_INCLUDED

#include <stdio.h>

typedef   signed char HEX_S8;
typedef unsigned char HEX_U8;

typedef   signed short HEX_S16;
typedef unsigned short HEX_U16;

#if INT_MAX == 4294967295 /* int is 32 bits */
typedef   signed int HEX_S32;
typedef unsigned int HEX_U32;
#else
typedef   signed long HEX_S32;
typedef unsigned long HEX_U32;
#endif /* int is 32 bits */

#define HEXT_I8		0
#define HEXT_I16	1
#define HEXT_I32	2

#define HEXE_EXPECTED_EOL	1
#define HEXE_INVALID_CHECKSUM	2
#define HEXE_INVALID_SIZE	3
#define HEXE_INVALID_TYPE	4
#define HEXE_MISSING_COLON	5
#define HEXE_MISSING_EOF	6
#define HEXE_NOT_HEX		7
#define HEXE_UNEXPECTED_EOF	8
#define HEXE_IO_ERROR		9

struct hex_reader {
	FILE *source;
	int line;
	int type;
};

#define HEXR_DATA		0x00
#define HEXR_END_OF_FILE	0x01
#define HEXR_EXT_SEG_ADDR	0x02
#define HEXR_START_SEG_ADDR	0x03
#define HEXR_EXT_LIN_ADDR	0x04
#define HEXR_START_LIN_ADDR	0x05

struct hex_record {
	HEX_U8 type;
	HEX_U8 size;
	HEX_U16 addr;
	union {
		HEX_U8 *data;
		HEX_U16 base_addr;
		HEX_U32 ext_instr_ptr;
		struct {
			HEX_U16 code_seg;
			HEX_U16 instr_ptr;
		} start;
	} data;
};

int hex_read(struct hex_reader *from, struct hex_record *rec);

int hex_write(FILE *into, const struct hex_record *rec);

#endif /* HEX_INCLUDED */
