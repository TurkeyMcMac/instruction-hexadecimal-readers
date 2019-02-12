#include "../test.h"
#include <string.h>

static size_t idx;
static const char *const lines[] = {
	":10C",
	":",
	":::",
	"",
	":00000001F"
};
static void next_line(struct ihr_record *rec)
{
	const char *line = lines[idx];
	int len;
	++idx;
	read_or_live(IHRT_I8, strlen(line), line, rec, idx,
		IHRE_SUB_MIN_LENGTH);
}

int main(void)
{
	struct ihr_record rec;
	IHR_U8 buf[IHR_MAX_SIZE];
	do {
		rec.data.data = buf;
		next_line(&rec);
	} while (idx < sizeof(lines) / sizeof(*lines));
}
