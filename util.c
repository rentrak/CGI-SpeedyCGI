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

int speedy_make_socket() {
    return socket(PF_INET, SOCK_STREAM, 0);
}

/* Port must be already in network byte order. */
void speedy_fillin_sin(struct sockaddr_in *sa, unsigned short port) {
    Zero(sa, sizeof(*sa), char);
    sa->sin_family = AF_INET;
    sa->sin_port = port;
    sa->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
}

char *speedy_strdup(char *s) {
    char *new;
    int l = strlen(s)+1;
    New(123, new, l, char);
    Copy(s, new, l, char);
    return new;
}

int speedy_argc(char **p) {
    int retval = 0;
    for (; *p; ++p) ++retval;
    return retval;
}

int speedy_connect(unsigned short port) {
    int s;
    struct sockaddr_in sa;

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

int speedy_make_secret(struct timeval *start_time) {
    unsigned char *s;
    struct timeval now;
    gettimeofday(&now, NULL);
    s = (unsigned char *)&start_time->tv_usec;
    return ((s[0]<<24)|(s[1]<<16)|(s[2]<<8)|s[3]) ^ now.tv_usec;
}
