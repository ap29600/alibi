#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#include "types.h"
#include "module.h"
#include "generic.h"

static const void *global_format_user_ptr;

i64 format_directive_to_bytes(void*dest, [[maybe_unused]] void* info, Short_String dir, va_list va) {
	Byte_Slice *d = dest;
	fmt_procedure_t proc = lookup_format_directive(dir);

	if (!proc) {
		fputs("[ERROR]: unknown format directive `", stderr);
		fputs(dir.begin, stderr);
		fputs("`", stderr);
		exit(1);
	}

	u64 result = proc(*d, va, *(Fmt_Info*)info);
	d->begin += result;
	return result;
}

i64 format_directive_to_stream(void*dest,[[maybe_unused]] void* info,  Short_String dir, va_list va) {
	Byte_Slice d = default_format_buffer;
	const char *begin = d.begin;
	u64 count = format_directive_to_bytes(&d, info, dir, va);
	return fwrite(begin, 1, count, dest);
}

i64 print_string_to_stream(void*dest, [[maybe_unused]] void* info, String view) {
	return fwrite(view.begin, 1, view.end - view.begin, dest);
}

i64 print_string_to_bytes(void*dest, [[maybe_unused]] void* info, String view) {
	Byte_Slice *d = dest;
	char *const begin = d->begin;
	d->begin = strncpy(d->begin, view.begin, min(string_len(view), d->end - d->begin));
	return d->begin - begin;
}

String format_to_(Byte_Slice dest, bool newline, cstring fmt, ...) {
	va_list params;
	va_start(params, fmt);

	Byte_Slice remaining = dest;
	Fmt_Info info = {0};
	handle_format(
		&remaining,
		&info,
		fmt,
		print_string_to_bytes,
		format_directive_to_bytes,
		params
	);

	va_end(params);
	if (newline && remaining.begin < remaining.end) { *remaining.begin++ = '\n'; }
	return (String){.begin = dest.begin, .end = remaining.begin};
}


i64 format_fprint_(FILE *dest, bool newline, cstring fmt, ...) {
	va_list params;
	va_start(params, fmt);

	Fmt_Info info = {0};
	i64 result = handle_format(
		dest,
		&info,
		fmt,
		print_string_to_stream,
		format_directive_to_stream,
		params
	);

	va_end(params);
	if (newline) { result += fputc('\n', dest); }
	return result;
}


i64 print(String s) {
	return fwrite(s.begin, 1, s.end - s.begin, stdout);
}

i64  println(String s) {
	i64 result = print(s);
	result += fputc('\n', stdout);
	return result;
}

char default_format_buffer_data[4096];
Byte_Slice default_format_buffer = {
	default_format_buffer_data,
	default_format_buffer_data + sizeof(default_format_buffer_data)
};

void set_format_user_ptr(const void* user_ptr) {
	global_format_user_ptr = user_ptr;
}

