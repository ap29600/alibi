#ifndef FZR_FORMAT_GENERIC_H
#define FZR_FORMAT_GENERIC_H

#include <stdio.h>
#include <stdlib.h>
#include "types.h"

#include "../string/module.h"

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

static Short_String parse_format_directive(const char *fmt, u64 *i) {
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

static i64 handle_format(
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

#endif // FZR_FORMAT_GENERIC_H
