#pragma once

#include <stdio.h>

#include "misc.h"

#define streq(A, B) (strcmp(A, B) == 0)

struct data_t
{
	char srcdirs[VALUE_SIZE];
	char incdirs[VALUE_SIZE];
	char builddir[VALUE_SIZE / 2];
	char ext[SMALLER_VALUE_SIZE];
	char cc[SMALL_VALUE_SIZE];
	char objFlag[SMALLER_VALUE_SIZE];
	unsigned int threads;
	FILE *flagFile;
};

int
strends(const char *A, const char *B);
