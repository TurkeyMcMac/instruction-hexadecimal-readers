#ifndef HEX_INCLUDED
#define HEX_INCLUDED

#include <stdio.h>
#include <limits.h>

typedef unsigned char HEX_U8;
typedef unsigned short HEX_U16;
typedef unsigned
#if INT_MAX == SHORT_MAX /* int is 16 bits */
	long
#else
	short
#endif /* int is 16 bits */
HEX_U32;

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

int hex_read(int type, FILE *from, struct hex_record *rec);

const char *hex_errstr(int errnum);

#endif /* HEX_INCLUDED */
