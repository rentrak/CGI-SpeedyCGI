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

#include <sys/types.h>

/* Persistent info stored in header of queue file */
typedef struct {
    char		*fname;		/* Queue file name */
    time_t		mtime;		/* Mtime of procs in the queue */
    int			secret_word;	/* Say the secret word */
    struct timeval	*start_time;
  int                 queue_size;
} SpeedyQueue;

/* Persistent info stored for each process in queue file */
typedef struct {
  pid_t pid;
  unsigned short port;		/* Stored in network byte order */
  unsigned short used;
} PersistInfo;


char *speedy_q_init(
    SpeedyQueue *q, char *tmpbase, char *cmd, struct timeval *start_time,
    struct stat *stbuf
);
void speedy_q_free(SpeedyQueue *q);
void speedy_q_destroy(SpeedyQueue *q);
char *speedy_q_get(SpeedyQueue *q, PersistInfo *pinfo);
char *speedy_q_getme(SpeedyQueue *q, PersistInfo *pinfo);
char *speedy_q_add(SpeedyQueue *q, PersistInfo *pinfo);
void speedy_fillin_pinfo(PersistInfo *pinfo, int lstn);



