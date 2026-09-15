#pragma once

#include <stdio.h>

#include "misc.h"

#define streq(A, B) (strcmp(A, B) == 0)

enum logLevel_e
{
	LOG_ERR = 0,
	LOG_WARN = 1,
	LOG_DEBUG = 2,
};

struct data_t
{
	char cc[VALUE_SIZE];
	char srcdirs[VALUE_SIZE];
	char builddir[SMALL_VALUE_SIZE];
	char name[SMALL_VALUE_SIZE];
	char ext[SMALLER_VALUE_SIZE];
	char *label;
	FILE *flagFile;
	unsigned int threads;
};

int
strends(const char *A, const char *B);

int
parse_template(struct data_t *data);
