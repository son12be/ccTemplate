#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "util.h"
#include "misc.h"

struct configPair_t
{
	const char *key;
	void *value;
};

int
strends(const char *A, const char *B)
{
	if(!A || !B)
		return 0;

	char *c = strrchr(A, B[0]);
	return c && strcmp(c, B) == 0 ? 1 : 0;
}

int
parse_template(struct data_t *data, const char *templatePath)
{
	const FILE *template = fopen(templatePath, "r");
	if(!template)
	{
		print_err(LOG_ERR, "Cant open \"%s\": %s \n", templatePath, STRERROR);
		return -1;
	}

	struct configPair_t configPairs[] =
	{
		{ "SRCDIR", &(data->srcdirs) },
		{ "BUILDDIR", &data->builddir },
		{ "EXT", &data->ext },
		{ "CC", &data->cc },
		{ "OFLAG", &data->objFlag},
	};

	char line[VALUE_SIZE];
	while((fgets(line, sizeof(line), template)) != NULL)
	{
		/* TODO clean this up */
		/* ignore lines starting w/ '#' or a whitespace */
		if(line[0] == '#' || isspace((unsigned char)line[0]))
			continue;

		line[strcspn(line, "\n")] = '\0';

		/* find the whitespace between key and value */
		char *value = strchr(line, ' ');
		if(!value || *(++value) == '\0')
		{
			print_err(LOG_WARN, "\"%s\" has no value assigned to it \n", line);
			continue;
		}

		*(value - 1) = '\0';

		/* handle special cases */
		if(streq(line, "FLAGFILE"))
		{
			data->flagFile = fopen(value, "r");
			if(!data->flagFile)
			{
				print_err(LOG_ERR, "Cant open flagfile \"%s\": %s \n", value, STRERROR);
				return -1;
			}

		} else
		{
			for(int i = 0; i < sizeof(configPairs) / sizeof(struct configPair_t); ++i)
			{
				if(!streq(line, configPairs[i].key))
					continue;

				*((char**)configPairs[i].value) = malloc(strlen(value) + 1);
				memcpy(*((char**)configPairs[i].value), value, strlen(value) + 1);
			}
		}
	}

	return 0;
}

void
print_err(const enum logLevel_e logLevel, const char *fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);

	const char *logLevels[] = { "ERR", "WARN", "DEBUG" };
	fprintf(stderr, "%s: ", logLevels[logLevel]);
	vfprintf(stderr, fmt, vl);

	va_end(vl);
}
