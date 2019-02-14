#include "ihr.h"

#define SUCCESS 0
#define FAILURE -1

/* Convert a hex digit to its integer form, or -1 if the given is not hex.
 * TODO: Do we need to accept lowercase hex? */
static int read_nibble(char hex)
{
	if ('0' <= hex && hex <= '9') return hex - '0';
	hex |= 0x20; /* Normalize to lowercase */
	if ('a' <= hex && hex <= 'f') return 10 + hex - 'a';
	return FAILURE;
}

/* Convert two hex digits to an unsigned byte, or -1 if a digit was invalid. */
static int read_u8(const char hex[2])
{
	return (read_nibble(hex[0]) << 4) | read_nibble(hex[1]);
}

/* Returns 1 if the type is valid for the given file type or 0 otherwise. */
static int ihex_valid_type(int file_type, IHR_U8 type)
{
	switch (type) {
	case IHRR_I_DATA:
	case IHRR_I_END_OF_FILE:
		return 1;
	case IHRR_I_EXT_SEG_ADDR:
	case IHRR_I_START_SEG_ADDR:
		return file_type == IHRT_I16;
	case IHRR_I_EXT_LIN_ADDR:
	case IHRR_I_START_LIN_ADDR:
		return file_type == IHRT_I32;
	default:
		return 0;
	}
}

/* Returns 1 if the type is valid for the given file type or 0 otherwise. */
static int srec_valid_type(int file_type, IHR_U8 type)
{
	switch (type) {
	case IHRR_S0_HEADER:
	case IHRR_S5_COUNT_16:
		return 1;
	case IHRR_S1_DATA_16:
	case IHRR_S9_START_16:
		return file_type == IHRT_S19;
	case IHRR_S2_DATA_24:
	case IHRR_S8_START_24:
		return file_type == IHRT_S28;
	case IHRR_S3_DATA_32:
	case IHRR_S7_START_32:
		return file_type == IHRT_S37;
	case IHRR_S6_COUNT_24:
		return file_type != IHRT_S19;
	default:
		return 0;
	}
}

/* Choose an error code to suit a pair of unparseable digits. Returns
 * -IHRE_INVALID_SIZE if the pair was a line break (the record was shorter than
 * indicated,) or -IHRE_NOT_HEX otherwise. */
static int invalid_hex_error(const char *pair)
{
	switch (pair[0]) {
	case '\r':
	case '\n':
		return -IHRE_INVALID_SIZE;
	default:
		return -IHRE_NOT_HEX;
	}
}

static int find_line_end(const char *text,
	size_t len,
	size_t *idx,
	struct ihr_record *rec)
{
	if (*idx < len) {
		switch (text[*idx]) {
		case '\n':
			break;
		case '\r':
			++*idx;
			if (*idx >= len || text[*idx] != '\n')
				--*idx;
			break;
		default:
			if (rec->size < IHR_MAX_SIZE)
				rec->type = -IHRE_INVALID_SIZE;
			else
				rec->type = -IHRE_EXPECTED_EOL;
			return FAILURE;
		}
		++*idx;
	}
	return SUCCESS;
}

static int read_data(const char *text, size_t *idx, struct ihr_record *rec)
{
	int status = SUCCESS;
	const char *hex = text + *idx;
	IHR_U8 i;
	for (i = 0; i < rec->size; ++i) {
		const char *pair = hex + i * 2;
		int byte = read_u8(pair);
		if (byte < 0) {
			rec->type = invalid_hex_error(pair);
			status = FAILURE;
			break;
		}
		rec->data.data[i] = byte;
	}
	*idx += (size_t)i * 2;
	return status;
}

static int ihex_read(int file_type,
	size_t len,
	const char *text,
	struct ihr_record *rec)
{
	size_t idx = 0;
	int read_cksum;
	/* Check that the given text can be a valid record: */
	if (len < IHR_I_MIN_LENGTH) {
		rec->type = -IHRE_SUB_MIN_LENGTH;
		goto error;
	}
	/* Check for record beginning colon: */
	if (text[idx] != ':') {
		rec->type = -IHRE_MISSING_START;
		goto error;
	} else {
		idx += 1;
	}
	/* Read in fields at record beginning (size, address, type): */
	{
		int size, addr, type;
		size = read_u8(text + idx);
		if (size < 0) goto error_not_hex;
		rec->size = size;
		idx += 2;
		addr = read_u8(text + idx);
		if (addr < 0) goto error_not_hex;
		rec->addr = addr << 8;
		idx += 2;
		addr = read_u8(text + idx);
		if (addr < 0) goto error_not_hex;
		rec->addr |= addr;
		idx += 2;
		type = read_u8(text + idx);
		if (type < 0) goto error_not_hex;
		rec->type = type;
		if (!ihex_valid_type(file_type, type)) {
			rec->type = -IHRE_INVALID_TYPE;
			goto error;
		}
		idx += 2;
	}
	/* Read data field: */
	{
		int min_size;
		if (len < idx + ((size_t)rec->size + 1) * 2)
			goto error_invalid_size;
		if (rec->type == IHRR_I_END_OF_FILE) {
			if (rec->size != 0) goto error_invalid_size;
		} else {
			switch (rec->type) {
			case IHRR_I_EXT_SEG_ADDR:
			case IHRR_I_EXT_LIN_ADDR:
				min_size = 2;
				break;
			case IHRR_I_START_SEG_ADDR:
			case IHRR_I_START_LIN_ADDR:
				min_size = 4;
				break;
			default:
				min_size = -1;
				break;
			}
			if ((int)rec->size < min_size) {
				rec->type = -IHRE_INVALID_SIZE;
				goto error_invalid_size;
			}
			if (read_data(text, &idx, rec)) goto error;
		}
	}
	/* Read in the checksum (verification comes later): */
	if ((read_cksum = read_u8(text + idx)) < 0) {
		rec->type = invalid_hex_error(text + idx);
		goto error;
	} else {
		idx += 2;
	}
	if (!find_line_end(text, len, &idx, rec)) goto error;
	/* Verify checksum: */
	{
		/* The checksum is the two's complement of the least significant
		 * byte of the sum of all preceding bytes. */
		IHR_U8 right_cksum = 0;
		IHR_U8 i = 0;
		right_cksum += rec->size;
		right_cksum += rec->addr >> 8;
		right_cksum += rec->addr & 0xFF;
		right_cksum += rec->type;
		for (i = 0; i < rec->size; ++i) {
			right_cksum += rec->data.data[i];
		}
		right_cksum = (~right_cksum + 1) & 0xFF;
		if ((IHR_U8)read_cksum != right_cksum) {
			rec->type = -IHRE_INVALID_CHECKSUM;
			goto error;
		}
	}
	/* Transfer data from rec->data.data to record-type-specific fields: */
	{
		IHR_U8 *data = rec->data.data;
		switch (rec->type) {
		case IHRR_I_DATA:
		case IHRR_I_END_OF_FILE:
			break;
		case IHRR_I_EXT_SEG_ADDR:
		case IHRR_I_EXT_LIN_ADDR:
			rec->data.ihex.base_addr = ((IHR_U16)data[0] << 8)
				| (IHR_U16)data[1];
			break;
		case IHRR_I_START_SEG_ADDR:
			rec->data.ihex.start.code_seg = ((IHR_U16)data[0] << 8)
				| (IHR_U16)data[1];
			rec->data.ihex.start.instr_ptr = ((IHR_U16)data[2] << 8)
				| (IHR_U16)data[3];
			break;
		case IHRR_I_START_LIN_ADDR:
			rec->data.ihex.ext_instr_ptr = ((IHR_U32)data[0] << 24)
				| ((IHR_U32)data[1] << 16)
				| ((IHR_U32)data[2] << 8) | (IHR_U32)data[3];
			break;
		}
	}
	return idx;

error_invalid_size:
	/* The reported size of the record was incorrect. */
	rec->type = -IHRE_INVALID_SIZE;
	return ~1;

error_not_hex:
	/* A character which should have been a hex digit was not. */
	rec->type = -IHRE_NOT_HEX;
	return ~idx;

error:
	/* Any other error. */
	return ~idx;
}

IHR_U8 srec_addr_size(IHR_U8 type)
{
	switch (type) {
	case IHRR_S0_HEADER:
	case IHRR_S1_DATA_16:
	case IHRR_S5_COUNT_16:
	case IHRR_S9_START_16:
		return 2;
	case IHRR_S2_DATA_24:
	case IHRR_S6_COUNT_24:
	case IHRR_S8_START_24:
		return 3;
	case IHRR_S3_DATA_32:
	case IHRR_S7_START_32:
		return 4;
	}
	return 2;
}

int srec_read(int file_type,
	size_t len,
	const char *text,
	struct ihr_record *rec)
{
	size_t idx = 0;
	int addr_size;
	int read_cksum;
	/* Check that the given text can be a valid record: */
	if (len < IHR_S_MIN_LENGTH) {
		rec->type = -IHRE_SUB_MIN_LENGTH;
		goto error;
	}
	/* Check for record beginning S: */
	if (text[idx] != 'S') {
		rec->type = -IHRE_MISSING_START;
		goto error;
	} else {
		idx += 1;
	}
	/* Read in fields at record beginning (type, size, address): */
	{
		int i, size, type;
		type = read_nibble(text[idx]);
		if (type < 0) goto error_not_hex;
		rec->type = type;
		if (!srec_valid_type(file_type, type)) {
			rec->type = -IHRE_INVALID_TYPE;
			goto error;
		}
		addr_size = srec_addr_size(rec->type);
		idx += 1;
		size = read_u8(text + idx);
		if (size < 0) goto error_not_hex;
		size -= addr_size + 1;
		if (size < 0) goto error_invalid_size;
		rec->size = size;
		idx += 2;
		rec->addr = 0;
		for (i = 0; i < addr_size; ++i) {
			int addr = read_u8(text + idx);
			if (addr < 0) goto error_not_hex;
			idx += 2;
			rec->addr |= addr;
			rec->addr <<= 8;
		}
	}
	/* Read data field: */
	{
		switch (rec->type) {
		case IHRR_S0_HEADER:
		case IHRR_S1_DATA_16:
		case IHRR_S2_DATA_24:
		case IHRR_S3_DATA_32:
			if (len < idx + ((size_t)rec->size + 1) * 2)
				goto error_invalid_size;
			if (read_data(text, &idx, rec)) goto error;
			break;
		default:
			if (rec->size != 0) goto error_invalid_size;
			break;
		}
	}
	/* Read in the checksum (verification comes later): */
	if ((read_cksum = read_u8(text + idx)) < 0) {
		rec->type = invalid_hex_error(text + idx);
		goto error;
	} else {
		idx += 2;
	}
	if (!find_line_end(text, len, &idx, rec)) goto error;
	/* Verify checksum: */
	{
		/* The checksum is the one's complement of the least significant
		 * byte of the sum of all preceding bytes (not the type.) */
		IHR_U8 right_cksum = 0;
		IHR_U32 addr = rec->addr;
		IHR_U8 i;
		right_cksum += rec->size + addr_size + 1;
		for (i = 0; i < addr_size; ++i) {
			right_cksum += addr & 0xFF;
			addr >>= 8;
		}
		for (i = 0; i < rec->size; ++i) {
			right_cksum += rec->data.data[i];
		}
		right_cksum = ~right_cksum & 0xFF;
		if ((IHR_U8)read_cksum != right_cksum) {
			rec->type = -IHRE_INVALID_CHECKSUM;
			goto error;
		}
	}
	return idx;

error_invalid_size:
	/* The reported size of the record was incorrect. */
	rec->type = -IHRE_INVALID_SIZE;
	return ~1;

error_not_hex:
	/* A character which should have been a hex digit was not. */
	rec->type = -IHRE_NOT_HEX;
	return ~idx;

error:
	/* Any other error. */
	return ~idx;
}

int ihr_read(int file_type,
	size_t len,
	const char *text,
	struct ihr_record *rec)
{
	switch (file_type) {
	case IHRT_I8:
	case IHRT_I16:
	case IHRT_I32:
		return ihex_read(file_type, len, text, rec);
	case IHRT_S19:
	case IHRT_S28:
	case IHRT_S37:
		return srec_read(file_type, len, text, rec);
	}
	return FAILURE; /* It is undefined behavior to reach here. */
}
