#include "../test.h"
#include <string.h>

static size_t idx;
static const char *const lines[] = {
	":020000021000EC",
	":0400000300003800C1",
	":10C20000E0A5E6F6FDFFE0AEE00FE6FCFDFFE6FD93",
	":10C21000FFFFF6F50EFE4B66F2FA0CFEF2F40EFE90",
	":10C22000F04EF05FF06CF07DCA0050C2F086F097DF",
	":10C23000F04AF054BCF5204830592D02E018BB03F9",
	":020000020000FC",
	":04000000FA00000200",
	":02000004FFFFFC", /* Invalid type for I16. */
	":04000005000000CD2A", /* Ditto */
	":00000001FF"
};
static void next_line(struct ihr_record *rec)
{
	const char *line = lines[idx];
	int len;
	++idx;
	read_or_die(IHRT_I16, strlen(line), line, rec, idx);
}
static void next_line_invalid_type(struct ihr_record *rec)
{
	const char *line = lines[idx];
	int len;
	++idx;
	read_or_live(IHRT_I16, strlen(line), line, rec, idx,
		IHRE_INVALID_TYPE);
}

int main(void)
{
	struct ihr_record rec;
	IHR_U8 buf[255];
	do {
		rec.data.data = buf;
		next_line(&rec);
	} while (idx < 8);
	do {
		rec.data.data = buf;
		next_line_invalid_type(&rec);
	} while (idx < 10);
	ihr_read(IHRT_I16, strlen(lines[10]), lines[10], &rec);
	assert(rec.type == IHRR_I_END_OF_FILE);
}
