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

#include "speedy.h"

/* Copy buffer code */

void speedy_cb_alloc(
    CopyBuf *bp, int maxsz, int rdfd, int wrfd, char *buf, int sz
)
{
    bp->buf	= buf;
    bp->sz	= sz;
    bp->maxsz	= maxsz;
    bp->eof	= 0;
    bp->rdfd	= rdfd;
    bp->wrfd	= wrfd;
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
	if (errno == EAGAIN) break;
	/* Fall through */
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

void speedy_cb_write(CopyBuf *bp) {
    int n = write(bp->wrfd, bp->buf, bp->sz);
    if (n > 0) {
	bp->sz -= n;
	if (bp->sz) {
	    speedy_memmove(bp->buf, bp->buf + n, bp->sz);
	}
	else if (bp->eof) {
	    speedy_cb_free(bp);
	}
    }
    else if (n == -1 && errno != EAGAIN) {
	bp->sz = 0;
	bp->eof = 1;
    }
}
