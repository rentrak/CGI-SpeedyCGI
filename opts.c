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

#define PRE "SPEEDY_"
#define xispc(c) (c == '\n' || c == '\t' || c == ' ')

static void setval(OptsRec *rec, char *value);
static void addargv(int *argc, char ***argv, char *toadd);
static void initargv(int *argc, char ***argv);

/*
 * Command Line:
 * Input:
 *	-x1 "-y2 -z3 --" "-t30 -a1" filename -opt1 arg1
 * Output:
 *	perl_argv: "" -x1 "-y2 -z3" filename -opt1 arg1
 *       cmd: filename
 *       speedy options: -t30 -a1
 * Input:
 *	"-x1 -y2 -z3 -- -t30 -a1" filename -opt1 arg1
 * Output:
 *       perl_argv: "" "-x1 -y2 -z3" filename -opt1 arg1
 *       cmd: filename
 *       speedy options: -t30 -a1
 * Input:
 *	"-x1 -y2 -z3" filename -opt1 arg1
 * Output:
 *       perl_argv: "" "-x1 -y2 -z3" filename -opt1 arg1
 *       cmd: filename
 *       speedy options: <none>
 */

#define ST_PERLOPTS	0
#define ST_SPEEDYOPTS	1

void speedy_getopt(
    OptsRec myopts[], int numopts, char *argv[], char *envp[],
    char ***scr_argv, char ***perl_argv
)
{
    char **p;
    int i, state, perl_argc, scr_argc;

    /* Init perl argv */
    initargv(&perl_argc, perl_argv);
    addargv(&perl_argc, perl_argv, "");

    /* Init script argv */
    initargv(&scr_argc, scr_argv);

    /* Fill in opts from environment first. */
    for (p = envp; *p; ++p) {
	if (strncmp(*p, PRE, sizeof(PRE)-1) == 0) {
	    for (i = 0; i < numopts; ++i) {
		char *name = myopts[i].name;
		int l = strlen(name);
		if (strncmp(*p+sizeof(PRE)-1, name, l) == 0 &&
		    (*p)[sizeof(PRE)+l-1] == '=')
		{
		    setval(myopts +i, *p+sizeof(PRE)+l);
		}
	    }
	}
    }

    /* Fill in opts from command line. */
    for (++argv, state = ST_PERLOPTS; *argv; ++argv) {
	int c, j;
	char *beg, *end;

	for (beg = *argv; ; beg = end + 1) {
	    c = *beg;
	    while (xispc(c)) {
		c = *++beg;
	    }
	    end = beg;
	    while (c && !xispc(c)) {
		c = *++end;
	    }
	    *end = '\0';

	    if (beg == end) break;

	    if (beg[0] != '-') {
		/* End of perl/speedy opts, start of argv[0] for script */
		*end = c;
		addargv(&perl_argc, perl_argv, beg);
		addargv(&scr_argc, scr_argv, beg);
		for (++argv; *argv; ++argv) {
		    addargv(&perl_argc, perl_argv, *argv);
		    addargv(&scr_argc, scr_argv, *argv);
		}
		return;
	    } else {
		switch(state) {

		    case ST_PERLOPTS:
		    if (beg[1] == '-') {
			state = ST_SPEEDYOPTS;
		    } else {
			addargv(&perl_argc, perl_argv, beg);
		    }
		    break;

		    case ST_SPEEDYOPTS:
		    for (j = 0; j < numopts; ++j) {
			OptsRec *r = myopts + j;
			if (r->opt == beg[1]) {
			    if (!beg[2]) {
				fprintf(stderr,
				    "missing argument to speedy option -%c\n",
				    beg[1]
				);
				exit(1);
			    }
			    setval(r, beg+2);
			    break;
			}
		    }
		    if (j == numopts) {
			fprintf(stderr, "unknown speedy option -%c\n", beg[1]);
			exit(1);
		    }
		    break;
		}
	    }
	    if (c == '\0') break;
	    *end = c;
	}
    }
}

static void setval(OptsRec *rec, char *value) {
    switch(rec->type) {
    case OTYPE_INT:
	rec->value = (void*)atoi(value);
	break;
    case OTYPE_STR:
	rec->value = speedy_strdup(value);
	break;
    }
}

static void addargv(int *argc, char ***argv, char *toadd) {
    Renew(*argv, (*argc)+2, char*);
    (*argv)[(*argc)++] = speedy_strdup(toadd);
    (*argv)[*argc] = NULL;
}

static void initargv(int *argc, char ***argv) {
    New(123, *argv, 1, char*);
    **argv = NULL;
    *argc = 0;
}

#ifdef OPTS_DEBUG
void doargv(char *name, char **argv);

void opts_debug(OptsRec *opts, int numopts, char **scr_argv, char **perl_argv) {
    int i;
    OptsRec *o;
    printf("scr_argv=%x perl_argv=%x\n", (int)scr_argv, (int)perl_argv);

    for (i = 0; i < numopts; ++i) {
	o = opts + i;
	switch(o->type)
	{
	    case OTYPE_INT:
	    printf("opts[%s]=%d\n", o->name, (int)o->value);
	    break;

	    case OTYPE_STR:
	    printf("opts[%s]=%s\n", o->name, (char*)o->value);
	    break;
	}
    }
    doargv("perl_argv", perl_argv);
    doargv("scr_argv", scr_argv);
    exit(0);
}

void doargv(char *name, char **argv) {
    int i = 0;
    do {
	char *s = *argv;
	if (s == NULL) s = "NULL";
	printf("%s[%d]=%s\n", name, i++, s);
    }
    while (*argv++);
}
#endif

