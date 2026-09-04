#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "err.h"

static char s_Context[512];
static char *s_Location = 0;
static int s_Code = 0;

int
log_err(const error_e code, const char *location, const char *fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);

	vsnprintf(s_Context, sizeof(s_Context), fmt, vl);

	va_end(vl);

	s_Location = location;

	s_Code = code;

	return -1;
}

int
report_err()
{
	fprintf(stderr, "\033[1;31m");

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
			fprintf(stderr, "Cant open (%s)", strerror(s_Code));
			break;
		case MALLOC:
			fprintf(stderr, "malloc() failed (%s)", strerror(s_Code));
			break;
		default:
			fprintf(stderr, "Assuming errno code: %s", strerror(-s_Code));
			break;
	}
	fprintf(stderr, ".\033[1;32m Hint: %s.\033[1;34m At %s \033[0m\n", s_Context, s_Location);

	return s_Code;
}

int
get_err()
{
	return s_Code;
}
