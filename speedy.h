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

#include <EXTERN.h>
#include <perl.h>

/* For sockets */
#ifndef VMS
# ifdef I_SYS_TYPES
#  include <sys/types.h>
# endif
#include <sys/socket.h>
#ifdef MPE
# define PF_INET AF_INET
# define PF_UNIX AF_UNIX
# define SOCK_RAW 3
#endif
#ifdef I_SYS_UN
#include <sys/un.h>
#endif
# ifdef I_NETINET_IN
#  include <netinet/in.h>
# endif
#include <netdb.h>
#ifdef I_ARPA_INET
# include <arpa/inet.h>
#endif
#else
#include "sockadapt.h"
#endif
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK         0x7F000001
#endif /* INADDR_LOOPBACK */

/* Various */
#ifdef I_UNISTD
#include <unistd.h>
#endif

/* For fcntl */
#ifdef I_FCNTL
#include <fcntl.h>
#endif

/* Found in pp_sys.c... */
   /* fcntl.h might not have been included, even if it exists, because
      the current Configure only sets I_FCNTL if it's needed to pick up
      the *_OK constants.  Make sure it has been included before testing
      the fcntl() locking constants. */
#  if defined(HAS_FCNTL) && !defined(I_FCNTL)
#    include <fcntl.h>
#  endif

#define max(a,b) ((a) > (b) ? (a) : (b))
#define CHKERR(retval, msg)     if ((retval) == -1) return(msg)
#define CHKERR2(v, retval, msg) if ((v=(retval)) == -1) return (msg)

/* #define OPTS_DEBUG */

#include "libspeedy.h"
#include "copybuf.h"
#include "opts.h"
#include "util.h"
#include "start_perl.h"
#include "speedy_poll.h"

#define OPT_BUFSZ_POST	0
#define OPT_BUFSZ_GET	1
#define	OPT_TMPBASE	2
#define OPT_MAXRUNS	3
#define OPT_TIMEOUT	4
#define OPT_MAXBACKENDS  5

#ifdef SPEEDY_C_SOURCE
static OptsRec opts[] = /* Our options */
{
    {
	"BUFSIZ_POST",	'b', OTYPE_INT, (void*) 1024
    },
    {
	"BUFSIZ_GET",	'B', OTYPE_INT, (void*) 8192
    },
    {
	"TMPBASE",	'T', OTYPE_STR, "/tmp/speedy"
    },
    {
	"MAXRUNS",	'r', OTYPE_INT,	(void*)0
    },
    {
	"TIMEOUT",	't', OTYPE_INT,	(void*)3600
    },
    {
	"MAXBACKENDS",	'M', OTYPE_INT,	(void*)0
    },
    {NULL, 0, 0, NULL}
};
#endif

extern void xs_init(void);
