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
static char *comm_init(
    unsigned short port, char *envp[], char *scr_argv[], int secret_word, int *s
);
static char *communicate(int s);
static char *sendenv(
    int s, char *envp[], char *scr_argv[], int secret_word, int doexit
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

    /* Traverse the environment to find locations of variables and end-of-list */
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
    int s;

    /* Create new queue */
    if ((retval = speedy_q_init(&q, opts, scr_argv[0], start_time)))
	return retval;

    /* Get proc out of the queue and init communication */
    retval = speedy_q_get(&q, &pinfo);
    if (!retval) 
	 retval = comm_init(pinfo.port, envp, scr_argv, q.secret_word, &s);

    /* If failed, create a new process. */
    if (retval) {
	retval = speedy_start_perl(&q, perl_argv, opts, &pinfo);
	if (!retval)
	    retval = comm_init(pinfo.port, envp, scr_argv, q.secret_word, &s);
	if (retval) goto doit_done;
    }

    /* Communicate */
    retval = communicate(s);

doit_done:
    speedy_q_free(&q);
    return retval;
}

static char *comm_init(
    unsigned short port, char *envp[], char *scr_argv[], int secret_word, int *s
)
{
    char *retval;
    /* Connect up */
    CHKERR2(*s, speedy_connect(port), "connect failed");

    /* Send over environment. */
    if ((retval = sendenv(*s, envp, scr_argv, secret_word, 0)))
	close(*s);

    return retval;
}

static char *communicate(int s) {
    CopyBuf ibuf, obuf;
    int did_shutdown;
    struct pollfd fds[3];

    /*	fd0 -> ibuf -> s */
    /*	s   -> obuf -> fd1 */
    const int ibuf_rd_idx = 0, ibuf_wr_idx = 2;
    const int obuf_rd_idx = 2, obuf_wr_idx = 1;
    fds[0].fd = 0;
    fds[1].fd = 1;
    fds[2].fd = s;

    /* Allocate buffers for copying below: */
    speedy_cb_alloc(
	&ibuf,
	OVAL_INT(opts[OPT_BUFSZ_POST]),
	fds[ibuf_rd_idx].fd,
	fds[ibuf_wr_idx].fd
    );
    speedy_cb_alloc(
	&obuf,
	OVAL_INT(opts[OPT_BUFSZ_GET]),
	fds[obuf_rd_idx].fd,
	fds[obuf_wr_idx].fd
    );


    /* Do multiplexing - continue until done sending stdout only. */
    /* CGI may not take data from its stdin. */
    for (did_shutdown = 0; !COPYDONE(obuf);) {

	/* See if we should shutdown the CGI's stdin. */
	if (!did_shutdown && COPYDONE(ibuf)) {
	    shutdown(s, 1);
	    did_shutdown = 1;
	}

	/* Reset events */
	fds[0].events = fds[1].events = fds[2].events = 0;
	
	/* Set read events */
	if (CANREAD(ibuf))  fds[ibuf_rd_idx].events |= POLLIN;
	if (CANREAD(obuf))  fds[obuf_rd_idx].events |= POLLIN;

	/* Set write events */
	if (CANWRITE(ibuf)) fds[ibuf_wr_idx].events |= POLLOUT;
	if (CANWRITE(obuf)) fds[obuf_wr_idx].events |= POLLOUT;

	/* Poll... */
	CHKERR(poll(fds, 3, -1), "poll");

	/* Do reads */
	if (fds[ibuf_rd_idx].revents & POLLIN)  speedy_cb_read(&ibuf);
	if (fds[obuf_rd_idx].revents & POLLIN)  speedy_cb_read(&obuf);

	/* Do writes */
	if (fds[ibuf_wr_idx].revents & POLLOUT) speedy_cb_write(&ibuf);
	if (fds[obuf_wr_idx].revents & POLLOUT) speedy_cb_write(&obuf);
    }

    /* Cleanup and return */
    speedy_cb_free(&ibuf);
    speedy_cb_free(&obuf);
    close(s);
    close(0);
    close(1);
    return NULL;
}

#define ADD(s,d,l,t) Copy(s,d,l,t); (d) += ((l)*sizeof(t))

static char *sendenv(
    int s, char *envp[], char *scr_argv[], int secret_word, int doexit
)
{
    char *buf, *bp, *retval = NULL;
    int sz, nwritten, n;

    /* Find out how big a buffer we need */
    sz = sizeof(int) + 1;	/* secret-word plus final command */
    if (!doexit) {
	sz += 1+array_bufsize(envp) + 1+array_bufsize(scr_argv+1);
    }

    /* Allocate buffer */
    New(123, buf, sz, char);
    bp = buf;

    /* Add secret word */
    ADD(&secret_word, bp, 1, int);

    /* Put env into buffer */
    *bp++ = 'E';
    add_strings(&bp, envp);

    /* Put argv into buffer */
    *bp++ = 'A';
    add_strings(&bp, scr_argv+1);

    /* End */
    *bp++ = ' ';

    /* Write */
    for (nwritten = 0; nwritten < sz; nwritten += n) {
	if ((n = write(s, buf+nwritten, sz-nwritten)) == -1) {
	    retval = "env protocol failure";
	    goto sendenv_done;
	}
    }

    /* Cleanup and exit */
sendenv_done:
    Safefree(buf);
    return retval;
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

/* Return the size of buffer needed to send array. */
static int array_bufsize(char **p) {
    int l, sz = sizeof(int);	/* bytes for terminator */
    for (; *p; ++p) {
	if ((l = strlen(*p))) sz += sizeof(int) + l;
    }
    return sz;
}
