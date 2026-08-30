#pragma once

#include <stdio.h>

#define streq(A, B) (strcmp(A, B) == 0)

enum logLevel_e
{
	LOG_ERR = 0,
	LOG_WARN = 1,
	LOG_DEBUG = 2,
};

struct data_t
{
	char *srcdirs;
	char *builddir;
	char *ext;
	char *cc;
	unsigned int threads;
	FILE *flagFile;
};

int
strends(const char *A, const char *B);

int
parse_template(struct data_t *data, const char *templatePath);
