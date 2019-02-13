#include "../test.h"
#include <string.h>

static size_t idx;
static const char *const lines[] = {
	":0B0010006164647265737320676170A7",
	":00000001FF\n",
	":10C20000E0A5E6F6FDFFE0AEE00FE6FCFDFFE6FD93\r\n",
	":10C21000FFFFF6F50EFE4B66F2FA0CFEF2F40EFE90\n\r",
	":10C22000F04EF05FF06CF07DCA0050C2F086F097DF\r\r\n",
	":00000001FF",
	":10C23000F04AF054BCF5204830592D02E018BB03F9",
	/* Invalid line endings: */
	":FF0000000000000000000000000000000000000000000000000000000000000000000"
		"00000000000000000000000000000000000000000000000000000000000000"
		"00000000000000000000000000000000000000000000000000000000000000"
		"00000000000000000000000000000000000000000000000000000000000000"
		"00000000000000000000000000000000000000000000000000000000000000"
		"00000000000000000000000000000000000000000000000000000000000000"
		"00000000000000000000000000000000000000000000000000000000000000"
		"00000000000000000000000000000000000000000000000000000000000000"
		"00000000000000001DEADBEEF\r\n",
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
	read_or_live(IHRT_I8, strlen(line), line, rec, idx, IHRE_EXPECTED_EOL);
}

int main(void)
{
	struct ihr_record rec;
	IHR_U8 buf[IHR_MAX_SIZE];
	do {
		rec.data.data = buf;
		next_line(&rec);
	} while (idx < 7);
	do {
		rec.data.data = buf;
		next_line_invalid_type(&rec);
	} while (idx < 8);
	rec.data.data = buf;
	ihr_read(IHRT_I8, strlen(lines[8]), lines[8], &rec);
	assert(rec.type == IHRR_I_END_OF_FILE);
}
