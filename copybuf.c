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

/* Copy buffer */

#include "speedy.h"

void speedy_cb_alloc(CopyBuf *bp, int maxsz, int rdfd, int wrfd) {
    bp->buf	= NULL;
    bp->sz	= 0;
    bp->maxsz	= maxsz;
    bp->eof	= 0;
    bp->rdfd	= rdfd;
    bp->wrfd	= wrfd;
}

void speedy_cb_free(CopyBuf *bp) {
    if (bp->buf) {
	Safefree(bp->buf);
	bp->buf = NULL;
    }
}

void speedy_cb_read(CopyBuf *bp) {
    int n;

    if (!bp->buf) {
	New(123, bp->buf, bp->maxsz, char);
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
	    Move(bp->buf+n, bp->buf, bp->sz, char);
	}
	else if (bp->eof) {
	    speedy_cb_free(bp);
	}
    }
}
