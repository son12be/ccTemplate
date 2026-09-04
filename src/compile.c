/* TODO
 * Complete it?
 * Linker stuff
 * Sync and conter stuff - DONE
 * Replace vector with a VLA - DONE
 */


#include <libgen.h>
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
		return errno;

	const int fd = fileno(file);
	if(fd < 0)
		return errno;

	struct statfs st;
	if(fstatfs(fd, &st) < 0)
		return errno;

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
make_argv(char ***ret, const struct data_t *data, int *argc, int *first_opt)
{
	if(!argc || !first_opt)
		return log_err(NULL_POINTER, __FUNCTION__, CURPOS);

	*argc = 5; /* at least for cc; filepath, -o; path, and NULL */

	/* get argc */
	for(int i = 0; (data->cc)[i] != '\0'; ++i)
		if((data->cc)[i] == ' ') (*argc)++;
	if(data->flagFile)
	{
		*argc += lines(data->flagFile);
		if(*argc < 0)
			return log_err(errno, "Flagfile line count", CURPOS);
	}

	*first_opt = *argc + 1;

	/* allocate */
	*ret = malloc(sizeof(char*) * *argc);
	if(!*ret)
		return log_err(MALLOC, "argv allocation", CURPOS);

	/* get args from data->cc */
	int i = 0;
	for(char *token = strtok(data->cc, " "); token; token = strtok(NULL, " "), ++i)
		(*ret)[i] = token;

	if(!data->flagFile)
		goto end;

	/* get args from flagFile */
	char *line = malloc(VALUE_SIZE / 2);
	if(!line)
		return log_err(MALLOC, "Flagfile line buffer", CURPOS);
	for(; (fgets(line, VALUE_SIZE, data->flagFile)) != NULL; ++i)
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

end:
	static char outpath[VALUE_SIZE / 2];
	snprintf(outpath, sizeof(outpath), "%s/", data->builddir);
	(*ret)[*ARGV_OUTPATH_I] = outpath;

	(*ret)[*ARGV_OFLAG_I] = "-o";
	(*ret)[*ARGV_NULL_I] = NULL;
	return 0;
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
		return log_err(errno, "Cant fork", CURPOS);
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
		log_err(errno, argv[0], CURPOS);
		report_err(); /* report now or never */
		exit(errno); /* Children shall never return */
	} else /* parent */
	{
		return 0;
	}
}

static inline int
loop_srcdir(const char *srcdir, const char *builddir, const char *ext, char **argv, const int argc, const FILE *timestampsFile)
{
	DIR *dir = opendir(srcdir);
	if(!dir)
		return log_err(CANT_OPEN, srcdir, CURPOS);

	/* loop through srcdir */
	int fileCount = 0;
	for(struct dirent *dirEntry = readdir(dir); dirEntry; dirEntry = readdir(dir))
	{
		if(!strends(dirEntry->d_name, ext))
			continue;

		char *filePath = malloc(VALUE_SIZE);
		if(!filePath)
			return log_err(MALLOC, "File path for source file", CURPOS);

		snprintf(filePath, VALUE_SIZE, "%s/%s", srcdir, dirEntry->d_name);

		struct stat st;
		if(stat(filePath, &st) < 0)
			return log_err(CANT_OPEN, "Stat struct", CURPOS);
		if(!has_changed(timestampsFile, filePath, st.st_mtim.tv_sec))
			continue;

		if(fileCount > 0)
			free(argv[ARGV_PATH_I]);
		argv[ARGV_PATH_I] = filePath;

		snprintf(strrchr(argv[ARGV_OUTPATH_I], '/') + 1, VALUE_SIZE / 2, "%i.o", fileCount);
		fileCount++;

		if(exec_cc(argv) < 0)
		{
			free(argv[ARGV_PATH_I]);
			return -1;
		}
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
		return log_err(CANT_OPEN, CHANGEFILE_FILENAME, CURPOS);
	
	/* sem_init(3)§ERRORS
	 * How can this even fail?
	 * Note: by data->threads being too large
	 */
	if(sem_init(&sem, 0, data->threads) < 0)
		return log_err(CANT_OPEN, "Semaphore", CURPOS);

	int argc;
	int i_firstOpt;
	char **argv;
	if(make_argv(&argv, data, &argc, &i_firstOpt) < 0)
		return -1;

	const struct sigaction sig = { .sa_handler = sigchld_handler };
	sigaction(SIGCHLD, &sig, NULL);

	/* loop through specified srcdirs */
	for(char *srcdir = strtok(data->srcdirs, "\t "); srcdir; srcdir = strtok(NULL, "\t "))
	{
		if(loop_srcdir(srcdir, data->builddir, data->ext, argv, argc, timestampsFile) < 0)
			return -1;
	}

	fclose(timestampsFile);

	for(; i_firstOpt < ARGV_OFLAG_I; ++i_firstOpt)
		free(argv[i_firstOpt]);

	free(argv);

	return 0;
}
