#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "err.h"

static char s_Context[512];
static int s_Code = 0;

#ifdef DEBUG
	static char *s_Location = 0;
#endif

int
#ifdef DEBUG
log_err(const error_e code, const char *location, const char *fmt, ...)
#else
log_err(const error_e code, const char *fmt, ...)
#endif
{
	va_list vl;
	va_start(vl, fmt);
	vsnprintf(s_Context, sizeof(s_Context), fmt, vl);
	va_end(vl);

	s_Code = code;

#ifdef DEBUG
	s_Location = location;
#endif

	return -1;
}

int
report_err()
{
	fprintf(stderr, "\033[1;31mError ocurred: \033[0;31m");

	switch(s_Code)
	{
		case OK:
			fprintf(stderr, "No error. \033[0m\n");
			return s_Code;
		case BAD_FORMAT:
			fprintf(stderr, "Bad file format");
			break;
		case NULL_POINTER:
			fprintf(stderr, "A NULL pointer was passed");
			break;
		case CANT_OPEN:
			fprintf(stderr, "Cant open (%s)", strerror(errno));
			break;
		case MALLOC:
			fprintf(stderr, "malloc() failed (%s)", strerror(errno));
			break;
		case NOMATCH:
			fprintf(stderr, "A search or pattern returned no match");
			break;
		default:
			fprintf(stderr, "Assuming errno code: %s", strerror(errno));
			break;
	}

	fprintf(stderr, ".\033[1;32m\n\tDescription:\033[0;32m %s. \033[0m\n", s_Context);
#ifdef DEBUG
	fprintf(stderr, "\033[1;34m\tAt %s\n", s_Location);
#endif

	return s_Code;
}

int
get_err()
{
	return s_Code;
}
