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

/* in local machine */
#include <vector.h>

#include "util.h"
#include "misc.h"

static sem_t sem;

inline static vector_t *
make_argv(const struct data_t *data)
{
	vector_t *vector = vector_new(20, sizeof(char*)); /* XXX: magic value? */
	if(!vector)
		return NULL;

	/* append compiler and flag to make object files */
	vector_append(vector, &(data->cc));
	vector_append(vector, &(data->objFlag));

	/* return now if no flag file */
	if(!data->flagFile)
		return vector;

	/* loop through flag file */
	char *line = malloc(VALUE_SIZE);
	if(!line)
		goto err;

	while((fgets(line, VALUE_SIZE, data->flagFile)) != NULL)
	{
		line[strcspn(line, "\n")] = '\0';
		if(!vector_append(vector, &line))
			goto err;

		line = malloc(VALUE_SIZE);
		if(!line)
			goto err;
	}

	/* reserve for file and NULL */
	if(!vector_reserve_with_null(vector, vector_size(vector) + 2))
		goto err;

	return vector;

err:
	for(int i = 0; i < vector_size(vector); ++i)
		free(*(char**)vector_at(vector, i));
	vector_free(&vector);
	return NULL;
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

	vector_t *vector = arg;

	sem_wait(&sem);
	pid_t pid = fork();
	if(pid < 0)
	{
		print_err(LOG_ERR, "Failed to fork: %s \n", STRERROR);
		return NULL;
	} else if(pid == 0) /* child */
	{
		for(uint32_t i = 0; i < vector_size(vector); ++i)
			printf("%s ", *(char**)vector_at(vector, i));
		printf("\n");

		execvp(*(char**)vector_at(vector, 0), (char**)vector_data(vector));

		/* if exec returns, then it failed */
		print_err(LOG_ERR, "%s: %s \n", *(char**)vector_at(vector, 0), STRERROR);
		return NULL;
	} else /* parent */
	{
		waitpid(pid, NULL, 0);
		sem_post(&sem);
		return NULL;
	}
}

static inline int
loop_srcdir(const char *srcdir, const struct data_t *data, vector_t *vector)
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
		vector_replace(vector, vector_size(vector) - 2, &filePath);

		pthread_t pthread_id;
		pthread_create(&pthread_id, NULL, exec_cc, vector);
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

	vector_t *vector = make_argv(data);
	if(!vector)
	{
		print_err(LOG_ERR, "Failure making argument vector: %s \n", STRERROR);
		return -1;
	}

	/* loop through specified srcdirs */
	for(char *srcdir = strtok(data->srcdirs, "\t "); srcdir; srcdir = strtok(NULL, "\t "))
	{
		if(loop_srcdir(srcdir, data, vector) < 0)
		{
			print_err(LOG_ERR, "Stopping compiling step due to a previous error \n");
			return -1;
		}
	}

	for(int i = 0; i < vector_size(vector); ++i)
		free(*(char**)vector_at(vector, i));

	vector_free(&vector);

	return 0;
}
