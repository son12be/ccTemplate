#pragma once

#include <errno.h>

#define STRERROR strerror(errno)

/* log err ECODE */
#ifdef DEBUG
#define STR(x) #x
#define STR_HELPER(x) STR(x)
#define CURPOS __FILE_NAME__ ":" STR_HELPER(__LINE__)

#define ERR_NR(ECODE, ...) log_err(ECODE, CURPOS, __VA_ARGS__)
#else
#define ERR_NR(ECODE, ...) log_err(ECODE, __VA_ARGS__)
#endif

/* log err ECODE and return -1 */
#define ERR(ECODE, ...) return ERR_NR(ECODE, __VA_ARGS__)

#define FAIL(EXPR)\
	if(EXPR)\
		return -1;
// #define FAIL_NULL(EXPR)\
// 	if(!(EXPR))\
// 		return -1;
#define FAIL_GOTO(EXPR, GOTO)\
	if(EXPR)\
		goto GOTO;
#define FAIL_CODE(EXPR, ECODE, ...)\
	if(EXPR)\
		ERR(ECODE, __VA_ARGS__)

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
#ifdef DEBUG
log_err(const error_e code, const char *location, const char *fmt, ...);
#else
log_err(const error_e code, const char *fmt, ...);
#endif

int
report_err();

int
get_err();
