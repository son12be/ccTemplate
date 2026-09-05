/* TODO
 * Complete it?
 * Linker stuff
 * Sync and conter stuff - DONE
 * Replace vector with a VLA - DONE
 */


#include <libgen.h>
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
	sem_post(&sem);
}

inline static int
lines(FILE *file)
{
	if(!file)
		return -1;

	const int fd = fileno(file);
	if(fd < 0)
		return -1;

	struct statfs st;
	if(fstatfs(fd, &st) < 0)
		return -1;

	unsigned int lines = 0;
	char buf[st.f_bsize];
	int rc;
	while((rc = read(fd, buf, sizeof(buf))) > 0)
		for(int i = 0; i < rc; ++i)
			if(buf[i] == '\n')
				lines++;

	rewind(file);

	return lines;
}

inline static int
make_argv(char ***ret, const struct data_t * const data, int * const argc, int * const first_opt)
{
	*argc = 6; /* at least for cc, -c, filepath, -o; path, and NULL */

	/* get argc */
	for(char *s = data->cc; (s = strrchr(s, ' ')); s++, (*argc)++);
	if(data->flagFile)
	{
		*argc += lines(data->flagFile);
		if(*argc < 0)
			return log_err(-errno, CURPOS, "Failure counting flagfile lines");
	}

	/* allocate */
	*ret = malloc(sizeof(char*) * *argc);
	if(!*ret)
		return log_err(MALLOC, CURPOS, "Cant allocate argv");

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
		return log_err(MALLOC, CURPOS, "Cant allocate line buffer");
	for(; (fgets(line, VALUE_SIZE, data->flagFile)) != NULL; ++i_arg)
	{
		line[strcspn(line, "\n")] = '\0';
		(*ret)[i_arg] = line;

		line = malloc(VALUE_SIZE);
		if(!line)
		{
			for(int j = 0; j < i_arg; ++j)
				free((*ret)[j]);
			free(*ret);
			return log_err(MALLOC, CURPOS, "Cant allocate line buffer");
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
		return log_err(MALLOC, CURPOS, "Outpath allocation");
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
	for(; first_opt < ARGV_OFLAG_I; ++first_opt)
		free((*argv)[first_opt]);
	free((*argv)[ARGV_OUTPATH_I]);
	free(*argv);
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
		return log_err(-errno, CURPOS, "Cant fork");
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
		log_err(-errno, CURPOS, "Cant execute \"%s\"", argv[0]);
		report_err(); /* report now or never */
		exit(errno); /* Children shall never return */
	} else /* parent */
	{
		return 0;
	}
}

static inline int
loop_srcdir(const char *const srcdir, const char *const builddir, const char *const ext, char **argv, const int argc, const FILE *timestampsFile)
{
	DIR *dir = opendir(srcdir);
	if(!dir)
		return log_err(CANT_OPEN, CURPOS, "Source dir \"%s\"", srcdir);

	/* loop through srcdir */
	for(struct dirent *dirEntry = readdir(dir); dirEntry; dirEntry = readdir(dir))
	{
		if(!strends(dirEntry->d_name, ext))
			continue;

		free(argv[ARGV_PATH_I]);
		argv[ARGV_PATH_I] = malloc(VALUE_SIZE);
		if(!argv[ARGV_PATH_I])
			return log_err(MALLOC, CURPOS, "Cant allocate path buffer");

		snprintf(argv[ARGV_PATH_I], VALUE_SIZE, "%s/%s", srcdir, dirEntry->d_name);

		struct stat st;
		if(stat(argv[ARGV_PATH_I], &st) < 0)
			return log_err(CANT_OPEN, CURPOS, "Cant allocate stat buffer");
		if(!has_changed(timestampsFile, argv[ARGV_PATH_I], st.st_mtim.tv_sec))
			continue;

		char *filename = basename(argv[ARGV_PATH_I]);
		snprintf(strrchr(argv[ARGV_OUTPATH_I], '/') + 1, NAME_MAX - strlen(builddir) - 1, "%s.o", filename);

		if(exec_cc(argv) < 0)
			return -1;
	}

	closedir(dir);

	return 0;
}

int
compile(const struct data_t *data)
{
	FILE *timestampsFile;
	if(access(CHANGEFILE_FILENAME, R_OK) < 0)
	{
		timestampsFile = fopen(CHANGEFILE_FILENAME, "w+");
	} else
	{
		timestampsFile = fopen(CHANGEFILE_FILENAME, "r+");
	}
	if(!timestampsFile)
		return log_err(CANT_OPEN, CURPOS, CHANGEFILE_FILENAME);
	
	/* sem_init(3)§ERRORS
	 * How can this even fail?
	 * Note: by data->threads being too large
	 */
	if(sem_init(&sem, 0, data->threads) < 0)
		return log_err(CANT_OPEN, CURPOS, "Cant open semaphore");

	int argc;
	int first_opt;
	char **argv;
	if(make_argv(&argv, data, &argc, &first_opt) < 0)
		return -1;

	const struct sigaction sig = { .sa_handler = sigchld_handler };
	sigaction(SIGCHLD, &sig, NULL);

	/* loop through specified srcdirs */
	for(char *srcdir = strtok(data->srcdirs, " "); srcdir; srcdir = strtok(NULL, " "))
	{
		if(loop_srcdir(srcdir, data->builddir, data->ext, argv, argc, timestampsFile) < 0)
		{
			destroy_argv(&argv, argc, first_opt);
			return -1;
		}
	}

	fclose(timestampsFile);

	destroy_argv(&argv, argc, first_opt);

	return 0;
}
