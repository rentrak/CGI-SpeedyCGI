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

#include "speedy.h"

#define USE_MMAP

#ifdef USE_MMAP
#   include <sys/mman.h>
#endif

#ifndef MAP_FAILED
#   define MAP_FAILED -1
#endif


/* Port must be already in network byte order. */
void speedy_fillin_sin(struct sockaddr_in *sa, unsigned short port) {
    speedy_libfuncs.ls_bzero(sa, sizeof(*sa));
    sa->sin_family = AF_INET;
    sa->sin_port = port;
    sa->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
}

int speedy_connect(unsigned short port) {
    struct sockaddr_in sa;
    int s;

    /* Make socket. */
    if ((s = speedy_make_socket()) == -1) return -1;

    /* Connect */
    speedy_fillin_sin(&sa, port);
    if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
	close(s);
	s = -1;
    }
    return s;
}

char *speedy_read_or_mmap(
    int fd, int writable, int minsz, int maxsz, int stat_size,
    void **buf, int *actual_size
)
{
    *actual_size = stat_size < minsz ? minsz :
                  (stat_size > maxsz ? maxsz : stat_size);
    if (stat_size < minsz) ftruncate(fd, *actual_size);

#   ifdef USE_MMAP
	/* Map in */
	{
	    int flags = PROT_READ | (writable ? PROT_WRITE : 0);
	    *buf = (void*) mmap(0, *actual_size, flags, MAP_SHARED, fd, 0);
	}
	if (*buf == (void*)MAP_FAILED) {
	    close(fd);
	    return "mmap";
	}
#   else
	*buf = speedy_libfuncs.ls_malloc(*actual_size);

	/* Read in */
	if (read(fd, *buf, *actual_size) != *actual_size) {
	    close(fd);
	    speedy_libfuncs.free(*buf);
	    *buf = NULL;
	    return "read";
	}
#   endif
    return NULL;
}

char *speedy_write_or_munmap(
    int fd, void **buf, int actual_size, int do_write
)
{
#   ifdef USE_MMAP
	if (*buf != (void *)MAP_FAILED) {
	    munmap(*buf, actual_size);
	    *buf = (void *)MAP_FAILED;
	}
#   else
	if (*buf) {
	    if (do_write) {
		lseek(fd, 0, SEEK_SET);
		CHKERR(write(fd, *buf, actual_size), "write");
	    }
	    speedy_libfuncs.ls_free(*buf);
	    *buf = NULL;
	}
#   endif
    close(fd);
    return NULL;
}
