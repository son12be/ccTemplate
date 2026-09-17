/* TODO
 * Complete it?
 * Linker stuff
 * Sync and conter stuff - DONE
 * Replace vector with a VLA - DONE
 */


#include <libgen.h>
#include <fcntl.h>
#include <sys/stat.h>
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

#define LABEL_FILE ".last_label"

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
	for(char *s = data->cc; (s = strchr(s, ' ')) != NULL; s++, (*argc)++);
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
compile_srcdir(const char *const srcdir, const struct data_t *data, char **argv, const int argc, const FILE *timestampsFile, int compile_anyways)
{
	if(!data->label)
		goto howdoinamethis;

	{ /* if we dont add this scope, above goto would bypass init of VLA last_label and give err */
		const int label_file_fd = open(LABEL_FILE, O_RDWR | O_CREAT, 0644);
		if(label_file_fd < 0)
			ERR(CANT_OPEN, "Cant open label file \"%s\"", LABEL_FILE);

		const int label_file_len = lseek(label_file_fd, 0, SEEK_END);
		lseek(label_file_fd, 0, SEEK_SET);
		char last_label[label_file_len];
		read(label_file_fd, last_label, sizeof(last_label));
		last_label[label_file_len - 1] = '\0';
		if(strcmp(data->label, last_label) != 0)
		{
			memset(last_label, 0, sizeof(last_label));
			pwrite(label_file_fd, last_label, sizeof(last_label), 0);
			pwrite(label_file_fd, data->label, strlen(data->label) + 1, 0);
			close(label_file_fd);
			compile_anyways = 1;
		}
	}

howdoinamethis:
	const DIR *dir = opendir(srcdir);
	if(!dir)
		ERR(CANT_OPEN, "Source dir was \"%s\"", srcdir);

	glob_t file_list;
	char pattern[strlen(srcdir) + 1 + sizeof("*.") + strlen(data->ext)];

	snprintf(pattern, sizeof(pattern), "%s/*.%s", srcdir, data->ext);

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
		if(!compile_anyways && !has_changed(timestampsFile, argv[ARGV_PATH_I], st.st_mtim.tv_sec))
			continue;

		snprintf(argv[ARGV_OUTPATH_I], NAME_MAX + strlen(data->builddir) + 2,
				"%s/%s.o",
				data->builddir,
				basename(argv[ARGV_PATH_I]));

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
compile(const struct data_t *data, const int compile_anyways)
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
		waitpid(-1, NULL, 0);
		if(compile_srcdir(srcdir, data, argv, argc, timestampsFile, compile_anyways) < 0)
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
		globfree(&file_list);
		ERR(NOMATCH, "Pattern was \"%s\"", pattern);
	}

	// argv = realloc(argv, sizeof(char*) * (argc + file_list.gl_pathc + 1)); // argv[<first_opt] not malloc'd
	int bytes = sizeof(char*) * (argc + file_list.gl_pathc + 1);
	char **argv_cc = malloc(bytes);
	memcpy(argv_cc, argv, bytes);
	argv = argv_cc;

	snprintf(argv[ARGV_OUTPATH_I], strlen(data->builddir) + NAME_MAX + 2, "%s/%s", data->builddir, data->name);

	memcpy(argv + ARGV_NULL_I, file_list.gl_pathv, sizeof(char*) * (file_list.gl_pathc + 1));
	argc += file_list.gl_pathc;

	for(int i = 0; i < ARGV_NULL_I; ++i)
		printf("%s ", argv[i]);
	printf("\n");

	execvp(argv[0], argv);

	fprintf(stderr, "%s: %s \n", argv[0], STRERROR);
	globfree(&file_list);
	destroy_argv(&argv, argc, first_opt);

	return -1;
}
