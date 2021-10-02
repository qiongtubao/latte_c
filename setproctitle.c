#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "setproctitle.h"
extern char **environ;

static struct {
	/* original value */
	const char *arg0;

	/* title space available */
	char *base, *end;

	 /* pointer to original nul character within base */
	char *nul;

	_Bool reset;
	int error;
} SPT;

/*
 * For discussion on the portability of the various methods, see
 * http://lists.freebsd.org/pipermail/freebsd-stable/2008-June/043136.html
 */
static int spt_clearenv(void) {
#if __GLIBC__
	clearenv();

	return 0;
#else
	extern char **environ;
	static char **tmp;

	if (!(tmp = malloc(sizeof *tmp)))
		return errno;

	tmp[0]  = NULL;
	environ = tmp;

	return 0;
#endif
} /* spt_clearenv() */

static int spt_copyenv(int envc, char *oldenv[]) {
    extern char **environ;
	char **envcopy = NULL;
	char *eq;
	int i, error;
	int envsize;

	if (environ != oldenv)
		return 0;

	/* Copy environ into envcopy before clearing it. Shallow copy is
	 * enough as clearenv() only clears the environ array.
	 */
	envsize = (envc + 1) * sizeof(char *);
	envcopy = malloc(envsize);
	if (!envcopy)
		return ENOMEM;
	memcpy(envcopy, oldenv, envsize);

	/* Note that the state after clearenv() failure is undefined, but we'll
	 * just assume an error means it was left unchanged.
	 */
	if ((error = spt_clearenv())) {
		environ = oldenv;
		free(envcopy);
		return error;
	}

	/* Set environ from envcopy */
	for (i = 0; envcopy[i]; i++) {
		if (!(eq = strchr(envcopy[i], '=')))
			continue;

		*eq = '\0';
		error = (0 != setenv(envcopy[i], eq + 1, 1))? errno : 0;
		*eq = '=';

		/* On error, do our best to restore state */
		if (error) {
#ifdef HAVE_CLEARENV
			/* We don't assume it is safe to free environ, so we
			 * may leak it. As clearenv() was shallow using envcopy
			 * here is safe.
			 */
			environ = envcopy;
#else
			free(envcopy);
			free(environ);  /* Safe to free, we have just alloc'd it */
			environ = oldenv;
#endif
			return error;
		}
	}

	free(envcopy);
	return 0;
}

static int spt_copyargs(int argc, char *argv[]) {
	char *tmp;
	int i;

	for (i = 1; i < argc || (i >= argc && argv[i]); i++) {
		if (!argv[i])
			continue;

		if (!(tmp = strdup(argv[i])))
			return errno;

		argv[i] = tmp;
	}

	return 0;
} /* spt_copyargs() */

/* Initialize and populate SPT to allow a future setproctitle()
 * call.
 *
 * As setproctitle() basically needs to overwrite argv[0], we're
 * trying to determine what is the largest contiguous block
 * starting at argv[0] we can use for this purpose.
 *
 * As this range will overwrite some or all of the argv and environ
 * strings, a deep copy of these two arrays is performed.
 */
void spt_init(int argc, char *argv[]) {
    char **envp = environ;
    char *base, *end, *nul, *tmp;
    int i, error, envc;
    if(!(base = argv[0])) {
        return;
    }
    /* We start with end pointing at the end of argv[0] */
    nul = &base[strlen(base)];
    end = nul + 1;
    /* Attempt to extend end as far as we can, while making sure
	 * that the range between base and end is only allocated to
	 * argv, or anything that immediately follows argv (presumably
	 * envp).
	 */
	for (i = 0; i < argc || (i >= argc && argv[i]); i++) {
		if (!argv[i] || argv[i] < end)
			continue;

		if (end >= argv[i] && end <= argv[i] + strlen(argv[i]))
			end = argv[i] + strlen(argv[i]) + 1;
	}
    envc = i;

	/* We're going to deep copy argv[], but argv[0] will still point to
	 * the old memory for the purpose of updating the title so we need
	 * to keep the original value elsewhere.
	 */
	if (!(SPT.arg0 = strdup(argv[0])))
		goto syerr;

#if __GLIBC__
	if (!(tmp = strdup(program_invocation_name)))
		goto syerr;

	program_invocation_name = tmp;

	if (!(tmp = strdup(program_invocation_short_name)))
		goto syerr;

	program_invocation_short_name = tmp;
#elif __APPLE__
	if (!(tmp = strdup(getprogname())))
		goto syerr;

	setprogname(tmp);
#endif

    /* Now make a full deep copy of the environment and argv[] */
	if ((error = spt_copyenv(envc, envp)))
		goto error;

	if ((error = spt_copyargs(argc, argv)))
		goto error;

	SPT.nul  = nul;
	SPT.base = base;
	SPT.end  = end;

	return;
syerr:
	error = errno;
error:
	SPT.error = error;
}
