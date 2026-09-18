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

#define ARGV_NULL_I argc - 1
#define ARGV_OUTPATH_I argc - 2
#define ARGV_OFLAG_I argc - 3
#define ARGV_PATH_I argc - 4

#define SLASH 1

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
	const int maxLenSz =
		strlen(data->builddir) + SLASH +
		strlen(data->label) + SLASH +
		NAME_MAX +
		1 /* NULL */;
	(*ret)[*ARGV_OUTPATH_I] = malloc(maxLenSz);
	if(!*ret)
	{
		for(int i = *first_opt; i < *ARGV_OFLAG_I; ++i)
			free((*ret)[i]);
		free(*ret);
		ERR(MALLOC, "Cant allocate outpath");
	}
	snprintf((*ret)[*ARGV_OUTPATH_I], maxLenSz, "%s/%s/", data->builddir, data->label);
	if(access((*ret)[*ARGV_OUTPATH_I], F_OK))
		FAIL_CODE(mkdir((*ret)[*ARGV_OUTPATH_I], 0755), -errno, "Cant create directory \"%s\"", (*ret)[*ARGV_OUTPATH_I]);

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

inline static int
has_changed(const char *const source_path, const char *const object_path)
{
	if(access(object_path, F_OK) != 0)
		return 1;

	struct stat st;
	time_t source_modtime;
	time_t object_modtime;

	FAIL_CODE(stat(source_path, &st) < 0, CANT_OPEN, "Cant open stat struct for path \"%s\"", source_path);
	source_modtime = st.st_mtim.tv_sec;

	FAIL_CODE(stat(object_path, &st) < 0, CANT_OPEN, "Cant open stat struct for path \"%s\"", object_path);
	object_modtime = st.st_mtim.tv_sec;
	
	return source_modtime > object_modtime;
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
compile_srcdir(const char *const srcdir, const struct data_t *data, char **argv, const int argc, int compile_anyways)
{
	const DIR *dir = opendir(srcdir);
	if(!dir)
		ERR(CANT_OPEN, "Cant open srcdir \"%s\"", srcdir);

	glob_t file_list;
	char pattern[
		strlen(srcdir) + SLASH +
		sizeof("*.") +
		strlen(data->ext)];

	snprintf(pattern, sizeof(pattern), "%s/*.%s", srcdir, data->ext);

	/* TODO maybe store rc? */
	if(glob(pattern, GLOB_NOSORT | GLOB_ERR, NULL, &file_list))
	{
		closedir(dir);
		ERR(NOMATCH, "Pattern was \"%s\"", pattern);
	}

	/* loop through returned paths */
	const char *outpath_end = strrchr(argv[ARGV_OUTPATH_I], '/') + 1;
	for(unsigned int i = 0; i < file_list.gl_pathc; ++i)
	{
		argv[ARGV_PATH_I] = file_list.gl_pathv[i];

		struct stat st;
		if(stat(argv[ARGV_PATH_I], &st) < 0)
		{
			ERR_NR(CANT_OPEN, "Cant get stat struct for file \"%s\"", argv[ARGV_PATH_I]);
			goto err;
		}
		snprintf(outpath_end, NAME_MAX, "%s.o", basename(argv[ARGV_PATH_I]));

		int changed;
		FAIL_GOTO((changed = has_changed(argv[ARGV_PATH_I], argv[ARGV_OUTPATH_I])) < 0, err);
		if(!compile_anyways && changed == 0)
			continue;

		if(exec_cc(argv) < 0)
			goto err;
	}

	const int dir_fd = dirfd(dir);
	FAIL(fsync(dir_fd));

	closedir(dir);
	globfree(&file_list);

	return 0;

err:
	closedir(dir);
	globfree(&file_list);
	return -1;
}

int
compile(struct data_t *data, const int compile_anyways)
{
	if(sem_init(&sem, 0, data->threads) < 0)
	{
		ERR(CANT_OPEN, "Cant open semaphore");
	}

	if(!data->label)
		data->label = "";

	int argc;
	int first_opt;
	char **argv;
	FAIL(make_argv(&argv, data, &argc, &first_opt) < 0);

	const struct sigaction sig = { .sa_handler = sigchld_handler };
	sigaction(SIGCHLD, &sig, NULL);

	/* loop through specified srcdirs */
	for(char *srcdir = strtok(data->srcdirs, " "); srcdir; srcdir = strtok(NULL, " "))
	{
		if(compile_srcdir(srcdir, data, argv, argc, compile_anyways) < 0)
		{
			destroy_argv(&argv, argc, first_opt);
			return -1;
		}
		waitpid(-1, NULL, 0);
	}

	/* move eveything before first_opt by one to overwrite '-c' */
	memmove(argv + 1, argv, sizeof(char*) * (first_opt - 1));
	argv++; argc--; first_opt--;

	/* Fuck it, simplier than having to deal with what to do with PATH */
	argv[ARGV_PATH_I] = "-Wl,--as-needed";

	glob_t file_list;
	char pattern[
		strlen(data->builddir) + SLASH +
		strlen(data->label) + SLASH +
		sizeof("*.o")];

	snprintf(pattern, sizeof(pattern), "%s/%s/*.o", data->builddir, data->label);
	if(glob(pattern, GLOB_NOSORT | GLOB_ERR, NULL, &file_list) != 0)
	{
		destroy_argv(&argv, argc, first_opt);
		globfree(&file_list);
		ERR(NOMATCH, "Pattern was \"%s\"", pattern);
	}

	// argv = realloc(argv, sizeof(char*) * (argc + file_list.gl_pathc + 1)); // argv[<first_opt] not malloc'd
	int bytes = sizeof(char*) * (argc + file_list.gl_pathc + 1);
	char **argv_cc = malloc(bytes);
	memcpy(argv_cc, argv, sizeof(char*) * argc);
	argv = argv_cc;

	snprintf(strrchr(argv[ARGV_OUTPATH_I], '/') + 1, NAME_MAX, "%s", data->name);

	memcpy(argv + ARGV_NULL_I, file_list.gl_pathv, sizeof(char*) * (file_list.gl_pathc + 1));
	argc += file_list.gl_pathc;

	for(int i = 0; i < ARGV_NULL_I; ++i)
		printf("%s ", argv[i]);
	printf("\n");

	execvp(argv[0], argv);

	ERR_NR(-errno, "Cant exec \"%s\"", argv[0]);
	globfree(&file_list);
	destroy_argv(&argv, argc, first_opt);

	return -1;
}
