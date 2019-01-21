#include "ihr.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

const char *errstr(int code);

int read_or_die(int type,
	size_t line_len,
	const char *text,
	struct ihr_record *rec,
	int line);

int read_or_live(int type,
	size_t line_len,
	const char *text,
	struct ihr_record *rec,
	int line,
	int expected_err);
