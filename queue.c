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

/* Implement queue for persistent-info.  Use a shared-file. */

#include "speedy.h"

#define DO_MMAP
#define FILE_SIZE 512

#define PINFO_BYTES	(FILE_SIZE - (sizeof(time_t) + 2*sizeof(int)))
#define NUM_PINFO	PINFO_BYTES / sizeof(PersistInfo)

/* Info stored in the file */
typedef struct {
    time_t	mtime;			/* Mtime for pinfo's in the queue */
    int		len;			/* Number of pinfo's in the queue */
    int		secret_word;		/* Say the secret word */
    PersistInfo	pinfo[NUM_PINFO];	/* Persistent Info */
} FileInfo;

typedef struct {
    int		fd;
    FileInfo	*finfo;
} FileHandle;


#ifdef DO_MMAP
#   include <sys/mman.h>
#endif

#ifndef MAP_FAILED
#   define MAP_FAILED -1
#endif


static char *q_get(SpeedyQueue *q, PersistInfo *pinfo, int getme);
static char *open_queue(SpeedyQueue *q, FileHandle *fh);
static char *close_queue(FileHandle *fh);
static void do_shutdown(SpeedyQueue *q, FileHandle *fh);


char *speedy_q_init(
    SpeedyQueue *q, OptsRec *opts, char *cmd, struct timeval *start_time
) {
    struct stat stbuf;
    int dev, ino, l;
    char *fname, *tmpbase;

    /* Stat command file */
    CHKERR(stat(cmd, &stbuf), cmd);

    /* Convert dev/ino to ints.  Linux uses double words that sprintf */
    /* doesn't like */
    dev = stbuf.st_dev;
    ino = stbuf.st_ino;

    /* Build lock filename */
    l = strlen(tmpbase = OVAL_STR(opts[OPT_TMPBASE]));
    New(123, fname, l+(17*3)+5, char);
    sprintf(fname, "%s.%x.%x.%x", tmpbase, ino, dev, geteuid());

    /* Make a new queue */
    q->fname		= fname;
    q->mtime		= stbuf.st_mtime;
    q->start_time	= start_time;

    return NULL;
}

void speedy_q_free(SpeedyQueue *q) {
    if (q->fname) {
	Safefree(q->fname);
	q->fname = NULL;
    }
}

void speedy_q_destroy(SpeedyQueue *q) {
    FileHandle fh;

    /* Open the queue. */
    if (open_queue(q, &fh) == NULL) {
	/* Only destroy if length is zero */
	if (fh.finfo->len == 0) {
	    /* Invalidate this file, in case someone already opened it */
	    fh.finfo->len = -1;
	    unlink(q->fname);
	}
	close_queue(&fh);
    }
    speedy_q_free(q);
}

char *speedy_q_add(SpeedyQueue *q, PersistInfo *pinfo) {
    FileHandle fh;
    FileInfo *finfo;
    char *retval;

    /* Open the queue. */
    if ((retval = open_queue(q, &fh))) return retval;
    finfo = fh.finfo;

    /* If we are adding an out-of-date process, fail */
    if (q->mtime < finfo->mtime) {
        retval = "file-changed";
    }
    /* See if queue is full */
    else if (finfo->len >= NUM_PINFO) {
	retval = "queue-full";
    }
    else {
	/* Write to end of queue */
	Copy(pinfo, finfo->pinfo + finfo->len, 1, PersistInfo);
	finfo->len++;
    }
    close_queue(&fh);
    return retval;
}

/* Take the last entry out of the queue */
char *speedy_q_get(SpeedyQueue *q, PersistInfo *pinfo) {
    return q_get(q, pinfo, 0);
}

/* Take myself out of the queue */
char *speedy_q_getme(SpeedyQueue *q, PersistInfo *pinfo) {
    return q_get(q, pinfo, 1);
}

static char *q_get(SpeedyQueue *q, PersistInfo *pinfo, int getme) {
    FileHandle fh;
    FileInfo *finfo;
    char *retval = NULL;
    int i;

    /* Open the queue. */
    if ((retval = open_queue(q, &fh))) return retval;
    finfo = fh.finfo;

    if (finfo->len == 0) {
	/* Nothing in the queue, fail */
	retval = "queue empty";
    } else {
	/* Getme means remove my entry */
	if (getme) {
	    retval = "not in queue";

	    /* Search for my record. */
	    for (i = 0; i < finfo->len; ++i) {
		PersistInfo *xpinfo = finfo->pinfo + i;

		/* Compare */
		if (pinfo->port == xpinfo->port) {
		    /* Found.  Move down other pinfos in the queue */
		    finfo->len--;
		    if (i < finfo->len)
			Move(xpinfo+1, xpinfo, finfo->len - i, PersistInfo);
		    retval = NULL;
		    break;
		}
	    }
	} else {
	    /* Get last pinfo in queue */
	    finfo->len--;
	    Copy(finfo->pinfo + finfo->len, pinfo, 1, PersistInfo);
	}
    }
    close_queue(&fh);
    return retval;
}

static char *open_queue(SpeedyQueue *q, FileHandle *fh) {
    struct flock fl;
    struct stat stbuf;

    while (1) {
	/* Open the file */
	CHKERR2(fh->fd, open(q->fname, O_RDWR | O_CREAT, 0600), q->fname);

	/* Lock it down.  Block until lock is available. */
	fl.l_type	= F_WRLCK;
	fl.l_whence	= SEEK_SET;
	fl.l_start	= 0;
	fl.l_len	= 0;
	if (fcntl(fh->fd, F_SETLKW, &fl) == -1) {
	    close(fh->fd);
	    return "get lock";
	}

	/* Make sure I own the file. */
	fstat(fh->fd, &stbuf);
	if (stbuf.st_uid != geteuid()) {
	    close(fh->fd);
	    return "wrong file owner";
	}

	/* File must be long enough */
	if (stbuf.st_size < FILE_SIZE) ftruncate(fh->fd, FILE_SIZE);

#	ifdef DO_MMAP
	    /* Map in */
	    fh->finfo = (FileInfo*)mmap(
		0, FILE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fh->fd, 0
	    );
	    if (fh->finfo == (FileInfo*)MAP_FAILED) {
		close(fh->fd);
		return "mmap";
	    }
#	else
	    New(123, fh->finfo, FILE_SIZE, FileInfo);

	    /* Read in */
	    if (read(fh->fd, fh->finfo, FILE_SIZE) == -1) {
		close(fh->fd);
		return "read";
	    }
#	endif

	/* If no info from file, create new */
	if (stbuf.st_size < FILE_SIZE) {
	    fh->finfo->len		= 0;
	    fh->finfo->mtime		= q->mtime;
	    fh->finfo->secret_word	= speedy_make_secret(q->start_time);
	}
	else if (fh->finfo->len < 0) {
	    fh->finfo->len = 0;
	}
	else if (fh->finfo->len > NUM_PINFO) {
	    fh->finfo->len = NUM_PINFO;
	}

	/* Get secret from file */
	q->secret_word	= fh->finfo->secret_word;

	/* See if file has been invalidated since we opened it. */
	if (fh->finfo->len == -1) {
	    close_queue(fh);
	} else {
	    break;
	}
    }

    /* If file has older mtime, shut down all procs in it. */
    if (fh->finfo->mtime < q->mtime) {
	do_shutdown(q, fh);
	fh->finfo->mtime = q->mtime;
    }

    return NULL;
}

static char *close_queue(FileHandle *fh) {
#   ifdef DO_MMAP
	if (fh->finfo != (FileInfo *)MAP_FAILED) {
	    munmap((void*)fh->finfo, FILE_SIZE);
	    fh->finfo = (FileInfo *)MAP_FAILED;
	}
#   else
	if (fh->finfo) {
	    lseek(fh->fd, 0, SEEK_SET);
	    CHKERR(write(fh->fd, fh->finfo, FILE_SIZE), "write");
	    Safefree(fh->finfo);
	    fh->finfo = NULL;
	}
#   endif
    close(fh->fd);
    return NULL;
}

static void do_shutdown(SpeedyQueue *q, FileHandle *fh) {
    FileInfo *finfo = fh->finfo;
    int s, e;
    char buf[sizeof(int)+1];

    while (finfo->len--) {
	PersistInfo *pinfo = finfo->pinfo + finfo->len;
	if ((s = speedy_connect(pinfo->port)) != -1) {
	    /* stderr */
	    e = speedy_connect(pinfo->port);

	    /* Kiss of death */
	    Copy(&(q->secret_word), buf, 1, int);
	    buf[sizeof(int)] = 'X';
	    write(s, buf, sizeof(buf));
	    close(s);
	    close(e);
	}
    }
}
