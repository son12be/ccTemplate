#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "compile.h"

/* bro meson is so much better wtf */

int
main(int argc, char **argv)
{
	char templatePath[128] = TEMPLATE_FILENAME;
	struct data_t data = 
	{
		.srcdirs = NULL,
		.incdirs = NULL,
		.builddir = NULL,
		.ext = NULL,
		.cc = NULL,
		.threads = 1,
		.objFlag = NULL,
		.flagFile = NULL,
	};

	int opt;
	while((opt = getopt(argc, argv, "c:j:")) != -1)
	{
		switch(opt)
		{
			case 'c':
				memcpy(templatePath, optarg, sizeof(templatePath));
				break;
			case 'j':
				data.threads = atoi(optarg);
				if(data.threads == 0)
				{
					fprintf(stderr, "\"%s\" is not a valid argument for -j \n", optarg);
					return -1;
				}
				break;
		}
	}

	parse_template(&data, templatePath);

	// printf("%s %s %s %s %s %i\n", data.srcdirs, data.incdirs, data.builddir, data.ext, data.cc, data.threads);
	compile(&data);

	return 0;
}
