#include "../test.h"
#include <string.h>

static size_t idx;
static const char *const lines[] = {
	"S00F000068656C6C6F202020202000003C",
	"S11F00007C0802A6900100049421FFF07C6C1B787C8C23783C6000003863000026",
	"S11F001C4BFFFFE5398000007D83637880010014382100107C0803A64E800020E9",
	"S111003848656C6C6F20776F726C642E0A0042",
	"S5030003F9",
	/* Invalid types for S19: */
	"S2030000FC",
	"S3030000FC",
	"S411003848656C6C6F20776F726C642E0A0042",
	/* EOF: */
	"S9030000FC"
};
static void next_line(struct ihr_record *rec)
{
	const char *line = lines[idx];
	int len;
	++idx;
	read_or_die(IHRT_S19, strlen(line), line, rec, idx);
}
static void next_line_invalid_type(struct ihr_record *rec)
{
	const char *line = lines[idx];
	int len;
	++idx;
	read_or_live(IHRT_S19, strlen(line), line, rec, idx, IHRE_INVALID_TYPE);
}

int main(void)
{
	struct ihr_record rec;
	IHR_U8 buf[IHR_MAX_SIZE];
	do {
		rec.data.data = buf;
		next_line(&rec);
	} while (idx < 5);
	do {
		rec.data.data = buf;
		next_line_invalid_type(&rec);
	} while (idx < 8);
	rec.data.data = buf;
	ihr_read(IHRT_S19, strlen(lines[8]), lines[8], &rec);
	assert(rec.type == IHRR_S9_START_16);
	assert(rec.data.srec.start == 0);
}
