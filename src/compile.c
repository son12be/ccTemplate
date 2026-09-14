/* TODO
 * Complete it?
 * Linker stuff
 * Sync and conter stuff - DONE
 * Replace vector with a VLA - DONE
 */


#include <libgen.h>
#include <glob.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/vfs.h>
#include <stdlib.h>
#include <signal.h>

#include "util.h"
#include "misc.h"
#include "err.h"

#include "compile.h"

#define ARGV_NULL_I argc - 1
#define ARGV_OUTPATH_I argc - 2
#define ARGV_OFLAG_I argc - 3
#define ARGV_PATH_I argc - 4

static sem_t sem;

void
sigchld_handler(int)
{
	while(waitpid(-1, NULL, WNOHANG) > 0)
		sem_post(&sem);
}

inline static int
lines(FILE *file)
{
	int lines = 0;
	char buf[VALUE_SIZE];
	while(fgets(buf, sizeof(buf), file))
		lines++;

	rewind(file);

	return lines;
}

/*
 * first_opt should point one past '-c'.
 * From first_opt and on, theres malloc'd buffers.
 * Before first_opt, theres '-c' and then a strtok'd stack-allocated buffer
 */
inline static int
make_argv(char ***ret, const struct data_t *const data, int *const argc, int *const first_opt)
{
	*argc = 6; /* at least for cc, -c, filepath, -o; path, and NULL */

	/* get argc */
	for(char *s = data->cc; (s = strrchr(s, ' ')); s++, (*argc)++);
	if(data->flagFile)
	{
		*argc += lines(data->flagFile);
		if(*argc < 0)
			ERR(-errno, "Failure counting flagfile lines");
	}

	/* allocate */
	*ret = malloc(sizeof(char*) * *argc);
	if(!*ret)
		ERR(MALLOC, "Cant allocate argv");

	/* get args from data->cc */
	int i_arg = 0;
	for(char *token = strtok(data->cc, " "); token; token = strtok(NULL, " "), ++i_arg)
		(*ret)[i_arg] = token;

	(*ret)[i_arg++] = "-c";

	if(!data->flagFile)
		goto end;

	*first_opt = i_arg;

	/* get args from flagFile */
	char *line = malloc(VALUE_SIZE / 2);
	if(!line)
		ERR(MALLOC, "Cant allocate line buffer");
	for(; (fgets(line, VALUE_SIZE / 2, data->flagFile)) != NULL; ++i_arg)
	{
		line[strcspn(line, "\n")] = '\0';
		(*ret)[i_arg] = line;

		line = malloc(VALUE_SIZE);
		if(!line)
		{
			for(int j = 0; j < i_arg; ++j)
				free((*ret)[j]);
			free(*ret);
			ERR(MALLOC, "Cant allocate line buffer");
		}
	}

	free(line);


end:
	int maxLenSz = strlen(data->builddir) + NAME_MAX + 2;
	(*ret)[*ARGV_OUTPATH_I] = malloc(maxLenSz);
	if(!*ret)
	{
		for(int i = *first_opt; i < *ARGV_OFLAG_I; ++i)
			free((*ret)[i]);
		free(*ret);
		ERR(MALLOC, "Cant allocate outpath");
	}

	snprintf((*ret)[*ARGV_OUTPATH_I], maxLenSz, "%s/", data->builddir);

	(*ret)[*ARGV_OFLAG_I] = "-o";
	(*ret)[*ARGV_PATH_I] = NULL;
	(*ret)[*ARGV_NULL_I] = NULL;
	return 0;
}

static void
destroy_argv(char ***argv, int argc, int first_opt)
{
	for(; first_opt < ARGV_PATH_I; ++first_opt)
		free((*argv)[first_opt]);
	free((*argv)[ARGV_OUTPATH_I]);
	*argv = NULL;
}

/*
 * Search in the (open) file for fileName, then compare currentTime with
 * oldTime to check blah blah blah
 */
inline static int
has_changed(FILE *file, const char *filePath, const time_t currentTime)
{
	rewind(file);

	char line[VALUE_SIZE];
	while(fgets(line, sizeof(line), file) != NULL) /* lookup fileName */
	{
		line[strcspn(line, "\n")] = '\0';
		const char *curPath = strchr(line, ' ');
		if(!curPath)
			continue;

		curPath++;
		if(!streq(curPath, filePath))
			continue;

		time_t oldTime;
		sscanf(line, "%010lu", &oldTime);

		if(oldTime >= currentTime)
			return 0;

		fseek(file, -strlen(line) - 1, SEEK_CUR);
		fprintf(file, "%010lu", currentTime);

		return 1;
	}

	/* add file if not found */
	fseek(file, 0, SEEK_END);
	fprintf(file, "%010lu %s\n", currentTime, filePath);

	return 1;
}

/*
 * Decrement sem and "fork and exec"
 * sigchld will handle incrementing sem
 */
inline static int
exec_cc(char **argv)
{
	/* No idea how to do this either */

	sem_wait(&sem);
	pid_t pid = fork();
	if(pid < 0)
	{
		ERR(-errno, "Cant fork");
	} else if(pid == 0) /* child */
	{
		/* TODO
		 * Move output to a logfile
		 */
		for(int i = 0; argv[i] != NULL; ++i)
			printf("%s ", argv[i]);
		printf("\n");

		execvp(argv[0], argv);

		/* if exec returns, then it failed */
		log_err(-errno, "Cant execute \"%s\"", argv[0]);
		report_err(); /* report now or never */
		exit(errno); /* Children shall never return */
	} else /* parent */
	{
		return 0;
	}
}

static inline int
compile_srcdir(const char *const srcdir, const char *const builddir, const char *const ext, char **argv, const int argc, const FILE *timestampsFile)
{
	const DIR *dir = opendir(srcdir);
	if(!dir)
		ERR(CANT_OPEN, "Source dir was \"%s\"", srcdir);

	glob_t file_list;
	char pattern[strlen(srcdir) + 1 + sizeof("*.") + strlen(ext)];

	snprintf(pattern, sizeof(pattern), "%s/*.%s", srcdir, ext);

	/* TODO maybe store rc? */
	if(glob(pattern, GLOB_NOSORT, NULL, &file_list))
	{
		closedir(dir);
		ERR(NOMATCH, "Pattern was \"%s\"", pattern);
	}

	/* loop through returned paths */
	for(unsigned int i = 0; i < file_list.gl_pathc; ++i)
	{
		argv[ARGV_PATH_I] = file_list.gl_pathv[i];

		struct stat st;
		if(stat(argv[ARGV_PATH_I], &st) < 0)
		{
			ERR_NR(CANT_OPEN, "Cant get stat struct for file \"%s\"", argv[ARGV_PATH_I]);
			goto err;
		}
		if(!has_changed(timestampsFile, argv[ARGV_PATH_I], st.st_mtim.tv_sec))
			continue;

		snprintf(argv[ARGV_OUTPATH_I], NAME_MAX + strlen(builddir) + 2, "%s/%s.o", builddir, basename(argv[ARGV_PATH_I]));

		if(exec_cc(argv) < 0)
			goto err;
	}

	closedir(dir);
	globfree(&file_list);

	return 0;

err:
	closedir(dir);
	globfree(&file_list);
	return -1;
}

int
compile(const struct data_t *data)
{
	FILE *timestampsFile;
	if(access(CHANGEFILE_FILENAME, F_OK) < 0)
	{
		timestampsFile = fopen(CHANGEFILE_FILENAME, "w+");
	} else
	{
		timestampsFile = fopen(CHANGEFILE_FILENAME, "r+");
	}
	if(!timestampsFile)
		ERR(CANT_OPEN, CHANGEFILE_FILENAME);
	
	/* sem_init(3)§ERRORS
	 * How can this even fail?
	 * Note: by data->threads being too large
	 */
	if(sem_init(&sem, 0, data->threads) < 0)
	{
		fclose(timestampsFile);
		ERR(CANT_OPEN, "Cant open semaphore");
	}

	int argc;
	int first_opt;
	char **argv;
	if(make_argv(&argv, data, &argc, &first_opt) < 0)
	{
		fclose(timestampsFile);
		return -1;
	}

	const struct sigaction sig = { .sa_handler = sigchld_handler };
	sigaction(SIGCHLD, &sig, NULL);

	/* loop through specified srcdirs */
	for(char *srcdir = strtok(data->srcdirs, " "); srcdir; srcdir = strtok(NULL, " "))
	{
		int rc;
		if((rc = compile_srcdir(srcdir, data->builddir, data->ext, argv, argc, timestampsFile)) < 0)
		{
			fclose(timestampsFile);
			destroy_argv(&argv, argc, first_opt);
			return -1;
		}
	}

	fclose(timestampsFile);

	/* move eveything before first_opt by one to overwrite '-c' */
	memmove(argv + 1, argv, sizeof(char*) * (first_opt - 1));
	argv++; argc--; first_opt--;

	/* Fuck it, simplier than having to deal with what to do with PATH */
	argv[ARGV_PATH_I] = "-Wl,--as-needed";

	glob_t file_list;
	char pattern[strlen(data->builddir) + 1 + sizeof("*.o")];

	snprintf(pattern, sizeof(pattern), "%s/*.o", data->builddir);
	if(glob(pattern, GLOB_NOSORT, NULL, &file_list) != 0)
	{
		destroy_argv(&argv, argc, first_opt);
		ERR(NOMATCH, "Pattern was \"%s\"", pattern);
	}

	// argv = realloc(argv, sizeof(char*) * (argc + file_list.gl_pathc + 1)); // argv[<first_opt] not malloc'd
	int bytes = sizeof(char*) * (argc + file_list.gl_pathc + 1);
	char **argv_cc = malloc(bytes);
	memcpy(argv_cc, argv, bytes);
	argv = argv_cc;

	snprintf(argv[ARGV_OUTPATH_I], strlen(data->builddir) + NAME_MAX + 2, "%s/dummy", data->builddir);

	memcpy(argv + ARGV_NULL_I, file_list.gl_pathv, sizeof(char*) * file_list.gl_pathc);
	argc += file_list.gl_pathc;

	for(int i = 0; i < argc; ++i)
		printf("%s ", argv[i]);
	printf("\n");

	execvp(argv[0], argv);

	fprintf(stderr, "%s: %s \n", argv[0], STRERROR);
	globfree(&file_list);
	destroy_argv(&argv, argc, first_opt);

	return 0;
}
