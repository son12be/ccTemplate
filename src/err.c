#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "err.h"

static char *s_Context;
static char *s_Location;
static int s_Code;

int
log_err(error_e code, char *context, char *location)
{
	if(context)
	{
		if(s_Context)
			free(s_Context);

		s_Context = malloc(strlen(context) + 1);

		memcpy(s_Context, context, strlen(context) + 1);
	}

	s_Location = location ? location : "Someone didnt put the location here";

	s_Code = code;

	return -code;
}

int
report_err()
{
	/* TODO
	 * Colors: Red for err info; green for hint and blue for location
	 */

	if(s_Code < 0)
	{
		fprintf(stderr, "%s", strerror(-s_Code));
	} else
	{
		switch(s_Code)
		{
			case OK:
				fprintf(stderr, "No error. \n");
				return s_Code;
			case BAD_CONFIG:
				fprintf(stderr, "Bad configuration file/option");
				break;
			case NULL_POINTER:
				fprintf(stderr, "A NULL pointer was passed");
				break;
			case CANT_OPEN:
				fprintf(stderr, "Cant open (%s)", strerror(s_Code));
				break;
			case MALLOC:
				fprintf(stderr, "malloc() failed (%s)", strerror(s_Code));
				break;
		}
	}
	if(s_Context)
		fprintf(stderr, ". Hint: %s", s_Context);
	fprintf(stderr, ". At %s \n", s_Location);

	return s_Code;
}

int
get_err()
{
	return s_Code;
}
