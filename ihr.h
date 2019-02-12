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
#define IHRT_S19	3
#define IHRT_S28	4
#define IHRT_S37	5

#define IHRE_EXPECTED_EOL	1
#define IHRE_INVALID_CHECKSUM	2
#define IHRE_INVALID_SIZE	3
#define IHRE_INVALID_TYPE	4
#define IHRE_MISSING_START	5
#define IHRE_NOT_HEX		7
#define IHRE_SUB_MIN_LENGTH	9

/* Intel HEX record types */
#define IHRR_I_DATA		0x00
#define IHRR_I_END_OF_FILE	0x01
#define IHRR_I_EXT_SEG_ADDR	0x02
#define IHRR_I_START_SEG_ADDR	0x03
#define IHRR_I_EXT_LIN_ADDR	0x04
#define IHRR_I_START_LIN_ADDR	0x05

/* Motorola SREC record types */
#define IHRR_S0_HEADER		0x0
#define IHRR_S1_DATA_16		0x1
#define IHRR_S2_DATA_24		0x2
#define IHRR_S3_DATA_32		0x3
#define IHRR_S5_COUNT_16	0x5
#define IHRR_S6_COUNT_24	0x6
#define IHRR_S7_START_32	0x7
#define IHRR_S8_START_24	0x8
#define IHRR_S9_START_16	0x9

#define IHR_I_MIN_LENGTH 11
#define IHR_S_MIN_LENGTH 10
#define IHR_MAX_LENGTH 525

#define IHR_MAX_SIZE 255

struct ihr_record {
	char type;
	IHR_U8 size;
	IHR_U32 addr;
	union {
		IHR_U8 *data;
		union {
			IHR_U16 base_addr;
			IHR_U32 ext_instr_ptr;
			struct {
				IHR_U16 code_seg;
				IHR_U16 instr_ptr;
			} start;
		} ihex;
		union {
			IHR_U32 start;
			IHR_U32 count;
		} srec;
	} data;
};

int ihr_read(int file_type,
	size_t len,
	const char *text,
	struct ihr_record *rec);

#endif /* IHR_INCLUDED */
