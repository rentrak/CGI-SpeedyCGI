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
static char *comm_init(unsigned short port, int *s, int *e);
static char *communicate(
    int s, int e, char *envp[], char *scr_argv[], int secret_word
);
static int sendenv_size(char *envp[], char *scr_argv[]);
static void sendenv_fill(
    char *buf, char *envp[], char *scr_argv[], int secret_word
);
static void add_strings(char **bp, char **p);
static int array_bufsize(char **p);


int main(int argc, char *argv[], char *envp[]) {
    char *s, *errmsg = NULL, **perl_argv, **scr_argv;
    char **my_argv = argv;
    struct timeval start_time;

    /* Get start time */
    gettimeofday(&start_time, NULL);

    /* Two ways we can be called. */
    /* */
    /*  - "speedyhandler" - use PATH_TRANSLATED for the cgi script */
    /*    name and juggle some environment variables. */
    /* */
    /*  - "speedy" - cgi script name is in argv */
    /* */
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
	opts, NUMOPTS, my_argv, envp, &scr_argv, &perl_argv
    );
    if (!errmsg && !scr_argv[0]) errmsg = "Missing command filename";

#ifdef OPTS_DEBUG
    opts_debug(opts, NUMOPTS, scr_argv, perl_argv);
#endif

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
    if ((retval = speedy_q_init(&q, opts, scr_argv[0], start_time)))
	return retval;

    /* Get proc out of the queue and init communication */
    retval = speedy_q_get(&q, &pinfo);
    if (!retval) retval = comm_init(pinfo.port, &s, &e);

    /* If failed, create a new process. */
    if (retval) {
	retval = speedy_start_perl(&q, perl_argv, opts, &pinfo);
	if (!retval) retval = comm_init(pinfo.port, &s, &e);
	if (retval) goto doit_done;
    }

    /* Communicate */
    retval = communicate(s, e, envp, scr_argv, q.secret_word);

doit_done:
    speedy_q_free(&q);
    return retval;
}

static char *comm_init(unsigned short port, int *s, int *e) {
    CHKERR2(*s, speedy_connect(port), "connect failed");
    if ((*e = speedy_connect(port)) == -1) {
	close(*s);
	return "connect failed";
    }
    return NULL;
}


static char *communicate(
    int s, int e, char *envp[], char *scr_argv[], int secret_word
)
{
#   define NUMCBUFS 3
    CopyBuf ibuf, obuf, ebuf;
    CopyBuf *cbs[NUMCBUFS] = {&ibuf, &obuf, &ebuf};
    PollInfo pi;
    char *ibuf_buf;
    int env_sz, ibuf_sz, did_shutdown;

    /* Find out the space we need for IBUF to fit the environment */
    env_sz = sendenv_size(envp, scr_argv);
    ibuf_sz = OVAL_INT(opts[OPT_BUFSZ_POST]);
    if (env_sz + 512 > ibuf_sz) ibuf_sz = env_sz + 512;

    /* Alloc buf, and fill in with env data */
    New(123, ibuf_buf, ibuf_sz, char);
    sendenv_fill(ibuf_buf, envp, scr_argv, secret_word);

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

    /* Copy streams */
    for (did_shutdown = 0; !COPYDONE(obuf) || !COPYDONE(ebuf);) {
	int i;

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

	    if (speedy_poll_isset(&pi, b->rdfd, SPEEDY_POLLIN))
		speedy_cb_read(b);
	    if (speedy_poll_isset(&pi, b->wrfd, SPEEDY_POLLOUT))
		speedy_cb_write(b);
	}
    }

    /* Cleanup and return */
    speedy_poll_free(&pi);
    speedy_cb_free(&ibuf);
    speedy_cb_free(&obuf);
    speedy_cb_free(&ebuf);
    close(s);
    close(e);
    close(0);
    close(1);
    close(2);
    return NULL;
}

/* Find out how big a buffer we need for sending environment.*/
static int sendenv_size(char *envp[], char *scr_argv[]) {
    return sizeof(int) +			/* secret-word */
	   1 + array_bufsize(envp) +		/* env */
	   1 + array_bufsize(scr_argv+1) +	/* argv */
	   1;					/* terminator */
}

/* Return the size of buffer needed to send array. */
static int array_bufsize(char **p) {
    int l, sz = sizeof(int);	/* bytes for terminator */
    for (; *p; ++p) {
	if ((l = strlen(*p))) sz += sizeof(int) + l;
    }
    return sz;
}

#define ADD(s,d,l,t) Copy(s,d,l,t); (d) += ((l)*sizeof(t))

/* Fill in the environment to send. */
static void sendenv_fill(
    char *buf, char *envp[], char *scr_argv[], int secret_word
)
{
    /* Add secret word */
    ADD(&secret_word, buf, 1, int);

    /* Put env into buffer */
    *buf++ = 'E';
    add_strings(&buf, envp);

    /* Put argv into buffer */
    *buf++ = 'A';
    add_strings(&buf, scr_argv+1);

    /* End */
    *buf++ = ' ';
}

/* Copy a block of strings into the buffer,  */
static void add_strings(char **bp, char **p) {
    int l;

    /* Put in length plus string, without terminator */
    for (; *p; ++p) {
	if ((l = strlen(*p))) {
	    ADD(&l, *bp, 1, int);
	    ADD(*p, *bp, l, char);
	}
    }

    /* Terminate with zero-length string */
    l = 0;
    ADD(&l, *bp, 1, int);
}
