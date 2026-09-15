#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "compile.h"
#include "err.h"

/* bro meson is so much better wtf */

int
main(int argc, char **argv)
{
	struct data_t data = 
	{
		.srcdirs = { 0 },
		.builddir = { 0 },
		.ext = { 0 },
		.cc = { 0 },
		.threads = 1,
		.flagFile = NULL,
	};

	int opt;
	while((opt = getopt(argc, argv, "c:j:")) != -1)
	{
		switch(opt)
		{
			case 'j':
				data.threads = atoi(optarg);
				if(data.threads == 0)
				{
					fprintf(stderr, "\"%s\" is not a valid argument for -j \n", optarg);
					return -1;
				}
				break;
			case '?':
				return -1;
				break;
		}
	}

	if(parse_template(&data) < 0)
		return report_err();

	// printf("%s %s %s %s %s %i\n", data.srcdirs, data.incdirs, data.builddir, data.ext, data.cc, data.threads);
	if(compile(&data) < 0)
		return report_err();

	/* We gotta do something about this
	 * Maybe an fatal flag?
	 */
	if(get_err())
		report_err();

	return 0;
}
