#include "../test.h"
#include <string.h>

static size_t idx;
static const char *const lines[] = {
	":0B0010006164647265737320676170A7",
	":00000001FF",
	":10C21000FFFFF6F50EFE4B66F2FA0CFEF2F40EFE90",
	":10C22000F04EF05FF06CF07DCA0050C2F086F097DF",
	":00000001FF",
	":10C23000F04AF054BCF5204830592D02E018BB03F9",
	/* Not invalid byte count; missing end of line: */
	":FF0000000000000000000000000000000000000000000000000000000000000000000"
		"00000000000000000000000000000000000000000000000000000000000000"
		"00000000000000000000000000000000000000000000000000000000000000"
		"00000000000000000000000000000000000000000000000000000000000000"
		"00000000000000000000000000000000000000000000000000000000000000"
		"00000000000000000000000000000000000000000000000000000000000000"
		"00000000000000000000000000000000000000000000000000000000000000"
		"00000000000000000000000000000000000000000000000000000000000000"
		"00000000000000001DEADBEEF",
	/* Invalid size: */
	":11C22000F04EF05FF06CF07DCA0050C2F086F097DF",
	":FF000001FF",
	":00C23000F04AF054BCF5204830592D02E018BB03F9",
	/* EOF: */
	":00000001FF"
};
static void next_line(struct ihr_record *rec)
{
	const char *line = lines[idx];
	int len;
	++idx;
	read_or_die(IHRT_I8, strlen(line), line, rec, idx);
}
static void next_line_invalid_type(struct ihr_record *rec)
{
	const char *line = lines[idx];
	int len;
	++idx;
	read_or_live(IHRT_I8, strlen(line), line, rec, idx, IHRE_INVALID_SIZE);
}

int main(void)
{
	struct ihr_record rec;
	IHR_U8 buf[IHR_MAX_SIZE];
	do {
		rec.data.data = buf;
		next_line(&rec);
	} while (idx < 6);
	rec.data.data = buf;
	read_or_live(IHRT_I8, strlen(lines[6]), lines[6], &rec, 6,
		IHRE_EXPECTED_EOL);
	++idx;
	do {
		rec.data.data = buf;
		next_line_invalid_type(&rec);
	} while (idx < 10);
	rec.data.data = buf;
	ihr_read(IHRT_I8, strlen(lines[10]), lines[10], &rec);
	assert(rec.type == IHRR_END_OF_FILE);
}
