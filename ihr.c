#include "ihr.h"

static int read_nibble(char hex)
{
	if ('0' <= hex && hex <= '9') return hex - '0';
	hex |= 0x20;
	if ('a' <= hex && hex <= 'f') return 10 + hex - 'a';
	return -1;
}

static int read_u8(const char hex[2])
{
	return (read_nibble(hex[0]) << 4) | read_nibble(hex[1]);
}

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

static int read_data(struct ihr_record *rec, const char *hex, size_t *idx)
{
	IHR_U8 i;
	int min_size;
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
		*idx = 1;
		return -1;
	}
	for (i = 0; i < rec->size; ++i) {
		const char *pair = hex + i * 2;
		int byte = read_u8(pair);
		if (byte < 0) {
			rec->type = invalid_hex_error(pair);
			*idx += i * 2;
			return -1;
		}
		rec->data.data[i] = byte;
	}
	*idx += i * 2;
	return 0;
}

static void unionize_data(struct ihr_record *rec)
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
			| ((IHR_U32)data[1] << 16) | ((IHR_U32)data[2] << 8)
			| (IHR_U32)data[3];
		break;
	}
}


static IHR_U8 calc_checksum(const struct ihr_record *rec)
{
	IHR_U8 checksum = 0;
	IHR_U8 i = 0;
	checksum += rec->size;
	checksum += rec->addr >> 8;
	checksum += rec->addr & 0xFF;
	checksum += rec->type;
	for (i = 0; i < rec->size; ++i) {
		checksum += rec->data.data[i];
	}
	checksum = ~checksum + 1;
	return checksum;
}

int ihr_read(int file_type,
	size_t len,
	const char *text,
	struct ihr_record *rec)
{
	size_t idx = 0;
	int size, addr, type, checksum;
	if (len < IHR_MIN_LENGTH) {
		rec->type = -IHRE_SUB_MIN_LENGTH;
		goto error;
	}
	if (text[idx] != ':') {
		rec->type = -IHRE_MISSING_COLON;
		goto error;
	}
	idx += 1;
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
	if (len < idx + (size + 1) * 2) goto error_invalid_size;
	if (read_data(rec, text + idx, &idx) < 0) goto error;
	checksum = read_u8(text + idx);
	if (checksum < 0) {
		rec->type = invalid_hex_error(text + idx);
		goto error;
	}
	if (checksum != calc_checksum(rec)) {
		rec->type = -IHRE_INVALID_CHECKSUM;
		goto error;
	}
	idx += 2;
	if (idx < len) {
		switch (text[idx]) {
		case '\n':
			break;
		case '\r':
			++idx;
			if (idx >= len || text[idx] != '\n') {
				--idx;
				rec->type = -IHRE_EXPECTED_EOL;
				goto error;
			}
			break;
		default:
			goto error_invalid_size;
		}
		++idx;
	}
	unionize_data(rec);
	return idx;

error_invalid_size:
	rec->type = -IHRE_INVALID_SIZE;
	return ~1;

error_not_hex:
	rec->type = -IHRE_NOT_HEX;
	return ~idx;

error:
	return ~idx;
}
