#include "../test.h"
#include <string.h>

static size_t idx;
static const struct {
	int not_hex;
	const char *str;
} lines[] = {
	{ 3, ":0B 010006164647265737320676170A7"},
	{27, ":10C21000FFFFF6F50EFE4B66FF_A0CFEF2F40EFE90"},
	{19, ":10C22000F04EF05FF06]F07DCA0050C2F086F097DF"},
	{ 7, ":0000000[FF"},
	{13, ":10C23000F04AF]54BCF5204830592D02E018BB03F9"},
	{ 1, ": 0000001FF"},
	{41, ":10C20000E0A5E6F6FDFFE0AEE00FE6FCFDFFE6FD9\01"}
};
static void next_line(struct ihr_record *rec)
{
	const char *line = lines[idx].str;
	int not_hex = lines[idx].not_hex;
	int len;
	++idx;
	len = ~read_or_live(IHRT_I8, strlen(line), line, rec, idx,
		IHRE_NOT_HEX);
	if (len != not_hex) {
		fprintf(stderr, "line %lu: Expected %d, got %d\n",
			idx, not_hex, len);
		exit(EXIT_FAILURE);
	}
}

int main(void)
{
	struct ihr_record rec;
	IHR_U8 buf[255];
	do {
		rec.data.data = buf;
		next_line(&rec);
	} while (idx < sizeof(lines) / sizeof(*lines));
}
