#ifndef IHR_INCLUDED
#define IHR_INCLUDED

#include <stddef.h>
#include <limits.h>

typedef unsigned char IHR_U8;
typedef unsigned short IHR_U16;
typedef unsigned
#if INT_MAX == SHORT_MAX /* int is 16 bits */
	long
#else
	short
#endif /* int is 16 bits */
IHR_U32;

#define IHRT_I8		0
#define IHRT_I16	1
#define IHRT_I32	2

#define IHRE_EXPECTED_EOL	1
#define IHRE_INVALID_CHECKSUM	2
#define IHRE_INVALID_SIZE	3
#define IHRE_INVALID_TYPE	4
#define IHRE_MISSING_COLON	5
#define IHRE_MISSING_EOF	6
#define IHRE_NOT_HEX		7
#define IHRE_UNEXPECTED_EOF	8
#define IHRE_SUB_MIN_LENGTH	9

#define IHRR_DATA		0x00
#define IHRR_END_OF_FILE	0x01
#define IHRR_EXT_SEG_ADDR	0x02
#define IHRR_START_SEG_ADDR	0x03
#define IHRR_EXT_LIN_ADDR	0x04
#define IHRR_START_LIN_ADDR	0x05

#define IHR_MIN_LENGTH 11
#define IHR_MAX_LENGTH 525

struct ihr_record {
	char type;
	IHR_U8 size;
	IHR_U16 addr;
	union {
		IHR_U8 *data;
		IHR_U16 base_addr;
		IHR_U32 ext_instr_ptr;
		struct {
			IHR_U16 code_seg;
			IHR_U16 instr_ptr;
		} start;
	} data;
};

int ihr_read(int file_type,
	size_t len,
	const char *text,
	struct ihr_record *rec);

#endif /* IHR_INCLUDED */
