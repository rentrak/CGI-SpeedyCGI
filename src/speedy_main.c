/*
 * Copyright (C) 2000  Daemon Consulting Inc.
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

static const int file_flags[NUMFDS] = {O_RDONLY, O_WRONLY, O_WRONLY};

int main(int argc, char **argv, char **_junk) {
    extern char **environ;
    PollInfo pi;
    char *ibuf_buf;
    int env_sz, ibuf_sz, did_shutdown, i;
    int s, e, is_open[NUMFDS];
    CopyBuf ibuf, obuf, ebuf, *cbs[NUMFDS];
    cbs[0] = &ibuf; cbs[1] = &obuf; cbs[2] = &ebuf;

    signal(SIGPIPE, SIG_IGN);

    /* Find out if fd's 0-2 are open.  Also make them non-blocking */
    for (i = 0; i < NUMFDS; ++i) {
	is_open[i] =
	    fcntl(i, F_SETFL, file_flags[i] | O_NONBLOCK) != -1 ||
	    errno != EBADF;
    }

    /* Initialize options */
    speedy_opt_init((const char * const *)argv, (const char * const *)environ);

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

    /* Connect up with a backend */
    speedy_frontend_connect(&s, &e);

    /* Non-blocking I/O on sockets */
    fcntl(e, F_SETFL, O_RDWR|O_NONBLOCK);
    fcntl(s, F_SETFL, O_RDWR|O_NONBLOCK);

    /* Create buffer with env/argv data to send */
    ibuf_buf = speedy_frontend_mkenv(
	(const char * const *)environ, speedy_opt_script_argv(),
	OPTVAL_BUFSIZPOST, 512,
	&env_sz, &ibuf_sz
    );

    speedy_poll_init(&pi, max(s,e));

    /* Allocate buffers for copying below: */
    /*	fd0 -> ibuf -> s	*/
    /*	s   -> obuf -> fd1	*/
    /*	e   -> ebuf -> fd2	*/
    speedy_cb_alloc(
	&ibuf,
	ibuf_sz,
	is_open[0] ? 0 : -1,
	s,
	ibuf_buf,
	env_sz
    );
    speedy_cb_alloc(
	&obuf,
	OPTVAL_BUFSIZGET,
	s,
	is_open[1] ? 1 : -1,
	NULL,
	0
    );
    speedy_cb_alloc(
	&ebuf,
	512,
	e,
	is_open[2] ? 2 : -1,
	NULL,
	0
    );

    /* Poll/select may not wakeup on intial eof, so set for initial read
     * (this is tested in initial_eof test #1)
     * Also, set eof for files that are not open
     */
    speedy_poll_reset(&pi);
    for (i = 0; i < NUMFDS; ++i) {
	if (is_open[i])
	    speedy_poll_set(&pi, cbs[i]->rdfd, SPEEDY_POLLIN);
	else
	    BUF_SETEOF(*(cbs[i]));
    }

    /* Try to write our env/argv without dropping into select */
    speedy_poll_set(&pi, cbs[0]->wrfd, SPEEDY_POLLOUT);

    /* Copy streams */
    for (did_shutdown = 0;;) {

	/* Do reads/writes */
	for (i = 0; i < NUMFDS; ++i) {
	    CopyBuf *b = cbs[i];
	    int do_read  = CANREAD(*b) &&
	                   speedy_poll_isset(&pi, b->rdfd, SPEEDY_POLLIN);
	    int do_write = CANWRITE(*b) &&
	                   speedy_poll_isset(&pi, b->wrfd, SPEEDY_POLLOUT);

	    while (do_read || do_write) {
		if (do_read) {
		    int sz = BUF_SZ(*b);
		    speedy_cb_read(b);
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
		    if (CANREAD(*b) && BUF_SZ(*b) < sz)
			do_read = 1;
		    do_write = 0;
		}

		/* See if we should shutdown the CGI's stdin. */
		if (!did_shutdown && COPYDONE(ibuf)) {
		    shutdown(s, 1);
		    did_shutdown = 1;
		}
	    }
	}
	
	if (COPYDONE(obuf) && COPYDONE(ebuf))
	    break;

	/* Reset events */
	speedy_poll_reset(&pi);
	
	/* Set read/write events */
	for (i = 0; i < NUMFDS; ++i) {
	    CopyBuf *b = cbs[i];
	    if ( CANREAD(*b)) speedy_poll_set(&pi, b->rdfd, SPEEDY_POLLIN);
	    if (CANWRITE(*b)) speedy_poll_set(&pi, b->wrfd, SPEEDY_POLLOUT);
	}

	/* Poll... */
	if (speedy_poll_wait(&pi, -1) == -1)
	    speedy_util_die("poll");
	speedy_util_time_invalidate();
    }

    /* SGI's /dev/tty goes crazy unless we turn of non-blocking I/O. */
    for (i = 0; i < NUMFDS; ++i) {
	if (is_open[i])
	    fcntl(i, F_SETFL, file_flags[i]);
    }

    /* Slightly faster to just exit now instead of cleaning up */
    _exit(0);

    /* Here's what would need to be done if we cleaned up properly */
    /*NOTREACHED*/
    close(1);
    close(2);
    close(0);
    close(s);
    close(e);
    speedy_poll_free(&pi);
    speedy_cb_free(&ibuf);
    speedy_cb_free(&obuf);
    speedy_cb_free(&ebuf);
}

/*
 * Glue Functions
 */

void speedy_abort(const char *s) {
    write(2, s, strlen(s));
    exit(1);
}
