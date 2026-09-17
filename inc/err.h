#pragma once

#include <errno.h>

#define STRERROR strerror(errno)

/* log err ECODE */
#define ERR_NR(ECODE, ...) log_err(ECODE, CURPOS, __VA_ARGS__)

/* log err ECODE and return -1 */
#define ERR(ECODE, ...) return ERR_NR(ECODE, __VA_ARGS__)

#define FAIL(FUNC)\
	if(FUNC < 0)\
		return -1;
#define FAIL_GOTO(FUNC, GOTO)\
	if(FUNC < 0)\
		goto GOTO;
#define FAIL_CODE(FUNC, ECODE, ...)\
	if(FUNC < 0)\
		ERR(ECODE, __VA_ARGS__)

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
	NOMATCH,
} error_e;

int
log_err(const error_e code, const char *location, const char *fmt, ...);

int
report_err();

int
get_err();
