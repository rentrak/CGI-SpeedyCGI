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

typedef struct {
    char	*name;
    char	opt;
    int		type;
    void	*value;
} OptsRec;

#define	OTYPE_INT	0
#define	OTYPE_STR	1

#define	OVAL_INT(o)	((int)(o).value)
#define OVAL_STR(o)	((char*)(o).value)

void speedy_getopt(
    OptsRec myopts[], char *argv[], char *envp[],
    char ***scr_argv, char ***perl_argv
);
void speedy_addopts_file(
    OptsRec myopts[], char *fname, char ***perl_argv
);


#ifdef OPTS_DEBUG
void opts_debug(OptsRec *opts, char **scr_argv, char **perl_argv);
void doargv(char *name, char **argv);
#endif
