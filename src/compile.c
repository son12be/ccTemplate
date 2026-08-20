/* TODO
 * Complete it?
 * Linker stuff
 * Sync and conter stuff - DONE
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

static sem_t sem;

static vector_t *
make_argv(const struct data_t *data)
{
	vector_t *vector = vector_new(20, sizeof(char*));
	if(!vector)
		return NULL;

	/* append compiler and flag to make object files */
	vector_append(vector, data->cc);
	vector_append(vector, data->objFlag);

	/* return now if no flag file */
	if(!data->flagFile)
		return vector;

	/* loop through flag file */
	char line[512];
	while((fgets(line, sizeof(line), data->flagFile)) != NULL)
	{
		line[strcspn(line, "\n")] = '\0';
		vector_append(vector, line);
	}

	/* reserve for file and NULL */
	vector_reserve_with_null(vector, vector_size(vector) + 2);

	return vector;
}

/*
 * Search in the (open) file for fileName, then compare currentTime with
 * oldTime to check blah blah blah
 */
static int
has_changed(FILE *file, const char *fileName, const time_t currentTime)
{
	char line[1024];
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
static void*
exec_cc(void *arg)
{
	/* No idea how to do this either */

	vector_t *vector = arg;

	sem_wait(&sem);
	execvp(vector_at(vector, 0), vector_data(vector));
	sem_post(&sem);

	return NULL;
}

void
compile(const struct data_t *data)
{
	if(!data)
		return;

	/* by default they are NULL, as set in main.c */
	if(!data->builddir ||
		!data->srcdirs ||
		!data->incdirs ||
		!data->ext ||
		!data->cc)
	{
		fprintf(stderr, "ERROR: One or more config keys are not set in the template \n");
		return;
	}

	FILE *timestampsFile = fopen(CHANGEFILE_FILENAME, "a+");
	if(!timestampsFile)
	{
		fprintf(stderr, "Error opening %s: %s \n", CHANGEFILE_FILENAME, strerror(errno));
		return;
	}
	
	/* sem_init(3)§ERRORS
	 * How can this even fail?
	 * Note: by data->threads being too large
	 */
	if(sem_init(&sem, 0, data->threads) < 0)
	{
		fprintf(stderr, "Could not open semaphore: %s \n", strerror(errno));
		return;
	}

	vector_t *vector = make_argv(data);

	/* loop through specified srcdirs */
	char *srcdir = strtok(data->srcdirs, "\t ");
	while(srcdir)
	{
		/* open it */
		DIR *dir = opendir(data->srcdirs);
		if(!dir)
		{
			fprintf(stderr, "Could not open \"%s\": %s \n", srcdir, strerror(errno));
			continue;
		}

		/* loop through srcdir */
		struct dirent *dirEntry;
		while((dirEntry = readdir(dir)) != NULL)
		{
			if(!strends(dirEntry->d_name, data->ext))
				continue;

			char filePath[sizeof(data->srcdirs)];
			snprintf(filePath, sizeof(filePath), "%s/%s", srcdir, dirEntry->d_name);

			struct stat st;
			if(stat(filePath, &st) < 0)
			{
				fprintf(stderr, "%s: %s \n", filePath, strerror(errno));
				continue;
			}

			if(!has_changed(timestampsFile, filePath, st.st_mtim.tv_sec))
				continue;

			/* Note for self: size - 1 is NULL. vector[size] */
			/* is not guaranteed to be NULL */
			vector_replace(vector, vector_size(vector) - 2, filePath);

			pthread_t pthread_id;
			pthread_create(&pthread_id, NULL, exec_cc, vector);
		}

		/* close it and get new srcdir */
		closedir(dir);
		srcdir = strtok(NULL, "\t ");
	}

	vector_free(&vector);
}
