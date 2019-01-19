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

static int read_u16(const char hex[4])
{
	return (read_u8(hex) << 8) | read_u8(hex + 2);
}

static int valid_type(const struct hex_reader *reader, HEX_U8 type)
{
	return type <= 5;
}

static int invalid_hex_error(const char *pair)
{
	switch (pair[0]) {
	case '\n':
		return -HEXE_INVALID_SIZE;
	case '\r':
		if (pair[1] == '\n') return -HEXE_INVALID_SIZE;
		/* FALLTHROUGH */
	default:
		return -HEXE_NOT_HEX;
	}
}

static int read_data(struct hex_record *rec, const char *hex)
{
	HEX_U8 i;
	int min_size;
	switch (rec->type) {
		case HEXR_EXT_SEG_ADDR:
		case HEXR_EXT_LIN_ADDR:
			min_size = 2;
			break;
		case HEXR_START_SEG_ADDR:
		case HEXR_START_LIN_ADDR:
			min_size = 4;
			break;
		default:
			min_size = -1;
	}
	if ((int)rec->size < min_size) return -HEXE_INVALID_SIZE;
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

static void unionize_data(struct hex_record *rec)
{
	HEX_U8 *data = rec->data.data;
	switch (rec->type) {
	case HEXR_DATA:
	case HEXR_END_OF_FILE:
		break;
	case HEXR_EXT_SEG_ADDR:
	case HEXR_EXT_LIN_ADDR:
		rec->data.base_addr = ((HEX_U16)data[0] << 8)
			| (HEX_U16)data[1];
		break;
	case HEXR_START_SEG_ADDR:
		rec->data.start.code_seg = ((HEX_U16)data[0] << 8)
			| (HEX_U16)data[1];
		rec->data.start.instr_ptr = ((HEX_U16)data[2] << 8)
			| (HEX_U16)data[3];
		break;
	case HEXR_START_LIN_ADDR:
		rec->data.ext_instr_ptr = ((HEX_U32)data[0] << 24)
			| ((HEX_U32)data[1] << 16) | ((HEX_U32)data[2] << 8)
			| (HEX_U32)data[3];
		break;
	}
}


static HEX_U8 calc_checksum(const struct hex_record *rec)
{
	HEX_U8 checksum = 0;
	HEX_U8 i = 0;
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
	if (fread((buf), 1, (size), (from)->source) != (size)) { \
		if (feof((from)->source)) { \
			on_eof; \
		} else { \
			return -HEXE_IO_ERROR; \
		} \
	}

int is_at_end(FILE *file)
{
	long pos, end;
	if (feof(file)) return 1;
	pos = ftell(file);
	fseek(file, 0, SEEK_END);
	end = ftell(file);
	if (pos == end) {
		return 1;
	} else {
		fseek(file, pos, SEEK_SET);
		return 0;
	}
}

int hex_read(struct hex_reader *from, struct hex_record *rec)
{
	int err;
	char buf[512];
	int size, addr, type, checksum;
	READ(from, 1, buf, return -HEXE_UNEXPECTED_EOF);
	if (buf[0] != ':') return -HEXE_MISSING_COLON;
	READ(from, 2, buf, return -HEXE_UNEXPECTED_EOF);
	size = read_u8(buf);
	rec->size = size;
	if (size < 0) return -HEXE_NOT_HEX;
	READ(from, 4, buf, return -HEXE_UNEXPECTED_EOF);
	addr = read_u16(buf);
	rec->addr = addr;
	if (addr < 0) return -HEXE_NOT_HEX;
	READ(from, 2, buf, return -HEXE_UNEXPECTED_EOF);
	type = read_u8(buf);
	rec->type = type;
	if (type < 0) return -HEXE_NOT_HEX;
	if (!valid_type(from, type)) return -HEXE_INVALID_TYPE;
	READ(from, size * 2, buf, return -HEXE_INVALID_SIZE);
	err = read_data(rec, buf);
	if (err < 0) return err;
	READ(from, 2, buf, return -HEXE_UNEXPECTED_EOF);
	checksum = read_u8(buf);
	if (checksum < 0) return invalid_hex_error(buf);
	if (checksum != calc_checksum(rec)) return -HEXE_INVALID_CHECKSUM;
	READ(from, 1, buf, return
		rec->type == HEXR_END_OF_FILE ? 0 : -HEXE_UNEXPECTED_EOF);
	switch (buf[0]) {
	case '\n':
		break;
	case '\r':
		READ(from, 1, buf, return -HEXE_EXPECTED_EOL);
		if (buf[0] != '\n') {
			return -HEXE_EXPECTED_EOL;
		}
		break;
	default:
		return -HEXE_EXPECTED_EOL;
	}
	++from->line;
	if (is_at_end(from->source)) {
		if (rec->type != HEXR_END_OF_FILE) {
			return -HEXE_UNEXPECTED_EOF;
		}
	} else if (rec->type == HEXR_END_OF_FILE) {
		return -HEXE_MISSING_EOF;
	}
	unionize_data(rec);
	return 0;
}

int hex_write(FILE *into, const struct hex_record *rec)
{
	return 0;
}

const char *hex_errstr(int code)
{
	if (code < 0) code *= -1;
	switch (code) {
	case 0:
		return "Success";
	case HEXE_EXPECTED_EOL:
		return "Expected line ending";
	case HEXE_INVALID_CHECKSUM:
		return "Stored checksum does not match computed checksum";
	case HEXE_INVALID_SIZE:
		return "Invalid byte count for record";
	case HEXE_INVALID_TYPE:
		return "Invalid record type";
	case HEXE_MISSING_COLON:
		return "Expected ':' to begin a record";
	case HEXE_MISSING_EOF:
		return "Expected end-of-file";
	case HEXE_NOT_HEX:
		return "Character is not a hexidecimal digit";
	case HEXE_UNEXPECTED_EOF:
		return "Unexpected end-of-file";
	case HEXE_IO_ERROR:
		return "I/O error";
	default:
		return "Uknown error";
	}
}
