#include "ihr.h"

/* Convert a hex digit to its integer form, or -1 if the given is not hex.
 * TODO: Do we need to accept lowercase hex? */
static int read_nibble(char hex)
{
	if ('0' <= hex && hex <= '9') return hex - '0';
	hex |= 0x20;
	if ('a' <= hex && hex <= 'f') return 10 + hex - 'a';
	return -1;
}

/* Convert two hex digits to an unsigned byte, or -1 if a digit was invalid. */
static int read_u8(const char hex[2])
{
	return (read_nibble(hex[0]) << 4) | read_nibble(hex[1]);
}

/* Returns 1 if the type is valid for the given file type or 0 otherwise. */
static int valid_type(int file_type, IHR_U8 type)
{
	switch (type) {
	case IHRR_DATA:
	case IHRR_END_OF_FILE:
		return 1;
	case IHRR_EXT_SEG_ADDR:
	case IHRR_START_SEG_ADDR:
		return file_type == IHRT_I16;
	case IHRR_EXT_LIN_ADDR:
	case IHRR_START_LIN_ADDR:
		return file_type == IHRT_I32;
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
	case '\n':
		return -IHRE_INVALID_SIZE;
	case '\r':
		if (pair[1] == '\n') return -IHRE_INVALID_SIZE;
		/* FALLTHROUGH */
	default:
		return -IHRE_NOT_HEX;
	}
}


int ihr_read(int file_type,
	size_t len,
	const char *text,
	struct ihr_record *rec)
{
	size_t idx = 0;
	int read_cksum;
	/* Check that the given text can be a valid record: */
	if (len < IHR_MIN_LENGTH) {
		rec->type = -IHRE_SUB_MIN_LENGTH;
		goto error;
	}
	/* Check for record beginning colon: */
	if (text[idx] != ':') {
		rec->type = -IHRE_MISSING_COLON;
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
		if (!valid_type(file_type, type)) {
			rec->type = -IHRE_INVALID_TYPE;
			goto error;
		}
		idx += 2;
	}
	/* Read data field: */
	{
		IHR_U8 i;
		int min_size;
		const char *hex;
		if (len < idx + ((size_t)rec->size + 1) * 2)
			goto error_invalid_size;
		switch (rec->type) {
			case IHRR_EXT_SEG_ADDR:
			case IHRR_EXT_LIN_ADDR:
				min_size = 2;
				break;
			case IHRR_START_SEG_ADDR:
			case IHRR_START_LIN_ADDR:
				min_size = 4;
				break;
			default:
				min_size = -1;
		}
		if ((int)rec->size < min_size) {
			rec->type = -IHRE_INVALID_SIZE;
			goto error_invalid_size;
		}
		hex = text + idx;
		for (i = 0; i < rec->size; ++i) {
			const char *pair = hex + i * 2;
			int byte = read_u8(pair);
			if (byte < 0) {
				rec->type = invalid_hex_error(pair);
				idx += i * 2;
				goto error;
			}
			rec->data.data[i] = byte;
		}
		idx += (size_t)i * 2;
	}
	/* Read in the checksum (verification comes later): */
	if ((read_cksum = read_u8(text + idx)) < 0) {
		rec->type = invalid_hex_error(text + idx);
		goto error;
	} else {
		idx += 2;
	}
	/* Make sure we've reached the end of the line/record: */
	if (idx < len) {
		switch (text[idx]) {
		case '\n':
			break;
		case '\r':
			++idx;
			if (idx >= len || text[idx] != '\n') {
				--idx;
				goto error_expected_eol;
			}
			break;
		default:
			if (rec->size < 255)
				goto error_invalid_size;
			else
				goto error_expected_eol;
		}
		++idx;
	}
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
		case IHRR_DATA:
		case IHRR_END_OF_FILE:
			break;
		case IHRR_EXT_SEG_ADDR:
		case IHRR_EXT_LIN_ADDR:
			rec->data.base_addr = ((IHR_U16)data[0] << 8)
				| (IHR_U16)data[1];
			break;
		case IHRR_START_SEG_ADDR:
			rec->data.start.code_seg = ((IHR_U16)data[0] << 8)
				| (IHR_U16)data[1];
			rec->data.start.instr_ptr = ((IHR_U16)data[2] << 8)
				| (IHR_U16)data[3];
			break;
		case IHRR_START_LIN_ADDR:
			rec->data.ext_instr_ptr = ((IHR_U32)data[0] << 24)
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

error_expected_eol:
	/* The record should have been ended by a line break already. */
	rec->type = -IHRE_EXPECTED_EOL;
	return ~idx;

error:
	/* Any other error. */
	return ~idx;
}
