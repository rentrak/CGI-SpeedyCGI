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

#include "speedy.h"

/* Copy buffer code */

void speedy_cb_alloc(
    CopyBuf *bp, int maxsz, int rdfd, int wrfd, char *buf, int sz
)
{
    bp->buf		= buf;
    bp->sz		= sz;
    bp->maxsz		= maxsz;
    bp->eof		= 0;
    bp->rdfd		= rdfd;
    bp->wrfd		= wrfd;
    bp->write_err	= 0;
}

void speedy_cb_free(CopyBuf *bp) {
    if (bp->buf) {
	speedy_free(bp->buf);
	bp->buf = NULL;
    }
}

void speedy_cb_read(CopyBuf *bp) {
    int n;

    if (!bp->buf) {
	bp->buf = speedy_malloc(bp->maxsz);
    }
    switch(n = read(bp->rdfd, bp->buf + bp->sz, bp->maxsz - bp->sz))
    {
    case -1:
	/* If not ready to read, then all done. */
	if (SP_NOTREADY(errno))
	    break;
	/* Fall through - assume eof if other read errors */
    case  0:
	bp->eof = 1;
	if (bp->sz == 0) {
	    speedy_cb_free(bp);
	}
	break;
    default:
	bp->sz += n;
	break;
    }
}

SPEEDY_INLINE static void cb_shift(CopyBuf *bp, int n) {
    bp->sz -= n;
    if (bp->sz)
	speedy_memmove(bp->buf, bp->buf + n, bp->sz);
}

void speedy_cb_write(CopyBuf *bp) {
    int n;

    if (!bp->write_err) {
	n = write(bp->wrfd, bp->buf, bp->sz);

	/* If any error other than EAGAIN, then write error */
	if (n == -1 && !SP_NOTREADY(errno))
	    BUF_SET_WRITE_ERR(*bp, errno ? errno : EIO);
    }

    /* If error then pretend we did the write */
    if (bp->write_err) 
	n = bp->sz;

    if (n > 0) {
	cb_shift(bp, n);
	if (bp->eof && !bp->sz)
	    speedy_cb_free(bp);
    }
}

int speedy_cb_shift(CopyBuf *bp) {
    int retval = (bp->buf)[0];
    cb_shift(bp, 1);
    return retval;
}
