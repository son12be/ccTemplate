#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "compile.h"

#define CP_BUF(DST) memcpy(DST, buf + i_delim + 1, sizeof(buf) - i_delim)

/* bro meson is so much better wtf */

int
main(int argc, char **argv)
{
	char templatePath[128] = TEMPLATE_FILENAME;
	struct data_t data = 
	{
		.srcdirs = { '\0' },
		.incdirs = { '\0' },
		.builddir = { '\0' },
		.ext = { '\0' },
		.cc = { '\0' },
		.threads = 1,
		.objFlag = { '\0' },
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

	FILE *file = fopen(templatePath, "r");
	if(!file)
	{
		fprintf(stderr, "Cant open \"%s\": %s \n", templatePath, strerror(errno));
		return -1;
	}
	
	/* Fill up data */
	char buf[VALUE_SIZE];
	while((fgets(buf, sizeof(buf), file)) != NULL)
	{
		/* TODO clean this up */
		/* ignore lines starting w/ '#' or a whitespace */
		if(buf[0] == '#' || isspace((unsigned char)buf[0]))
			continue;

		unsigned int i_delim = 0;
 		/* zero-init not needed b/c of this. If value is empty, then \n would be at value's place */
		/* and after i_delim, so when its \0'd, NUL is always after i_delim */
		/* * important for the next check btw */
		buf[strcspn(buf, "\n")] = '\0';
		i_delim = strcspn(buf, "\t "); /* find the whitespace between key and value */
		if(buf[i_delim] == '\0' || buf[i_delim + 1] == '\0') /* nothing past i_delim - 1, no value */
		{
			fprintf(stderr, "WARNING: \"%s\" has no value assigned to it \n", buf);
			continue;
		}

		/* note for self: i_delim points AT the whitespace */
		/* (it may not before the previous check) */
		buf[i_delim] = '\0';

		if(streq(buf, "SRCDIR"))
			CP_BUF(data.srcdirs);
		else if(streq(buf, "INCDIR"))
			CP_BUF(data.incdirs);
		else if(streq(buf, "BUILDDIR"))
			CP_BUF(data.builddir);
		else if(streq(buf, "EXT"))
			CP_BUF(data.ext);
		else if(streq(buf, "CC"))
			CP_BUF(data.cc);
		else if(streq(buf, "OFLAG"))
			CP_BUF(data.objFlag);
		else if(streq(buf, "FLAGFILE"))
			data.flagFile = fopen(buf + i_delim + 1, "r");
	}

	compile(&data);

	return 0;
}
