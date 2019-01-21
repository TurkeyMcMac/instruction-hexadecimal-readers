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
	":00000001FF"
};
static void next_line(struct ihr_record *rec)
{
	const char *line = lines[idx];
	int len;
	++idx;
	read_or_die(IHRT_I16, strlen(line), line, rec, idx);
}

int main(void)
{
	struct ihr_record rec;
	IHR_U8 buf[255];
	do {
		rec.data.data = buf;
		next_line(&rec);
	} while (rec.type != IHRR_END_OF_FILE);
}
