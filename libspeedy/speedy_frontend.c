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

static int array_bufsize(char **p);
static void add_strings(char **bp, char **p);

char *speedy_getclient(SpeedyQueue *q, int *s, int *e)
{
    char *retval;
    PersistInfo pinfo;

    /* Get proc out of the queue and init communication */
    retval = speedy_q_get(q, &pinfo);
    if (!retval) retval = speedy_comm_init(pinfo.port, s, e);
    return retval;
}

/*
 * This is an entry for programs that are not linked already with the
 * perl library (mod_speedycgi for instance).  The program should fork
 * and then exec via this function to spawn the perl backend.
 */
char *speedy_exec_client(char **argv, int lstn) {
    if (lstn != 3) {
	dup2(lstn, 3);
	close(lstn);
    }
    argv[0] = "speedybackend";
    execv(SPEEDY_BIN, argv);
    return "unreached";
}

char *speedy_comm_init(unsigned short port, int *s, int *e) {
    CHKERR2(*s, speedy_connect(port), "connect failed");
    if ((*e = speedy_connect(port)) == -1) {
	close(*s);
	return "connect failed";
    }
    return NULL;
}

/* Find out how big a buffer we need for sending environment.*/
int speedy_sendenv_size(char *envp[], char *scr_argv[]) {
    return sizeof(int) +			/* secret-word */
	   1 + array_bufsize(envp) +		/* env */
	   1 + array_bufsize(scr_argv+1) +	/* argv */
	   1;					/* terminator */
}

/* Return the size of buffer needed to send array. */
static int array_bufsize(char **p) {
    int l, sz = sizeof(int);	/* bytes for terminator */
    for (; *p; ++p) {
	if ((l = strlen(*p))) sz += sizeof(int) + l;
    }
    return sz;
}

#define ADD(s,d,l,t) \
    speedy_libfuncs.ls_memcpy(d,s,(l)*sizeof(t)); (d) += ((l)*sizeof(t))

/* Fill in the environment to send. */
void speedy_sendenv_fill(
    char *buf, char *envp[], char *scr_argv[], int secret_word
)
{
    /* Add secret word */
    ADD(&secret_word, buf, 1, int);

    /* Put env into buffer */
    *buf++ = 'E';
    add_strings(&buf, envp);

    /* Put argv into buffer */
    *buf++ = 'A';
    add_strings(&buf, scr_argv+1);

    /* End */
    *buf++ = ' ';
}

/* Copy a block of strings into the buffer,  */
static void add_strings(char **bp, char **p) {
    int l;

    /* Put in length plus string, without terminator */
    for (; *p; ++p) {
	if ((l = strlen(*p))) {
	    ADD(&l, *bp, 1, int);
	    ADD(*p, *bp, l, char);
	}
    }

    /* Terminate with zero-length string */
    l = 0;
    ADD(&l, *bp, 1, int);
}

/*
 * Start a listener.  Always runs in the parent process.
 */
char *speedy_do_listen(SpeedyQueue *q, PersistInfo *pinfo, int *lstn) {
    struct sockaddr_in sa;
    char *errmsg;

    /* Open socket */
    CHKERR2(*lstn, speedy_make_socket(), "socket");

    /* Fill in name -- any port will do */
    speedy_fillin_sin(&sa, 0);

    /* Bind to name */
    if (bind(*lstn, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
	errmsg = "bind"; goto do_listen_error;
    }

    /* Listen */
    if (listen(*lstn, 1) == -1) {
	errmsg = "listen"; goto do_listen_error;
    }

    /* Fill-in the pinfo based on the listener */
    speedy_fillin_pinfo(pinfo, *lstn);

    return NULL;

do_listen_error:
    close(*lstn);
    return errmsg;
}
