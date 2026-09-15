#include <string.h>
#include <sys/param.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

#include "util.h"
#include "misc.h"
#include "err.h"

struct tuple_t
{
	const char *key;
	void *value;
	uint16_t maxSz;
};

int
strends(const char *A, const char *B)
{
	if(!A || !B)
		return 0;

	char *c = strrchr(A, B[0]);
	return c && (strcmp(c, B) == 0);
}

static char *
trim_ws(const char **s)
{
	while(isspace((unsigned char)**s))
		(*s)++;

	char *end = strchr(*s, '\0');
	end--;
	while(end > *s && isspace((unsigned char)*end))
	{
		*end = '\0';
		end--;
	}

	return *s;
}

int
parse_template(struct data_t *data)
{
	const FILE *template = fopen(TEMPLATE_FILENAME, "r");
	if(!template)
		ERR(CANT_OPEN, "Template file (%s)", TEMPLATE_FILENAME);

	struct tuple_t confTuples[] =
	{
		{ "SRCDIRS", data->srcdirs, sizeof(data->srcdirs) },
		{ "BUILDDIR", data->builddir, sizeof(data->builddir) },
		{ "EXT", data->ext, sizeof(data->ext) },
		{ "CC", data->cc, sizeof(data->cc) },
		{ "NAME", data->name, sizeof(data->cc) },
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
			fclose(template);
			ERR(BAD_FORMAT, "Tempate file. At line \"%s\"", line);
		}

		*(value - 1) = '\0';

		/* handle special cases */
		if(streq(line, "FLAGFILE"))
		{
			data->flagFile = fopen(value, "r");
			if(!data->flagFile)
			{
				fclose(template);
				ERR(CANT_OPEN,  value);
			}

		} else
		{
			for(unsigned int i = 0; i < sizeof(confTuples) / sizeof(struct tuple_t); ++i)
			{
				if(!streq(line, confTuples[i].key))
					continue;

				trim_ws(&value);

				/* This line makes me want to use C++ */
				snprintf(confTuples[i].value, confTuples[i].maxSz, "%s", value);
			}
		}
	}
	
	fclose(template);

	if(!(data->srcdirs[0] && data->builddir[0] && data->cc[0] && data->ext[0] && data->name[0]))
		ERR(BAD_FORMAT, "One or more required options are not set");

	return 0;
}
