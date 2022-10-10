#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#include "types.h"
#include "module.h"

#include "../string/module.h"
#include "../error/module.h"

static const void *global_format_user_ptr;

static inline void unmatched_delimiter_error [[gnu::noreturn]] (const char *fmt, char c) {
	fputs("[ERROR]: unmatched format delimiter `", stderr);
	fputc(c, stderr);
	fputs("` in format string:\n`", stderr);
	fputs(fmt, stderr);
	fputs("`\n", stderr);
	fflush(stderr);
	exit(1);
}

static inline void long_directive_error [[gnu::noreturn]] (const char *fmt)  {
	fputs("[ERROR]: format directive longer than 16 bytes in format string:\n`", stderr);
	fputs(fmt, stderr);
	fputs("`\n", stderr);
	fflush(stderr);
	exit(1);
}

Short_String parse_format_directive(const char *fmt, u64 *i) {
	Short_String directive = {0};

	// we should be parsing a format directive
	assert(fmt[*i] == '{');
	++*i;

	// skip leading spaces
	for ( ; fmt[*i] == ' ' ; ++*i);

	for (u64 j = 0; ; ++*i, ++j) {
		switch (fmt[*i]) {
			break;case '\0': { unmatched_delimiter_error(fmt, '{'); }

			// the directive is assumed to end on whitespace, ':' or '}'
			// the rest of the directive may contain some options, but we ignore
			// them for now.
			break;          case '}' :
			[[fallthrough]];case ':' :
			[[fallthrough]];case '\t':
			[[fallthrough]];case ' ' : { goto parse_to_end; }

			// copy regular chars into the directive, up to the max size.
			break;default: {
				if (j >= sizeof(Short_String)) { long_directive_error(fmt); }
				directive.begin[j] = fmt[*i];
			}
		}
	}

	parse_to_end:
	// skip any trailing chars up to the delimiter
	for (; fmt[*i] != '}' && fmt[*i] != '\0'; ++*i);
	if (fmt[*i] == '\0') { unmatched_delimiter_error(fmt, '{'); }

	return directive;
}

i64 handle_format(
	void*dest,
	void*context,
	const char *fmt,
	i64 handle_string(void*dest, void* context, String view),
	i64 handle_directive(void*dest, void*context, Short_String dir, va_list va),
	va_list va
) {
	u64 count = 0;
	u64 i     = 0;
	u64 base  = 0;
	for (;;) {
		// these chars trigger flushing the pending chars:
		// '\0'     - because we will be returning from the function
		// '{', '}' - because they are doubled or initiate a format directive,
		//            so we can't process the chars across them.
		if (i != base && (fmt[i] == '\0' || fmt[i] == '{' || fmt[i] == '}')) {
			count += handle_string(dest, context, cstring_unsafe_slice(fmt, base, i));
		}

		switch(fmt[i]) {
			break;case '\0': { return count; }

			break;case '{': {
				if (fmt[i+1] != '{') {
					Short_String directive = parse_format_directive(fmt, &i);
					count += handle_directive(dest, context, directive, va);
				} else {
					++i;
				}
				++i;
				base = i;
			}

			break;case '}': {
				if (fmt[++i] != '}') { unmatched_delimiter_error(fmt, '}'); }
				++i;
				base = i;
			}

			break;default: { ++i; }
		}
	}
}

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

String format_to_va(Byte_Slice dest, cstring fmt, va_list params) {
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
	return (String){.begin = dest.begin, .end = remaining.begin};
}
i64 format_print_va(FILE *dest, cstring fmt, va_list params) {
	Fmt_Info info = {0};
	return handle_format(
		dest,
		&info,
		fmt,
		print_string_to_stream,
		format_directive_to_stream,
		params
	);
}

void print(String s) {
	fwrite(s.begin, 1, s.end - s.begin, stdout);
}

void println(String s) {
	print(s);
	fputc('\n', stdout);
}

// TODO: factor out common behaviour
String format(cstring fmt, ...) {
	va_list params;
	va_start(params, fmt);
	String result = format_to_va(default_format_buffer, fmt, params);
	va_end(params);
	return result;
}

void format_fprint(FILE *stream, cstring fmt, ...) {
	va_list params;
	va_start(params, fmt);
	format_print_va(stream, fmt, params);
	va_end(params);
}

void format_fprintln(FILE *stream, cstring fmt, ...) {
	va_list params;
	va_start(params, fmt);
	format_print_va(stream, fmt, params);
	va_end(params);
	fputc('\n', stream);
}

void format_print(cstring fmt, ...) {
	va_list params;
	va_start(params, fmt);
	format_print_va(stdout, fmt, params);
	va_end(params);
}

void format_println(cstring fmt, ...) {
	va_list params;
	va_start(params, fmt);
	format_print_va(stdout, fmt, params);
	va_end(params);
	fputc('\n', stdout);
}

char default_format_buffer_data[4096];
Byte_Slice default_format_buffer = {
	default_format_buffer_data,
	default_format_buffer_data + sizeof(default_format_buffer_data)
};

void set_format_user_ptr(const void* user_ptr) {
	global_format_user_ptr = user_ptr;
}

String format_to(Byte_Slice dest, cstring fmt, ...) {
	va_list params;
	va_start(params, fmt);
	String result = format_to_va(dest, fmt, params);
	va_end(params);
	return result;
}
