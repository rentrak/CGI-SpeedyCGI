/*
 * Copyright (C) 1999 Daemon Consulting, Inc.  All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *      
 * 3. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Daemon Consulting Inc."
 *
 * THIS SOFTWARE IS PROVIDED BY DAEMON CONSULTIN INC``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL DAEMON CONSULTING INC OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* Speedycgi - Sam Horrocks, Daemon Consulting */

#define SPEEDY_C_SOURCE
#include "speedy.h"


static char *handler_env(char *envp[]);
static char *doit(
    char *perl_argv[], char *envp[], char *scr_argv[],
    struct timeval *start_time
);
static char *communicate(
    int s, int e, char *envp[], char *scr_argv[], int secret_word
);


/*
 * Functions that interface libspeedy to libperl
 */
static void ls_memcpy(void *d, void *s, int n) { Copy(s, d, n, char); }
static void ls_memmove(void *d, void *s, int n) { Move(s, d, n, char); }
static void ls_bzero(void *s, int n) { Zero(s, n, char); }
static void *ls_malloc(int n) { void *s; New(123, s, n, char); return s; }
static void ls_free(void *s) { Safefree(s); }

/*
 * This global var is used by the library to find the above functions
 */
LS_funcs speedy_libfuncs = {
    ls_memcpy,
    ls_memmove,
    ls_bzero,
    ls_malloc,
    ls_free,
};

int main(int argc, char *argv[], char *envp[]) {
    char *s, *errmsg = NULL, **perl_argv, **scr_argv;
    char **my_argv = argv;
    struct timeval start_time;

    /* Get start time */
    gettimeofday(&start_time, NULL);

    /* We can be called as:
     *
     *  - "speedyhandler" - use PATH_TRANSLATED for the cgi script
     *    name and juggle some environment variables.
     *
     *  - "speedybackend" - run the backend only.
     *
     *  - "speedy" - cgi script name is in argv
     *
     */
    if ((s = strrchr(argv[0], '/'))) {
	++s;
    } else {
	s = argv[0];
    }
    if (strcmp(s, "speedyhandler") == 0) {
	char *cmd = handler_env(envp);
	if (!cmd) {
	    errmsg = "Missing PATH_TRANSLATED environment variable";
	}
	/* Append cmd to my_argv */
	New(123, my_argv, argc+2, char*);
	Copy(argv, my_argv, argc, char*);
	my_argv[argc] = cmd;
	my_argv[argc+1] = NULL;
    }

    /* Get our options */
    speedy_getopt(
	opts, my_argv, envp, &scr_argv, &perl_argv
    );
    if (!errmsg && !scr_argv[0]) errmsg = "Missing command filename";

#ifdef OPTS_DEBUG
    opts_debug(opts, scr_argv, perl_argv);
#endif

    /* If called as "speedybackend", then go directly to the perl backend */
    if (strcmp(s, "speedybackend") == 0) {
	char *err;
	SpeedyQueue q;
	PersistInfo pinfo;

	err = speedy_q_init(
	    &q, OVAL_STR(opts[OPT_TMPBASE]), scr_argv[0], &start_time, NULL
	);
	speedy_fillin_pinfo(&pinfo, 3);
	speedy_exec_perl(&q, scr_argv[0], perl_argv, opts, &pinfo, 3, envp);
	exit(1);
    }

    /* Communicate with the cgi. */
    if (!errmsg) {
	errmsg = doit(perl_argv, envp, scr_argv, &start_time);
    }

    /* See if a failure. */
    if (errmsg) {
	if (errno) {
	    fprintf(stderr, "%s: %s\n", errmsg, Strerror(errno));
	} else {
	    fprintf(stderr, "%s\n", errmsg);
	}
	exit(1);
    }
    return 0;
}

/* Fix up environment for handler mode, and return the script name */

#define PINFO	"PATH_INFO="
#define PTRANS	"PATH_TRANSLATED="
#define SNAME	"SCRIPT_NAME="

/* Set up environment when we are acting as a handler. */
static char *handler_env(char *p[]) {
    char **path_trans = NULL, **path_info = NULL, **script_name = NULL;
    char *retval;

    /* Traverse the environment to find locations of variables and end-of-list
     */
    for (; *p; ++p) {
	if (!path_info && strncmp(*p, PINFO, sizeof(PINFO)-1) == 0) {
	    path_info = p;
	}
	else if (!path_trans && strncmp(*p, PTRANS, sizeof(PTRANS)-1) == 0) {
	    path_trans = p;
	}
	else if (!script_name && strncmp(*p, SNAME, sizeof(SNAME)-1) == 0) {
	    script_name = p;
	}
    }

    /* Save a copy of PATH_TRANSLATED to return as function value */
    retval = path_trans ? (*path_trans + (sizeof(PTRANS)-1)) : NULL;

    /* Copy PATH_INFO to SCRIPT_NAME */
    if (path_info) {
	char *newval;

	int l = strlen(*path_info + (sizeof(PINFO)-1));
	New(123, newval, sizeof(SNAME)+l, char);

	Copy(SNAME, newval, sizeof(SNAME)-1, char);
	Copy(*path_info+(sizeof(PINFO)-1), newval+(sizeof(SNAME)-1), l+1, char);

	if (script_name) {
	    *script_name = newval;	/* Replace old script_name */
	} else {
	    *p++ = newval;		/* Append to list */
	}
    }

    /* Delete PATH_INFO */
    if (path_info) {
	*path_info = *p--;
    }

    /* Delete PATH_TRANSLATED */
    if (path_trans) {
	*path_trans = *p--;
    }

    /* Terminate the list. */
    *p = NULL;

    return retval;
}

static char *doit(
    char *perl_argv[], char *envp[], char *scr_argv[],
    struct timeval *start_time
)
{
    char *retval;
    SpeedyQueue q;
    PersistInfo pinfo;
    int s, e;

    /* Create new queue */
    retval = speedy_q_init(
        &q, OVAL_STR(opts[OPT_TMPBASE]), scr_argv[0], start_time, NULL
    );
    if (retval) return retval;

    /* Connect up with client, if it's already there */
    retval = speedy_getclient(&q, &s, &e);

    /* If failed, create a new process. */
    if (retval) {
	retval = speedy_spawn_perl(
	    &q, scr_argv[0], perl_argv, opts, &pinfo, envp
	);
	if (!retval) retval = speedy_comm_init(pinfo.port, &s, &e);
	if (retval) goto doit_done;
    }

    /* Communicate */
    retval = communicate(s, e, envp, scr_argv, q.secret_word);

doit_done:
    speedy_q_free(&q);
    return retval;
}


static char *communicate(
    int s, int e, char *envp[], char *scr_argv[], int secret_word
)
{
    PollInfo pi;
    char *ibuf_buf;
    int env_sz, ibuf_sz, did_shutdown, i;
#   define NUMCBUFS 3
    CopyBuf ibuf, obuf, ebuf, *cbs[NUMCBUFS];
    cbs[0] = &ibuf; cbs[1] = &obuf; cbs[2] = &ebuf;

    /* Find out the space we need for IBUF to fit the environment */
    env_sz = speedy_sendenv_size(envp, scr_argv);
    ibuf_sz = OVAL_INT(opts[OPT_BUFSZ_POST]);
    if (env_sz + 512 > ibuf_sz) ibuf_sz = env_sz + 512;

    /* Alloc buf, and fill in with env data */
    New(123, ibuf_buf, ibuf_sz, char);
    speedy_sendenv_fill(ibuf_buf, envp, scr_argv, secret_word);

    speedy_poll_init(&pi, s > e ? s : e);

    /* Allocate buffers for copying below: */
    /*	fd0 -> ibuf -> s	*/
    /*	s   -> obuf -> fd1	*/
    /*	e   -> ebuf -> fd2	*/
    speedy_cb_alloc(
	&ibuf,
	ibuf_sz,
	0,
	s,
	ibuf_buf,
	env_sz
    );
    speedy_cb_alloc(
	&obuf,
	OVAL_INT(opts[OPT_BUFSZ_GET]),
	s,
	1,
	NULL,
	0
    );
    speedy_cb_alloc(
	&ebuf,
	512,
	e,
	2,
	NULL,
	0
    );

    /* Non-blocking I/O */
    fcntl(0, F_SETFL, O_NONBLOCK);
    fcntl(1, F_SETFL, O_NONBLOCK);
    fcntl(2, F_SETFL, O_NONBLOCK);
    fcntl(e, F_SETFL, O_NONBLOCK);
    fcntl(s, F_SETFL, O_NONBLOCK);

    /* Poll/select may not wakeup on intial eof, so read now
     * (this is tested in initial_eof test #1)
     */
    for (i = 0; i < NUMCBUFS; ++i) {
	speedy_cb_read(cbs[i]);
    }

    /* Copy streams */
    for (did_shutdown = 0; !COPYDONE(obuf) || !COPYDONE(ebuf);) {

	/* See if we should shutdown the CGI's stdin. */
	if (!did_shutdown && COPYDONE(ibuf)) {
	    shutdown(s, 1);
	    did_shutdown = 1;
	}

	/* Reset events */
	speedy_poll_reset(&pi);
	
	/* Set read/write events */
	for (i = 0; i < NUMCBUFS; ++i) {
	    CopyBuf *b = cbs[i];
	    if ( CANREAD(*b)) speedy_poll_set(&pi, b->rdfd, SPEEDY_POLLIN);
	    if (CANWRITE(*b)) speedy_poll_set(&pi, b->wrfd, SPEEDY_POLLOUT);
	}

	/* Poll... */
	CHKERR(speedy_poll_wait(&pi, -1), "poll");

	/* Do reads/writes */
	for (i = 0; i < NUMCBUFS; ++i) {
	    CopyBuf *b = cbs[i];
	    int did_read = 0;

	    if (speedy_poll_isset(&pi, b->rdfd, SPEEDY_POLLIN)) {
		speedy_cb_read(b);
		did_read = 1;
	    }
	    /*
	     * Attempt write now if we did a read.  Slightly more efficient
	     * and on SGI if we are run with >/dev/null,  select won't
	     * initially wakeup (this is tested in initial_eof test #2)
	     */
	    if ((did_read && CANWRITE(*b)) ||
	        speedy_poll_isset(&pi, b->wrfd, SPEEDY_POLLOUT))
	    {
		speedy_cb_write(b);
	    }
	}
    }

    /* SGI's /dev/tty goes crazy unless we turn of non-blocking I/O. */
    fcntl(1, F_SETFL, 0); close(1);
    fcntl(2, F_SETFL, 0); close(2);
    fcntl(0, F_SETFL, 0); close(0);
    close(s);
    close(e);

    /* Cleanup and return */
    speedy_poll_free(&pi);
    speedy_cb_free(&ibuf);
    speedy_cb_free(&obuf);
    speedy_cb_free(&ebuf);
    return NULL;
}
