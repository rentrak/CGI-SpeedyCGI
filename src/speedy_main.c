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

#define NUMCBUFS	3

int main(int argc, char **argv, char **_junk) {
    extern char **environ;
    PollInfo pi;
    char *ibuf_buf;
    int env_sz, ibuf_sz, did_shutdown, i;
    int s, e;
    CopyBuf ibuf, obuf, ebuf, *cbs[NUMCBUFS];
    cbs[0] = &ibuf; cbs[1] = &obuf; cbs[2] = &ebuf;

    signal(SIGPIPE, SIG_IGN);

    /* Initialize options */
    speedy_opt_init((const char * const *)argv, (const char * const *)environ);

    /* Connect up with a backend */
    speedy_frontend_connect(NULL, &s, &e);

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
	0,
	s,
	ibuf_buf,
	env_sz
    );
    speedy_cb_alloc(
	&obuf,
	OPTVAL_BUFSIZGET,
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
    fcntl(0, F_SETFL, O_RDONLY|O_NONBLOCK);
    fcntl(1, F_SETFL, O_WRONLY|O_NONBLOCK);
    fcntl(2, F_SETFL, O_WRONLY|O_NONBLOCK);
    fcntl(e, F_SETFL, O_RDWR|O_NONBLOCK);
    fcntl(s, F_SETFL, O_RDWR|O_NONBLOCK);

    /* Poll/select may not wakeup on intial eof, so set for initial read
     * (this is tested in initial_eof test #1)
     */
    speedy_poll_reset(&pi);
    for (i = 0; i < NUMCBUFS; ++i) {
	speedy_poll_set(&pi, cbs[i]->rdfd, SPEEDY_POLLIN);
    }

    /* Try to write our env/argv without dropping into select */
    speedy_poll_set(&pi, cbs[0]->wrfd, SPEEDY_POLLOUT);

    /* Copy streams */
    for (did_shutdown = 0;;) {

	/* Do reads/writes */
	for (i = 0; i < NUMCBUFS; ++i) {
	    CopyBuf *b = cbs[i];
	    int do_read  = speedy_poll_isset(&pi, b->rdfd, SPEEDY_POLLIN);
	    int do_write = speedy_poll_isset(&pi, b->wrfd, SPEEDY_POLLOUT);

	    while (do_read || do_write) {
		if (do_read) {
		    int sz = BUF_SZ(*b);
		    speedy_cb_read(b);
		    if (CANWRITE(*b) && (BUF_EOF(*b) || BUF_SZ(*b) > sz))
			do_write = 1;
		    do_read = 0;
		}

		/*
		 * Attempt write now if we did a read.  Slightly more efficient
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
	for (i = 0; i < NUMCBUFS; ++i) {
	    CopyBuf *b = cbs[i];
	    if ( CANREAD(*b)) speedy_poll_set(&pi, b->rdfd, SPEEDY_POLLIN);
	    if (CANWRITE(*b)) speedy_poll_set(&pi, b->wrfd, SPEEDY_POLLOUT);
	}

	/* Poll... */
	if (speedy_poll_wait(&pi, -1) == -1)
	    speedy_util_die("poll");
    }

    /* SGI's /dev/tty goes crazy unless we turn of non-blocking I/O. */
    fcntl(0, F_SETFL, O_RDONLY);
    fcntl(1, F_SETFL, O_WRONLY);
    fcntl(2, F_SETFL, O_WRONLY);

    /* Slightly faster to just exit now instead of cleaning up */
    return 0;

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
    fputs(s, stderr);
    exit(1);
}
