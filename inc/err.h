#pragma once

#include <errno.h>

#define STRERROR strerror(errno)

#define STR(x) #x
#define STR_HELPER(x) STR(x)
#define CURPOS __FILE_NAME__ ":" STR_HELPER(__LINE__)

typedef enum
{
	OK = 0,
	BAD_FORMAT,
	NULL_POINTER,
	CANT_OPEN,
	MALLOC,
} error_e;

int
log_err(const error_e code, const char *location, const char *fmt, ...);

int
report_err();

int
get_err();
