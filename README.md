# instruction-hexadecimal-readers
A small C library for reading Intel HEX, Motorola SREC, and possibly more in the
future.

## Formats
All these formats are for storing programs as text using hexidecimal bytes. You
can read all about them on Wikipedia:
 * [Intel HEX](https://en.wikipedia.org/wiki/Intel_HEX)
 * [Motorola SREC](https://en.wikipedia.org/wiki/SREC_(file_format))

## Example
```c
FILE *file = fopen("example.hex", "r");
char buf[IHR_MAX_SIZE];
int reclen;
struct ihr_record rec;
size_t size;
char *line;
do {
	line = fgetln(file, &size); /* This is valid until next call. */
	/* The buffer is filled if the record has type 0 (data): */
	rec.data.data = buf;
	/* Read the line as an I8HEX record: */
	if ((reclen = ihr_read(IHRT_I8, size, line, &rec)) < 0) {
		/* ~reclen is the column; -rec.type is the error code: */
		do_error(&rec, ~reclen);
	} else {
		use_record(&rec);
	}
} while (rec.type != IHRR_I_END_OF_FILE);
fclose(file);
```

## Usage
To use this library, you can probably just copy the header file into some header
directory and the source file into your source directory. It should compile with
ANSI C.

## API
There is only one function in the API, although it is somewhat complex:
```c
int ihr_read(
	int file_type,
	size_t len,
	const char *text,
	struct ihr_record *rec);
```
This function reads text-encoded data from `text`, which is assumed to be of
length `len`. No string terminator is necessary.

Before you call the function, you **must** set `rec->data.data` to a buffer at
least `IHR_MAX_SIZE` bytes in width (the maximum data entry length.) `rec`
points to a structure with this format:
```c
struct ihr_record {
	char type;
	IHR_U8 size;
	IHR_U32 addr;
	union {
		IHR_U8 *data;
			union {
			IHR_U16 base_addr;
			IHR_U32 ext_instr_ptr;
			struct {
				IHR_U16 code_seg;
				IHR_U16 instr_ptr;
			} start;
		} ihex;
	} data;
};
```
The types called things like `IHR_U8` are just unsigned integers of a certain
width (8 bits in this case.) You can read about the format of a record in the
Wikipedia link from the introduction, but here is a summary of the fields:
 * `type` is the record type. The possible values are as follows:
   * `IHRR_DATA` is Data.
   * `IHRR_END_OF_FILE` is End of File
   * `IHRR_EXT_SEG_ADDR` is Extended Segment Address.
   * `IHRR_START_SEG_ADDR` is Start Segment Address.
   * `IHRR_EXT_LIN_ADDR` is Extended Linear Address.
   * `IHRR_START_LIN_ADDR` is Start Linear Address.
 * `size` is the byte count.
 * `addr` is the address.
 * `data` is a bunch of bytes, the number of which is indicated by `size`. The
   members of its union:
   * `data` is just raw bytes. This field is associated with the Data record
     type. The library does no allocation, which is why you must set this to an
     empty buffer beforehand.
   * `ihex` contains options specific to Intel HEX:
     * `base_addr` is associated with either one of the Extended Segment Address
       or Extended Linear Address types. The meaning of the address is
       different, but the data can be stored the same way.
     * `ext_instr_ptr` corresponds with the Start Linear Address type.
     * `start` corresponds with the Start Segment Address type. Its `code_seg`
       field is the start of the code segment, while its `instr_ptr` field is
       the initial instruction pointer position.

The `file_type` field describes the subset of the format to use. The subset
restricts which field types are valid.
 * `IHRT_I8` is I8HEX
 * `IHRT_I16` is I32HEX
 * `IHRT_I32` is I32HEX
 * `IHRT_S19` is SREC S19
 * `IHRT_S28` is SREC S28
 * `IHRT_S37` is SREC S37

The return value of the function is positive on success or negative on error. If
it is positive, it indicates how much of the text was read as part of the
record. If it is negative, `~line` (not `-`, but `~`) indicates the erroneous
part of the text. On error, the `type` of the record is set to a negated error
value. No other data can be read from the record. Here are possible errors with
their descriptions:
 * `IHRE_EXPECTED_EOL`: Record does not end with `\n` or `\r\n`.
 * `IHRE_INVALID_CHECKSUM`: The checksum of the record did not match its
   computed checksum.
 * `IHRE_INVALID_SIZE`: The byte count of the record was not correct.
 * `IHRE_INVALID_TYPE`: The type of the record was not valid.
 * `IHRE_MISSING_COLON`: The `:` which must begin every record was not present.
 * `IHRE_NOT_HEX`: A pair of bytes could not be parsed as a hexidecimal number.
 * `IHRE_SUB_MIN_LENGTH`: The given `len` is below `IHR_MIN_LENGTH`, the minimum
   text-encoded record length.
