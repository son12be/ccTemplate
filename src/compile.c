/* TODO
 * Complete it?
 * Linker stuff
 * Sync and conter stuff - DONE
 * Replace vector with a VLA
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

#include "util.h"
#include "misc.h"

static sem_t sem;

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
make_argv(char ***ret, const struct data_t *data, int *argc)
{
	if(!argc)
		return -1;

	if(!data->flagFile)
	{
		*ret = malloc(sizeof(char*) * 4); /* cc, objFlag, source file, and NULL */
		if(!*ret)
			return -1;

		(*ret)[0] = data->cc;
		(*ret)[1] = data->objFlag;
		(*ret)[3] = NULL;

		*argc = 4;

		return 0;
	}

	*argc = lines(data->flagFile);
	if(*argc < 0)
		return -1;

	*argc += 4;

	*ret = malloc(sizeof(char*) * *argc);
	if(!*ret)
		return -1;

	(*ret)[0] = data->cc;
	(*ret)[1] = data->objFlag;
	(*ret)[*argc - 1] = NULL;

	char *line = malloc(VALUE_SIZE);
	if(!line)
		return -1;

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
			return -1;
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
inline static void*
exec_cc(void *arg)
{
	/* No idea how to do this either */

	/* IMPORTANT LOOKATME REGRESSION
	 * Are we so fast that we replace argv[argc - 2] while we run execvp?
	 */

	char **argc = arg;

	sem_wait(&sem);
	pid_t pid = fork();
	if(pid < 0)
	{
		print_err(LOG_ERR, "Failed to fork: %s \n", STRERROR);
		return NULL;
	} else if(pid == 0) /* child */
	{
		for(int i = 0; argc[i] != NULL; ++i)
			printf("%s ", argc[i]);
		printf("\n");

		execvp(argc[0], argc);

		/* if exec returns, then it failed */
		print_err(LOG_ERR, "%s: %s \n", argc, STRERROR);
		return NULL;
	} else /* parent */
	{
		waitpid(pid, NULL, 0);
		sem_post(&sem);
		return NULL;
	}
}

static inline int
loop_srcdir(const char *srcdir, const struct data_t *data, char **argv, const int argc)
{
	DIR *dir = opendir(data->srcdirs);
	if(!dir)
	{
		print_err(LOG_ERR, "Cant open \"%s\": %s \n", srcdir, STRERROR);
		return -1;
	}

	/* loop through srcdir */
	for(struct dirent *dirEntry = readdir(dir); dirEntry; dirEntry = readdir(dir))
	{
		if(!strends(dirEntry->d_name, data->ext))
			continue;

		char *filePath = malloc(VALUE_SIZE);
		if(!filePath)
		{
			print_err(LOG_ERR, "Cant allocate: %s \n", STRERROR);
			return -1;
		}
		snprintf(filePath, VALUE_SIZE, "%s/%s", srcdir, dirEntry->d_name);

		struct stat st;
		if(stat(filePath, &st) < 0)
		{
			print_err(LOG_ERR, "%s: %s \n", filePath, STRERROR);
			continue;
		}

		// if(!has_changed(timestampsFile, filePath, st.st_mtim.tv_sec))
		// 	continue;

		/* Note for self: size - 1 is NULL. vector[size] */
		/* is not guaranteed to be NULL */
		argv[argc - 2] = filePath;

		pthread_t pthread_id;
		pthread_create(&pthread_id, NULL, exec_cc, argv);
	}

	closedir(dir);

	return 0;
}

int
compile(const struct data_t *data)
{
	if(!data)
		return -1;

	/* by default they are NULL, as set in main.c */
	if(!data->builddir ||
		!data->srcdirs ||
		!data->ext ||
		!data->objFlag ||
		!data->cc)
	{
		print_err(LOG_ERR, "One or more config keys are not set in the template \n");
		return -1;
	}

	FILE *timestampsFile = fopen(CHANGEFILE_FILENAME, "a+");
	if(!timestampsFile)
	{
		print_err(LOG_ERR, "Cant open %s: %s \n", CHANGEFILE_FILENAME, STRERROR);
		return -1;
	}
	
	/* sem_init(3)§ERRORS
	 * How can this even fail?
	 * Note: by data->threads being too large
	 */
	if(sem_init(&sem, 0, data->threads) < 0)
	{
		print_err(LOG_ERR, "Cant open semaphore: %s \n", STRERROR);
		return -1;
	}

	int argc;
	char **argv;
	if(make_argv(&argv, data, &argc) < 0)
	{
		print_err(LOG_ERR, "Failure making argument vector: %s \n", STRERROR);
		return -1;
	}

	/* loop through specified srcdirs */
	for(char *srcdir = strtok(data->srcdirs, "\t "); srcdir; srcdir = strtok(NULL, "\t "))
	{
		if(loop_srcdir(srcdir, data, argv, argc) < 0)
		{
			print_err(LOG_ERR, "Stopping compiling step due to a previous error \n");
			return -1;
		}
	}

	for(int i = 0; i < argc; ++i)
		free(argv[i]);

	free(argv);

	return 0;
}
