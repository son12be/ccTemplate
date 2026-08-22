#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>

#include "util.h"

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

void
parse_template(struct data_t *data, const char *templatePath)
{
	const FILE *template = fopen(templatePath, "r");
	if(!template)
	{
		fprintf(stderr, "Cant open \"%s\": %s \n", templatePath, strerror(errno));
		exit(-1);
	}

	struct configPair_t configPairs[] =
	{
		{ "SRCDIR", &(data->srcdirs) },
		{ "INCDIR", &data->incdirs },
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
			fprintf(stderr, "WARNING: \"%s\" has no value assigned to it \n", line);
			continue;
		}

		*(value - 1) = '\0';

		/* handle special cases */
		if(streq(line, "FLAGFILE"))
		{
			data->flagFile = fopen(value, "r");
			if(!data->flagFile)
				fprintf(stderr, "Cant open flagfile \"%s\": %s \n", value, strerror(errno));

		} else
		{
			for(int i = 0; i < sizeof(configPairs) / sizeof(struct configPair_t); ++i)
			{
				if(streq(line, configPairs[i].key))
				{
					*((char**)configPairs[i].value) = malloc(strlen(value) + 1);
					memcpy(*((char**)configPairs[i].value), value, strlen(value) + 1);
				}
				else
					continue;

				if(!configPairs[i].value)
					fprintf(stderr, "Error assigning \"%s\" to \"%s\": %s \n", line, value, strerror(errno));
			}
		}
	}
}
