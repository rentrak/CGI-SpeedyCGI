
/*
 * Copyright (C) 2000  Daemon Consulting Inc.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include "speedy.h"

#define PREFIX		"SPEEDY_"
#define PREFIX_LEN	(sizeof(PREFIX)-1)
#define STRLIST_MALLOC	10

/*
 * StrList is a variable length array of char*'s
 */
typedef struct {
    char **ptrs;
    int  len;
    int  malloced;
} StrList;

/* Globals */
static StrList exec_argv, exec_envp, perl_argv;
static const char * const *orig_argv;
static int script_argv_loc;
static int got_shbang;

/*
 * StrList Methods
 */

#define strlist_len(l)		((l)->len)
#define strlist_str(l, i)	((l)->ptrs[i])
#define strlist_concat(l, in)	\
		strlist_concat2((l), (const char * const *)strlist_export(in))
#define strlist_append(l, s)	strlist_append2(l, s, strlen(s))

static void strlist_init(StrList *list) {
    list->malloced = STRLIST_MALLOC;
    list->ptrs = (char**)speedy_malloc(sizeof(char*) * list->malloced);
    list->len = 0;
}

static void strlist_setlen(StrList *list, int newlen) {
    while (list->len > newlen)
	speedy_free(list->ptrs[--(list->len)]);
    list->len = newlen;
    while (list->len >= list->malloced) {
	list->malloced *= 2;
	list->ptrs = (char**)speedy_realloc(
	    list->ptrs, sizeof(char*) * list->malloced
	);
    }
}

static void strlist_append3(StrList *list, char *str) {
    list->ptrs[list->len] = str;
    strlist_setlen(list, list->len + 1);
}

static void strlist_append2(StrList *list, const char *str, int len) {
    char *s = speedy_malloc(len+1);
    speedy_memcpy(s, str, len);
    s[len] = '\0';
    strlist_append3(list, s);
}

static char **strlist_export(StrList *list) {
    list->ptrs[list->len] = NULL;
    return list->ptrs;
}

static void strlist_concat2(StrList *list, const char * const *in) {
    for (; *in; ++in)
	strlist_append(list, *in);
}

static void strlist_free(StrList *list) {
    strlist_setlen(list, 0);
    speedy_free(list->ptrs);
}

static void strlist_replace(StrList *list, int i, char *newstr) {
    speedy_free(list->ptrs[i]);
    list->ptrs[i] = newstr;
}

/* Split string on whitespace */
static void strlist_split(StrList *out, const char * const *in) {
    const char * const *p;
    const char *s, *beg;

    for (p = in; *p; ++p) {
	for (s = beg = *p; *s;) {
	    if (isspace((int)*s)) {
		if (beg < s)
		    strlist_append2(out, beg, s - beg);
		while (isspace((int)*s))
		    ++s;
		beg = s;
	    } else {
		++s;
	    }
	}
	if (beg < s) {
	    strlist_append2(out, beg, s - beg);
	}
    }
}

/*
 * End of StrList stuff
 */

/* Split into arg0, perl args, speedy options and script args */
static void cmdline_split(
    const char * const *in, char **arg0, StrList *perl_args,
    StrList *speedy_opts, StrList *script_args
)
{
    StrList split;
    char **p;

    /* Arg-0 */
    if (arg0)
	*arg0 = speedy_strdup(*in);
    ++in;

    /* Split on spaces */
    strlist_init(&split);
    strlist_split(&split, in);
    p = strlist_export(&split);

    /* Perl args & Speedy options */
    for (; *p && **p == '-'; ++p) {
	if (p[0][1] == '-' && p[0][2] == '\0') {
	    for (++p; *p && **p == '-'; ++p)
		strlist_append(speedy_opts, *p);
	    break;
	} else {
	    strlist_append(perl_args, *p);
	}
    }

    /* Script argv */
    if (script_args) {
	for (; *p; ++p)
	    strlist_append(script_args, *p);
    }
    
    strlist_free(&split);
}


int speedy_opt_set(OptRec *optrec, const char *value) {
    if (optrec->type == OTYPE_STR) {
	if (optrec->changed && optrec->value)
	    speedy_free(optrec->value);
	optrec->value = speedy_strdup(value);
    } else {
	int val = atoi(value);

	switch(optrec->type) {
	    case OTYPE_WHOLE:
		if (val < 0) return 0;
		break;
	    case OTYPE_NATURAL:
		if (val < 1) return 0;
		break;
	}
	optrec->value = (void*)val;
    }
    optrec->changed = 1;
    return 1;
}

const char *speedy_opt_get(OptRec *optrec) {
    if (optrec->type == OTYPE_STR) {
	return (char*)optrec->value;
    } else {
	static char buf[20];
	sprintf(buf, "%d", (int)optrec->value);
	return buf;
    }
}

static int ocmp(const void *a, const void *b) {
    return strcmp((const char *)a, ((OptRec *)b)->name);
}

static int opt_set_byname2(const char *optname, int len, const char *value) {
    OptRec *match;
    char *upper;
    int retval = 0;

    /* Copy the upper-case optname into "upper" */
    upper = speedy_malloc(len+1);
    upper[len] = '\0';
    while (len--)
	upper[len] = toupper(optname[len]);

    match = bsearch(upper, speedy_optdefs, SPEEDY_NUMOPTS, sizeof(OptRec), &ocmp);
    if (match)
	retval = speedy_opt_set(match, value);
    speedy_free(upper);
    return retval;
}

int speedy_opt_set_byname(const char *optname, const char *value) {
    return opt_set_byname2(optname, strlen(optname), value);
}

static void process_speedy_opts(StrList *speedy_opts, int len) {
    int i, j;

    for (i = 0; i < len; ++i) {
	char *s = strlist_str(speedy_opts, i);
	char letter = s[1];

	for (j = 0; j < SPEEDY_NUMOPTS; ++j) {
	    if (speedy_optdefs[j].letter == letter) {
		speedy_opt_set(speedy_optdefs + j, s+2);
		break;
	    }
	}
	if (j == SPEEDY_NUMOPTS) {
	    DIE_QUIET("Unknown speedy option '-%c'", letter);
	}
    }
}

void speedy_opt_init(const char * const *argv, const char * const *envp) {
    StrList speedy_opts, script_argv;
    int opts_len_before, i;
    const char * const *p;

    strlist_init(&exec_argv);
    strlist_init(&exec_envp);
    strlist_init(&script_argv);
    strlist_init(&perl_argv);
    strlist_init(&speedy_opts);

    orig_argv = argv;

    /* Make sure perl_argv has an arg0 */
    strlist_append(&perl_argv, "perl");

    /* Split up the command line */
    cmdline_split(argv, NULL, &perl_argv, &speedy_opts, &script_argv);

    /* Append the PerlArgs option to perl_argv */
    if (OPTREC_PERLARGS.changed) {
	StrList split;
	char *tosplit[2];

	strlist_init(&split);
	tosplit[0] = OPTVAL_PERLARGS;
	tosplit[1] = NULL;
	strlist_split(&split, (const char * const *)tosplit);
	strlist_concat(&perl_argv, &split);
	strlist_free(&split);
    }

    /* Append to the speedy_opts any OptRec's changed before this call */
    opts_len_before = strlist_len(&speedy_opts);
    for (i = 0; i < SPEEDY_NUMOPTS; ++i) {
	OptRec *rec = speedy_optdefs + i;

	if (rec->changed && rec->letter) {
	    const char *s = speedy_opt_get(rec);
	    char *t = speedy_malloc(strlen(s)+3);
	    sprintf(t, "-%c%s", rec->letter, s);
	    strlist_append3(&speedy_opts, t);
	}
    }

    /* Set our OptRec values based on the speedy_opts that we got from argv */
    process_speedy_opts(&speedy_opts, opts_len_before);

    /*
     * Create exec args from perl args, speedy args and script args
     * Save the location of the script args
     */
    strlist_concat(&exec_argv, &perl_argv);
    if (strlist_len(&speedy_opts)) {
	strlist_append2(&exec_argv, "--", 2);
	strlist_concat(&exec_argv, &speedy_opts);
    }
    script_argv_loc = strlist_len(&exec_argv);
    strlist_concat(&exec_argv, &script_argv);

    /* Copy the environment to exec_envp */
    strlist_concat2(&exec_envp, envp);

    /* Set our OptRec values based on the environment */
    for (p = envp; *p; ++p) {
	if (strncmp(*p, PREFIX, PREFIX_LEN) == 0) {
	    const char *optname = *p + PREFIX_LEN;
	    const char *eqpos = strchr(optname, '=');
	    if (eqpos)
		(void) opt_set_byname2(optname, eqpos - optname, eqpos+1);
	}
    }

    strlist_free(&speedy_opts);
    strlist_free(&script_argv);
}

/* Read the script file for options on the #! line at top. */
void speedy_opt_read_shbang() {
    int file_size, fd;
    char buf[512];
    const char *fname = speedy_opt_script_argv()[0];
    char *s, *argv[3], *arg0;
    StrList speedy_opts;

    if (got_shbang || !fname)
	return;
    
    got_shbang = 1;

    if ((fd = open(fname, O_RDONLY, 0600)) != -1) {
	if ((file_size = read(fd, buf, sizeof(buf))) > 2 &&
	    buf[0] == '#' && buf[1] == '!')
	{
	    /* Null-terminate at end-of-file and/or end-of-line */
	    buf[file_size-1] = '\0';
	    if ((s = strchr(buf, '\n')))
		*s = '\0';

	    argv[0] = "";
	    argv[1] = strchr(buf + 2, ' ');
	    argv[2] = NULL;

	    /* Split up the command line */
	    strlist_init(&speedy_opts);
	    cmdline_split(
		(const char * const *)argv, &arg0,
		&perl_argv, &speedy_opts, NULL
	    );

	    /* Put arg0 into perl_argv[0] */
	    strlist_replace(&perl_argv, 0, arg0);

	    /* Set our OptRec values based on the speedy opts */
	    process_speedy_opts(&speedy_opts, strlist_len(&speedy_opts));
	    strlist_free(&speedy_opts);
	}
	close(fd);
    }
}

int speedy_opt_got_shbang() {
    return got_shbang;
}

void speedy_opt_set_script_argv(const char * const *argv) {
    /* Replace the existing script_argv with this one */
    strlist_setlen(&exec_argv, script_argv_loc);
    strlist_concat2(&exec_argv, argv);
}

const char * const *speedy_opt_script_argv() {
    return (const char * const *)(strlist_export(&exec_argv) + script_argv_loc);
}

char **speedy_opt_perl_argv() {
    static int called_before;

    if (!called_before) {
	/* Append the script argv to the end of perl_argv */
	strlist_concat2(&perl_argv, speedy_opt_script_argv());
	called_before = 1;
    }
    return strlist_export(&perl_argv);
}

const char * const *speedy_opt_orig_argv() {
    return orig_argv;
}

const char * const *speedy_opt_exec_argv() {
    exec_argv.ptrs[0] = OPTVAL_BACKENDPROG;
    return (const char * const *)strlist_export(&exec_argv);
}

const char * const *speedy_opt_exec_envp() {
    return (const char * const *)strlist_export(&exec_envp);
}
