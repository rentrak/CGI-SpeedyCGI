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

/* Info stored at the front of the file */
typedef struct {
    time_t	mtime;		/* Mtime for pinfo's in the queue */
    int		len;		/* Number of pinfo's in the queue */
    int		secret_word;	/* Say the secret word */
} FileInfo;

#define DO_MMAP
#define MMAP_SIZE 512

#ifdef DO_MMAP
#    include <sys/mman.h>
#endif

#ifndef MAP_FAILED
#   define MAP_FAILED -1
#endif

typedef struct {
    int		fd;
#   ifdef DO_MMAP
	char	*maddr;
#   endif
} FileHandle;


static char *q_get(SpeedyQueue *q, PersistInfo *pinfo, int getme);
static char *open_queue(SpeedyQueue *q, FileInfo *finfo, FileHandle *fh);
static void close_queue(FileHandle *fh);
static char *write_finfo(FileHandle *fh, FileInfo *finfo);
static void do_shutdown(SpeedyQueue *q, FileHandle *fh, FileInfo *finfo);
static char *read_pinfo(FileHandle *fh, PersistInfo *pinfo, int idx);
static char *write_pinfo(FileHandle *fh, PersistInfo *pinfo, int idx);


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

void speedy_q_set_secret(SpeedyQueue *q, int secret_word) {
    q->secret_word = secret_word;
}

void speedy_q_free(SpeedyQueue *q) {
    Safefree(q->fname);
}

void speedy_q_destroy(SpeedyQueue *q) {
    FileHandle fh;
    FileInfo finfo;

    /* Open the queue. */
    if (open_queue(q, &finfo, &fh) == NULL) {
	if (finfo.len == 0) {
	    /* Invalidate this file, in case someone already opened it */
	    finfo.len = -1;
	    write_finfo(&fh, &finfo);
	    unlink(q->fname);
	    close_queue(&fh);
	}
    }
    speedy_q_free(q);
}

char *speedy_q_add(SpeedyQueue *q, PersistInfo *pinfo) {
    FileHandle fh;
    FileInfo finfo;
    char *retval;

    /* Open the queue. */
    if ((retval = open_queue(q, &finfo, &fh))) return retval;

    /* If we are adding an out-of-date process, fail */
    if (q->mtime < finfo.mtime) return "file-changed";

    /* Write to end of queue */
    retval = write_pinfo(&fh, pinfo, finfo.len++);

    if (retval == NULL) {
	/* Update length of queue */
	retval = write_finfo(&fh, &finfo);
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
    FileInfo finfo;
    char *retval;

    /* Open the queue. */
    if ((retval = open_queue(q, &finfo, &fh))) return retval;

    if (finfo.len == 0) {
	/* Nothing in the queue, fail */
	retval = "queue empty";
    } else {
	if (getme) {
	    int i;
	    PersistInfo xpinfo;

	    /* Search for my record. */
	    for (i = 0; i < finfo.len; ++i) {
		/* Read this slot. */
		if ((retval = read_pinfo(&fh, &xpinfo, i))) break;
		retval = "not in queue";

		/* Compare */
		if (pinfo->port == xpinfo.port) {

		    /* Copy last pinfo to this slot. */
		    retval = read_pinfo(&fh, &xpinfo, --finfo.len);
		    if (retval == NULL) {
			retval = write_pinfo(&fh, &xpinfo, i);
		    }
		    break;
		}
	    }
	} else {
	    /* Get last pinfo in queue */
	    retval = read_pinfo(&fh, pinfo, --finfo.len);
	}
	if (retval == NULL) {
	    /* Update length of queue */
	    retval = write_finfo(&fh, &finfo);
	}
    }
    close_queue(&fh);
    return retval;
}

static char *open_queue(SpeedyQueue *q, FileInfo *finfo, FileHandle *fh) {
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


#	ifdef DO_MMAP
	    /* File must be long enough to map */
	    if (stbuf.st_size != MMAP_SIZE) ftruncate(fh->fd, MMAP_SIZE);

	    /* Map in */
	    fh->maddr = mmap(
		0, MMAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fh->fd, 0
	    );
	    if (fh->maddr == (char *)MAP_FAILED) {
		close(fh->fd);
		return "mmap";
	    }

	    /* Copy in */
	    Copy(fh->maddr, finfo, 1, FileInfo);
#	else
	    /* Read in */
	    if (read(fh->fd, finfo, sizeof(*finfo)) == -1) {
		close(fh->fd);
		return "read";
	    }
#	endif

	/* If no info from file, create new */
	if (stbuf.st_size < sizeof(*finfo)) {
	    char *retval;

	    finfo->len		= 0;
	    finfo->mtime	= q->mtime;
	    finfo->secret_word	= speedy_make_secret(q->start_time);

	    if ((retval = write_finfo(fh, finfo))) {
		close_queue(fh);
		return retval;
	    }
	}

	/* Get secret from file */
	q->secret_word	= finfo->secret_word;

	/* See if file has been invalidated since we opened it. */
	if (finfo->len == -1) {
	    close_queue(fh);
	} else {
	    break;
	}
    }

    /* If file has older mtime, shut down all procs in it. */
    if (finfo->mtime < q->mtime) {
	do_shutdown(q, fh, finfo);
	finfo->mtime = q->mtime;
    }

    return NULL;
}

static void close_queue(FileHandle *fh) {
#   ifdef DO_MMAP
	if (fh->maddr != (char *)MAP_FAILED) {
	    munmap(fh->maddr, MMAP_SIZE);
	}
#   endif
    close(fh->fd);
}

static void do_shutdown(SpeedyQueue *q, FileHandle *fh, FileInfo *finfo) {
    PersistInfo pinfo;
    int s, e;
    char buf[sizeof(int)+1];

    while (finfo->len) {
	if (read_pinfo(fh, &pinfo, --finfo->len) == NULL) {
	    s = speedy_connect(pinfo.port);
	    e = speedy_connect(pinfo.port);
	    if (s != -1) {
		/* Kiss of death */
		Copy(&(q->secret_word), buf, 1, int);
		buf[sizeof(int)] = 'X';
		write(s, buf, sizeof(buf));
		close(s);
	    }
	}
    }
}

static char *write_finfo(FileHandle *fh, FileInfo *finfo) {
#   ifdef DO_MMAP
	Copy(finfo, fh->maddr, 1, FileInfo);
#   else
	lseek(fh->fd, 0, SEEK_SET);
	CHKERR(write(fh->fd, finfo, sizeof(*finfo)), "write");
#   endif
    return NULL;
}

#define DOSEEK(fh, idx) \
    lseek((fh->fd), sizeof(FileInfo) + (idx) * sizeof(PersistInfo), SEEK_SET)

#define GETADDR(p, fh, idx) \
    (p) = (fh->maddr + sizeof(FileInfo) + (idx) * sizeof(PersistInfo)); \
    if ((p) + sizeof(PersistInfo) >= fh->maddr + MMAP_SIZE) \
	return "queue overflow";

static char *read_pinfo(FileHandle *fh, PersistInfo *pinfo, int idx) {
#   ifdef DO_MMAP
	char *p;
	GETADDR(p, fh, idx)
	Copy(p, pinfo, 1, PersistInfo);
#   else
	DOSEEK(fh, idx);
	CHKERR(read(fh->fd, pinfo, sizeof(*pinfo)), "read");
#   endif
    return NULL;
}

static char *write_pinfo(FileHandle *fh, PersistInfo *pinfo, int idx) {
#   ifdef DO_MMAP
	char *p;
	GETADDR(p, fh, idx)
	Copy(pinfo, p, 1, PersistInfo);
#   else
	DOSEEK(fh, idx);
	CHKERR(write(fh->fd, pinfo, sizeof(*pinfo)), "write");
#   endif
    return NULL;
}
