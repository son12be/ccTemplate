#pragma once

#include <stdio.h>

#include "misc.h"

#define streq(A, B) (strcmp(A, B) == 0)

struct data_t
{
	char *srcdirs;
	char *incdirs;
	char *builddir;
	char *ext;
	char *cc;
	char *objFlag;
	unsigned int threads;
	FILE *flagFile;
};

int
strends(const char *A, const char *B);

void
parse_template(struct data_t *data, const char *templatePath);
