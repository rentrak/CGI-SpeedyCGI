/*
 * Copyright (C) 2001  Daemon Consulting Inc.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

/*
 * Speedy Frontend Program
 */

#include "speedy.h"

#define NUMFDS	3
#define CB_IN	(cb[0])
#define CB_OUT	(cb[1])
#define CB_ERR	(cb[2])

#define MY_COPYDONE(b) \
    (COPYDONE(b) && (got_stdout || &(b) != &CB_OUT || BUF_EOF(b)))
#define MY_CANREAD(b) \
    (CANREAD(b) || (!got_stdout && &(b) == &CB_OUT && !BUF_EOF(b)))

static const int fd_flags[NUMFDS] = {O_RDONLY, O_WRONLY, O_WRONLY};

static void die_on_sigpipe() {
    signal(SIGPIPE, SIG_DFL);
    speedy_util_kill(getpid(), SIGPIPE);
    speedy_util_exit(1,0);
}

/*
 * When profiling, only call speedy_opt_init once
 */
#ifdef SPEEDY_PROFILING
static int did_opt_init, profile_runs;
#define DO_OPT_INIT if (!did_opt_init++) speedy_opt_init
#else
#define DO_OPT_INIT speedy_opt_init
#endif

static void doit(const char * const *argv)
{
    extern char **environ;
    PollInfo pi;
    char *ibuf_buf;
    int env_sz, ibuf_sz, did_shutdown, i, got_stdout = 0;
    int s, e, fd_open[NUMFDS], cb_closed[NUMFDS];
    CopyBuf cb[NUMFDS];

    signal(SIGPIPE, SIG_IGN);

    /* Find out if fd's 0-2 are open.  Also make them non-blocking */
    for (i = 0; i < NUMFDS; ++i) {
	fd_open[i] =
	    fcntl(i, F_SETFL, fd_flags[i] | O_NONBLOCK) != -1 || errno != EBADF;
    }

    /* Initialize options */
    DO_OPT_INIT(argv, (const char * const *)environ);

#   ifdef IAMSUID
	if (speedy_util_geteuid() == 0) {
	    int new_uid;

	    /* Set group-id */
	    if (speedy_script_getstat()->st_mode & S_ISGID) {
		if (setegid(speedy_script_getstat()->st_gid) == -1)
		    speedy_util_die("setegid");
	    }

	    /* Must set euid to something - either the script owner
	     * or the real-uid
	     */
	    if (speedy_script_getstat()->st_mode & S_ISUID) {
		new_uid = speedy_script_getstat()->st_uid;
	    } else {
		new_uid = speedy_util_getuid();
	    }
	    if (speedy_util_seteuid(new_uid) == -1)
		speedy_util_die("seteuid");
	}
#   endif

    /* Create buffer with env/argv data to send */
    ibuf_buf = speedy_frontend_mkenv(
	(const char * const *)environ, speedy_opt_script_argv(),
	OPTVAL_BUFSIZPOST, 512,
	&env_sz, &ibuf_sz, 0
    );

    /* Connect up with a backend */
    speedy_frontend_connect(&s, &e);

    /* Non-blocking I/O on sockets */
    fcntl(e, F_SETFL, O_RDWR|O_NONBLOCK);
    fcntl(s, F_SETFL, O_RDWR|O_NONBLOCK);

    /* Allocate buffers for copying below: */
    /*	fd0 -> cb[0] -> s	*/
    /*	s   -> cb[1] -> fd1	*/
    /*	e   -> cb[2] -> fd2	*/
    speedy_cb_alloc(
	&CB_IN,
	ibuf_sz,
	0,
	s,
	ibuf_buf,
	env_sz
    );
    speedy_cb_alloc(
	&CB_OUT,
	OPTVAL_BUFSIZGET,
	s,
	1,
	NULL,
	0
    );
    speedy_cb_alloc(
	&CB_ERR,
	512,
	e,
	2,
	NULL,
	0
    );

    /* Disable i/o on any fd's that are not open */
    if (!fd_open[0])
	BUF_SETEOF(CB_IN);
    if (!fd_open[1])
	BUF_SET_WRITE_ERR(CB_OUT, EBADF);
    if (!fd_open[2])
	BUF_SET_WRITE_ERR(CB_ERR, EBADF);

    /* Poll/select may not wakeup on intial eof, so set for initial read here.
     * (this is tested in initial_eof test #1)
     */
    speedy_poll_init(&pi, max(s,e));
    speedy_poll_reset(&pi);
    for (i = 0; i < NUMFDS; ++i) {
	if (MY_CANREAD(cb[i]))
	    speedy_poll_set(&pi, cb[i].rdfd, SPEEDY_POLLIN);
    }

    /* Try to write our env/argv without dropping into select */
    if (CANWRITE(CB_IN))
	speedy_poll_set(&pi, CB_IN.wrfd, SPEEDY_POLLOUT);

    for (i = 0; i < NUMFDS; ++i)
	cb_closed[i] = 0;

    /* Copy streams */
    for (did_shutdown = 0;;) {

	/* Do reads/writes */
	for (i = 0; i < NUMFDS; ++i) {
	    CopyBuf *b = cb + i;
	    int do_read  = MY_CANREAD(*b) &&
			   speedy_poll_isset(&pi, b->rdfd, SPEEDY_POLLIN);
	    int do_write = CANWRITE(*b) &&
			   speedy_poll_isset(&pi, b->wrfd, SPEEDY_POLLOUT);

	    while (do_read || do_write) {

		if (do_read) {
		    int sz = BUF_SZ(*b);
		    speedy_cb_read(b);
		    if (!got_stdout && b == &CB_OUT && BUF_SZ(*b) > sz) {
			got_stdout = 1;
			speedy_frontend_proto2(e, speedy_cb_shift(b));
		    }
		    if (CANWRITE(*b) && (BUF_EOF(*b) || BUF_SZ(*b) > sz))
			do_write = 1;
		    do_read = 0;
		}

		/* Attempt write now if we did a read.  Slightly more efficient
		 * and on SGI if we are run with >/dev/null,  select won't
		 * initially wakeup (this is tested in initial_eof test #2)
		 */
		if (do_write) {
		    int sz = BUF_SZ(*b);
		    speedy_cb_write(b);

		    /* We should die on EPIPE's, but only for stdout/stderr */
		    if (i > 0 && BUF_GET_WRITE_ERR(*b) == EPIPE)
			die_on_sigpipe();

		    if (MY_CANREAD(*b) && BUF_SZ(*b) < sz)
			do_read = 1;
		    do_write = 0;
		}

		/* Do closes now instead of later, so we have another
		 * chance to read/write before dropping into select.
		 */
		if (!cb_closed[i] && MY_COPYDONE(*b)) {
		    cb_closed[i] = 1;

		    if (fd_open[i]) {
			fcntl(i, F_SETFL, fd_flags[i]);
#ifndef SPEEDY_PROFILING
			close(i);
#endif
			fd_open[i] = 0;
		    }
		    if (b == &CB_IN)
			shutdown(CB_IN.wrfd, 1);
		}
	    }
	}

	if (MY_COPYDONE(CB_OUT) && MY_COPYDONE(CB_ERR))
	    break;

	/* Reset events */
	speedy_poll_reset(&pi);

	/* Set read/write events */
	for (i = 0; i < NUMFDS; ++i) {
	    CopyBuf *b = cb + i;
	    if (MY_CANREAD(*b))
		speedy_poll_set(&pi, b->rdfd, SPEEDY_POLLIN);
	    if (CANWRITE(*b))
		speedy_poll_set(&pi, b->wrfd, SPEEDY_POLLOUT);
	}

	/* Poll... */
	if (speedy_poll_wait(&pi, -1) == -1)
	    speedy_util_die("poll");
    }

    /* SGI's /dev/tty goes crazy unless we turn of non-blocking I/O. */
    for (i = 0; i < NUMFDS; ++i) {
	if (fd_open[i])
	    fcntl(i, F_SETFL, fd_flags[i]);
    }

#ifdef SPEEDY_PROFILING
    if (profile_runs) {
	close(s);
	close(e);
	speedy_poll_free(&pi);
	speedy_cb_free(&CB_IN);
	speedy_cb_free(&CB_OUT);
	speedy_cb_free(&CB_ERR);
	return;
    }
#endif
    /* Slightly faster to just exit now instead of cleaning up */
    speedy_util_exit(0,1);
}

int main(int argc, char **argv, char **_junk) {
#ifdef SPEEDY_PROFILING
    char *runs = getenv("SPEEDY_PROFILE_RUNS");
    profile_runs = runs ? atoi(runs) : 1;
    while (profile_runs--)
#endif
	doit((const char * const *)argv);
    return 0;
}

/*
 * Glue Functions
 */

void speedy_abort(const char *s) {
    write(2, s, strlen(s));
    speedy_util_exit(1, 0);
}
