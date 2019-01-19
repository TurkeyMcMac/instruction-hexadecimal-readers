#include "hex.h"
#include <ctype.h>

static int read_nibble(char hex)
{
	int d = digittoint(hex);
	if (d == 0 && hex != '0') d = -1;
	return d;
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

static int read_data(struct ihr_record *rec, const char *hex)
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
	if ((int)rec->size < min_size) return -IHRE_INVALID_SIZE;
	for (i = 0; i < rec->size; ++i) {
		const char *pair = hex + i * 2;
		int byte = read_u8(pair);
		if (byte < 0) {
			return invalid_hex_error(pair);
		}
		rec->data.data[i] = byte;
	}
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

#define READ(from, size, buf, on_eof) \
	if (fread((buf), 1, (size), (from)) != (size)) { \
		if (feof((from))) { \
			on_eof; \
		} else { \
			return -IHRE_IO_ERROR; \
		} \
	}

int ihr_read(int file_type, FILE *from, struct ihr_record *rec)
{
	int err;
	char buf[512];
	int size, addr, type, checksum;
	READ(from, 1, buf, return -IHRE_UNEXPECTED_EOF);
	if (buf[0] != ':') return -IHRE_MISSING_COLON;
	READ(from, 2, buf, return -IHRE_UNEXPECTED_EOF);
	size = read_u8(buf);
	rec->size = size;
	if (size < 0) return -IHRE_NOT_HEX;
	READ(from, 4, buf, return -IHRE_UNEXPECTED_EOF);
	addr = read_u8(buf);
	if (addr < 0) return -IHRE_NOT_HEX;
	rec->addr = addr << 8;
	addr = read_u8(buf + 2);
	if (addr < 0) return -IHRE_NOT_HEX;
	rec->addr |= addr;
	READ(from, 2, buf, return -IHRE_UNEXPECTED_EOF);
	type = read_u8(buf);
	rec->type = type;
	if (type < 0) return -IHRE_NOT_HEX;
	if (!valid_type(file_type, type)) return -IHRE_INVALID_TYPE;
	READ(from, size * 2, buf, return -IHRE_INVALID_SIZE);
	err = read_data(rec, buf);
	if (err < 0) return err;
	READ(from, 2, buf, return -IHRE_UNEXPECTED_EOF);
	checksum = read_u8(buf);
	if (checksum < 0) return invalid_hex_error(buf);
	READ(from, 1, buf,
		if (rec->type == IHRR_END_OF_FILE) goto finish;
		return -IHRE_UNEXPECTED_EOF;
	);
	switch (buf[0]) {
	case '\n':
		break;
	case '\r':
		READ(from, 1, buf, return -IHRE_EXPECTED_EOL);
		if (buf[0] != '\n') {
			return -IHRE_EXPECTED_EOL;
		}
		break;
	default:
		return -IHRE_INVALID_SIZE;
	}
	if (fread(buf, 1, 1, from) != 1) {
		if (feof(from)) {
			if (rec->type != IHRR_END_OF_FILE) {
				return -IHRE_UNEXPECTED_EOF;
			}
		} else {
			return -IHRE_IO_ERROR;
		}
	} else if (rec->type == IHRR_END_OF_FILE) {
		return -IHRE_MISSING_EOF;
	} else {
		ungetc(buf[0], from);
	}
finish:
	if (checksum != calc_checksum(rec)) return -IHRE_INVALID_CHECKSUM;
	unionize_data(rec);
	return 0;
}

const char *ihr_errstr(int code)
{
	if (code < 0) code *= -1;
	switch (code) {
	case 0:
		return "Success";
	case IHRE_EXPECTED_EOL:
		return "Expected line ending";
	case IHRE_INVALID_CHECKSUM:
		return "Stored checksum does not match computed checksum";
	case IHRE_INVALID_SIZE:
		return "Invalid byte count for record";
	case IHRE_INVALID_TYPE:
		return "Invalid record type";
	case IHRE_MISSING_COLON:
		return "Expected ':' to begin a record";
	case IHRE_MISSING_EOF:
		return "Expected end-of-file";
	case IHRE_NOT_HEX:
		return "Character pair is not a hexidecimal digit pair";
	case IHRE_UNEXPECTED_EOF:
		return "Unexpected end-of-file";
	case IHRE_IO_ERROR:
		return "I/O error";
	default:
		return "Uknown error";
	}
}
