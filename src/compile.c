/* TODO
 * Complete it?
 * Linker stuff
 * Sync and conter stuff - DONE
 * Replace vector with a VLA - DONE
 */


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

static sem_t sem;

void
sigchild_handler(int sig)
{
	sem_post(&sem);
}

inline static int
lines(FILE *file)
{
	if(!file)
		return -errno;

	const int fd = fileno(file);
	if(fd < 0)
		return -errno;

	struct statfs st;
	if(fstatfs(fd, &st) < 0)
		return -errno;

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
make_argv(char ***ret, const struct data_t *data, int *argc)
{
	if(!argc)
		return log_err(NULL_POINTER, __FUNCTION__, CURPOS);

	if(!data->flagFile)
	{
		*ret = malloc(sizeof(char*) * 4); /* cc, objFlag, source file, and NULL */
		if(!*ret)
			return log_err(MALLOC, "argv allocation", CURPOS);

		(*ret)[0] = data->cc;
		(*ret)[1] = data->objFlag;
		(*ret)[3] = NULL;

		*argc = 4;

		return 0;
	}

	*argc = lines(data->flagFile);
	if(*argc < 0)
		return log_err(MALLOC, "Flagfile line count", CURPOS);

	*argc += 4;

	*ret = malloc(sizeof(char*) * *argc);
	if(!*ret)
		return log_err(MALLOC, "argv allocation", CURPOS);

	(*ret)[0] = data->cc;
	(*ret)[1] = data->objFlag;
	(*ret)[*argc - 1] = NULL;

	char *line = malloc(VALUE_SIZE);
	if(!line)
		return log_err(MALLOC, "Flagfile line buffer", CURPOS);

	for(int i = 2; (fgets(line, VALUE_SIZE, data->flagFile)) != NULL; ++i)
	{
		line[strcspn(line, "\n")] = '\0';
		(*ret)[i] = line;

		line = malloc(VALUE_SIZE);
		if(!line)
		{
			for(int j = 0; j < i; ++j)
				free((*ret)[j]);
			free(*ret);
			return log_err(MALLOC, "Flagfile line buffer", CURPOS);
		}
	}

	return 0;
}

/*
 * Search in the (open) file for fileName, then compare currentTime with
 * oldTime to check blah blah blah
 */
inline static int
has_changed(FILE *file, const char *fileName, const time_t currentTime)
{
	char line[VALUE_SIZE];
	time_t oldTime;
	while(fgets(line, sizeof(line), file)) /* lookup fileName */
	{
		if(!strstr(line, fileName))
			continue;

		sscanf(line, "%s %lu", line, &oldTime);

		if(oldTime >= currentTime)
			return 0;

		/* XXX may be wrong */
		fseek(file, strlen(line) + 1, SEEK_CUR);
		fprintf(file, "%s %lu\n", fileName, currentTime);

		return 1;
	}

	/* add file if not found */
	fseek(file, 0, SEEK_END);
	fprintf(file, "%s %lu\n", fileName, currentTime);

	return 1;
}

/*
 * We need a wrapper since exec* functions require more than
 * the void* argument pthread_create() provides
 */
inline static int
exec_cc(char **argv)
{
	/* No idea how to do this either */

	sem_wait(&sem);
	pid_t pid = fork();
	if(pid < 0)
	{
		return log_err(-errno, "Cant fork", CURPOS);
	} else if(pid == 0) /* child */
	{
		/* TODO
		 * Move output to a logfile
		 */
		// for(int i = 0; argv[i] != NULL; ++i)
		// 	printf("%s ", argv[i]);
		// printf("\n");

		execvp(argv[0], argv);

		/* if exec returns, then it failed */
		log_err(-errno, argv[0], CURPOS);
		report_err(); /* report now or never */
		exit(errno); /* Children shall never return */
	} else /* parent */
	{
		return 0;
	}
}

static inline int
loop_srcdir(const char *srcdir, const char *ext, char **argv, const int argc)
{
	DIR *dir = opendir(srcdir);
	if(!dir)
		return log_err(CANT_OPEN, srcdir, CURPOS);

	/* loop through srcdir */
	for(struct dirent *dirEntry = readdir(dir); dirEntry; dirEntry = readdir(dir))
	{
		if(!strends(dirEntry->d_name, ext))
			continue;

		char *filePath = malloc(VALUE_SIZE);
		if(!filePath)
			log_err(MALLOC, "File path for source file", CURPOS);

		snprintf(filePath, VALUE_SIZE, "%s/%s", srcdir, dirEntry->d_name);

		struct stat st;
		if(stat(filePath, &st) < 0)
		{
			log_err(CANT_OPEN, "Stat struct", CURPOS);
			continue;
		}

		// if(!has_changed(timestampsFile, filePath, st.st_mtim.tv_sec))
		// 	continue;

		/* Note for self: size - 1 is NULL. vector[size] */
		/* is not guaranteed to be NULL */
		argv[argc - 2] = filePath;

		exec_cc(argv);
	}

	closedir(dir);

	return 0;
}

int
compile(const struct data_t *data)
{
	/* by default they are NULL, as set in main.c */
	if(!data->builddir ||
		!data->srcdirs ||
		!data->ext ||
		!data->objFlag ||
		!data->cc)
	{
		return log_err(BAD_CONFIG, "One or more options are not set", CURPOS);
	}

	FILE *timestampsFile = fopen(CHANGEFILE_FILENAME, "a+");
	if(!timestampsFile)
		return log_err(CANT_OPEN, CHANGEFILE_FILENAME, CURPOS);
	
	/* sem_init(3)§ERRORS
	 * How can this even fail?
	 * Note: by data->threads being too large
	 */
	if(sem_init(&sem, 0, data->threads) < 0)
		return log_err(CANT_OPEN, "Semaphore", CURPOS);

	int argc;
	char **argv;
	if(make_argv(&argv, data, &argc) < 0)
		return get_err();

	const struct sigaction sig = { .sa_handler = sigchild_handler };
	sigaction(SIGCHLD, &sig, NULL);

	/* loop through specified srcdirs */
	for(char *srcdir = strtok(data->srcdirs, "\t "); srcdir; srcdir = strtok(NULL, "\t "))
	{
		if(loop_srcdir(srcdir, data->ext, argv, argc) < 0)
			return get_err();
	}

	for(int i = 0; i < argc; ++i)
		free(argv[i]);

	free(argv);

	return 0;
}
