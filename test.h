#include "ihr.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static const char *errstr(int code)
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
	case IHRE_SUB_MIN_LENGTH:
		return "Record text below minimum possible size";
	default:
		return "Uknown error";
	}
}

static int read_or_die(int type,
	size_t line_len,
	const char *text,
	struct ihr_record *rec,
	int line)
{
	int len = ihr_read(type, line_len, text, rec);
	if (len < 0) {
		fprintf(stderr, "ERROR (line %d, column %d): %s.\n",
			line, -len, errstr(rec->type));
		exit(EXIT_FAILURE);
	}
	return len;
}

static int read_or_live(int type,
	size_t line_len,
	const char *text,
	struct ihr_record *rec,
	int line,
	int expected_err)
{
	int len = ihr_read(type, line_len, text, rec);
	if (rec->type != -expected_err) {
		fprintf(stderr, "NOT EXPECTED %s (line %d, column %d): ",
			errstr(expected_err), line, -len);
		if (rec->type > 0)
			fprintf(stderr, "%d\n", rec->type);
		else
			fprintf(stderr, "%s\n", errstr(rec->type));
		exit(EXIT_FAILURE);
	}
	return len;
}
