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
#include <patchlevel.h>

/*
 * Accomodate 5.004
 */
#if PATCHLEVEL < 5
#   define PL_na na
#   define newSVpvn(s,l) my_newSVpvn(s,l)
    static SV *my_newSVpvn(char *s, int l) {
	return l ? newSVpv(s, l) : newSVpv("",0);
    }
#endif

static PerlInterpreter	*g_perl;	/* Perl interpreter */
static SpeedyQueue	*g_q;		/* Queue pointer */
static PersistInfo	*g_pinfo;	/* Persistent Info */
static int		g_queued;	/* Are we in the queue? */
static int		g_alarm;	/* Alarm wakeup */

typedef struct {
    GV	*pv_stdin;
    GV	*pv_stdout;
    GV	*pv_stderr;
    HV	*pv_env;
    AV	*pv_argv;
    SV  *pv_opts_changed;
    HV	*pv_opts;
} PerlVars;


static void doit(
    int lstn, char **perl_argv, OptsRec *opts, int curdir
);
static void onerun(int secret_word, int mypid, PerlVars *pv, int numrun);
static int get_string(PerlIO *pio_in, char **buf);
static void tryexit();
static void doabort();
static Signal_t doabort_sig(int x);
static void all_done();
static Signal_t wakeup(int x);
static void set_sigs();


char *speedy_spawn_perl(
    SpeedyQueue *q, char *cmd, char **perl_argv, OptsRec *opts,
    PersistInfo *pinfo, char *envp[]
)
{
    char *errmsg;
    int lstn;

    /* Start listening before fork'ing */
    if ((errmsg = speedy_do_listen(q, pinfo, &lstn)))
	return errmsg;

    /* Fork and return if parent. */
    if (fork() != 0) {
	close(lstn);
	return NULL;
    }
    speedy_exec_perl(q, cmd, perl_argv, opts, pinfo, lstn, envp);
    exit(1);
    return "notreached";
}

void speedy_exec_perl(
    SpeedyQueue *q, char *cmd, char **perl_argv, OptsRec *opts,
    PersistInfo *pinfo, int lstn, char *envp[]
)
{
    int i, curdir;

    /* Copy into globals */
    g_q		= q;
    g_pinfo	= pinfo;

    /*
     * Bug was that the script "cgitest" would not work under mod_speedycgi.
     * Perl would not produce any output at all.  Something in the original
     * environment passed down from Apache was causing this -- zero'ing
     * out the apache envp fixes this.
     */
    envp[0] = NULL;

    /* We should be in our own session */
    setsid();

    /* Force the listener to fd-3 to stay clear of stdio */
    if (lstn != 3) {
	dup2(lstn, 3);
	close(lstn);
	lstn = 3;
    }

    /* Close off all I/O except for listener and stderr (close it later) */
    for (i = 32; i >= 0; --i) {
	if (i != lstn && i != 2) close(i);
    }

    /* Catch signals */
    set_sigs();

    /* Need to remember current directory */
    if ((curdir = open(".", O_RDONLY, 0)) != 1) {
	/* Make the directory fd 4 to keep it out of the way of stdio */
	if (curdir != 4) {
	    dup2(curdir, 4);
	    close(curdir);
	    curdir = 4;
	}
    }

    /* Read more args from #! line in perl script */
    speedy_addopts_file(opts, cmd, &perl_argv);

    /* Do it */
    doit(lstn, perl_argv, opts, curdir);
}

static void doit(
    int lstn, char **perl_argv, OptsRec *opts, int curdir
) {
    int s, e, len, numrun, maxruns;
    struct sockaddr_in sa;
    int mypid = getpid();
    PerlVars pv;

    /* Allocate new perl */
    g_perl = perl_alloc();
    perl_construct(g_perl);

    /* Parse perl file. */
    perl_parse(g_perl, xs_init, speedy_argc(perl_argv), perl_argv, NULL);

    /* Find perl variables */
    if (!(pv.pv_env    = perl_get_hv("ENV", 0))
     || !(pv.pv_argv   = perl_get_av("ARGV", 0))
     ||	!(pv.pv_stdin  = gv_fetchpv("STDIN", TRUE, SVt_PVIO))
     ||	!(pv.pv_stdout = gv_fetchpv("STDOUT", TRUE, SVt_PVIO))
     ||	!(pv.pv_stderr = gv_fetchpv("STDERR", TRUE, SVt_PVIO)))
    {
	doabort();
    }

    /* Create internal variable telling our library that we are really
     * SpeedyCGI
     */
    {
	SV *sv = perl_get_sv("CGI::SpeedyCGI::_i_am_speedy", TRUE);
	if (sv) sv_inc(sv);
    }

    /*
     * Options variables in our library.  If our module is not installed,
     * these may come back null.
     */
    if ((pv.pv_opts_changed = perl_get_sv("CGI::SpeedyCGI::_opts_changed", TRUE)))
	sv_setiv(pv.pv_opts_changed, 0);
    if ((pv.pv_opts = perl_get_hv("CGI::SpeedyCGI::_opts", TRUE))) {
	OptsRec *o;
	for (o = opts; o->name; ++o) {
	    SV *sv;
	    if (o->type == OTYPE_STR) {
		sv = newSVpvn(OVAL_STR(*o), strlen(OVAL_STR(*o)));
	    } else {
		sv = newSViv(OVAL_INT(*o));
	    }
	    hv_store(pv.pv_opts, o->name, strlen(o->name), sv, 0);
	}
    }

    /* Time to close stderr */
    do_close(pv.pv_stdin,  TRUE);
    do_close(pv.pv_stdout, TRUE);
    do_close(pv.pv_stderr, TRUE);

    /* We are not in the queue yet. Parent will connect without using queue. */
    g_queued = 0;

    for (numrun = 1;;numrun++) {

	/* Set timeout */
        if ((g_alarm = OVAL_INT(opts[OPT_TIMEOUT])) > 0) {
	    alarm(g_alarm);
	    rsignal(SIGALRM, &wakeup);
	}

	/* Accept a connection */
	len = sizeof(sa);
	if ((s = accept(lstn, (struct sockaddr*)&sa, &len)) == -1) doabort();
	g_queued = 0;
	if (s != 0) dup2(s,0);
	if (s != 1) dup2(s,1);
	if (s > 1) close(s);

	if ((e = accept(lstn, (struct sockaddr*)&sa, &len)) == -1) doabort();
	if (e != 2) {
	    dup2(e,2);
	    close(e);
	}

	/* Turn off timeout */
	if (g_alarm) {
	    alarm(0);
	    rsignal(SIGALRM, &doabort_sig);  
	    g_alarm = 0;
	}

	/* Do one run through the script. */
	onerun(g_q->secret_word, mypid, &pv, numrun);

	/* Make sure we cd back to the correct directory */
	if (curdir != -1) fchdir(curdir);

	/* See if we've gone over the maxruns */
	if ((maxruns = OVAL_INT(opts[OPT_MAXRUNS])) > 0) {
	    if (numrun >= maxruns) doabort();
	}

	/* See if we should get new options from the perl script */
	if (!pv.pv_opts_changed) {
	    pv.pv_opts_changed =
	        perl_get_sv("CGI::SpeedyCGI::_opts_changed", 0);
	}
	if (pv.pv_opts_changed && SvIV(pv.pv_opts_changed) != 0) {
	    if (!pv.pv_opts) {
		pv.pv_opts = perl_get_hv("CGI::SpeedyCGI::_opts", 0);
	    }
	    if (pv.pv_opts) {
		OptsRec *o;
		for (o = opts; o->name; ++o) {
		    SV **sv = hv_fetch(pv.pv_opts, o->name, strlen(o->name), 0);
		    if (sv) {
			if (o->type == OTYPE_STR) {
			    o->value = speedy_strdup(SvPV(*sv, PL_na));
			} else {
			    o->value = (void*)SvIV(*sv);
			}
		    }
		}
	    }
	    sv_setiv(pv.pv_opts_changed, 0);
	}

	/* Put ourself into the queue to wait for a connection. */
	if (speedy_q_add(g_q, g_pinfo)) doabort();
	g_queued = 1;
    }
}


/* One run of the perl process, do stdio using socket. */
static void onerun(int secret_word, int mypid, PerlVars *pv, int numrun) {
    int sz, cmd_done, par_secret;
    char *buf, *s;
    char *emptyargs[] = {NULL};
    PerlIO *pio_in, *pio_out, *pio_err;

    /* Set up stdio */
    if (!(pio_in  = PerlIO_fdopen(0, "r"))) doabort();
    if (!(pio_out = PerlIO_fdopen(1, "w"))) doabort();
    if (!(pio_err = PerlIO_fdopen(2, "w"))) doabort();

    IoIFP(GvIOp(pv->pv_stdin))  = IoOFP(GvIOp(pv->pv_stdin))  = pio_in;
    IoIFP(GvIOp(pv->pv_stdout)) = IoOFP(GvIOp(pv->pv_stdout)) = pio_out;
    IoIFP(GvIOp(pv->pv_stderr)) = IoOFP(GvIOp(pv->pv_stderr)) = pio_err;
    setdefout(pv->pv_stdout);

    /* Get secret word from parent. */
    if (PerlIO_read(pio_in, &par_secret, sizeof(int)) != sizeof(int))
	doabort();
    if (par_secret != secret_word && numrun > 1) {
	/* Don't check security on the first run.  Slightly less secure,
	 * but our parent should already be connecting to us very quickly
	 * after the socket begins listening.  Saves us from having to pass
	 * the secret word across an exec call (which is problematic).
	 */
	/* Security Alert! */
	sleep(10);
	doabort();
    }

    /* Get commands from parent. */
    for (cmd_done = 0; !cmd_done; ) {
	switch(PerlIO_getc(pio_in)) {
	case 'X':
	case -1:
	    /* Exit. */
	    doabort();
	case 'E':	/* %ENV */
	    /* Undef it */
	    hv_undef(pv->pv_env);

	    /* Read in environment from stdin. */
	    while ((sz = get_string(pio_in, &buf))) {

		/* Find equals. Store key/val in %ENV */
		if ((s = strchr(buf, '='))) {
		    int i = s - buf;
		    SV *sv = newSVpvn(s+1, sz-(i+1));
		    hv_store(pv->pv_env, buf, i, sv, 0);
		    *s = '\0';
		    my_setenv(buf, s+1);
		}
		Safefree(buf);
	    }
	    break;
	case 'A':	/* @ARGV */
	    /* Undef it. */
	    av_undef(pv->pv_argv);

	    /* Read in argv from stdin. */
	    while ((sz = get_string(pio_in, &buf))) {
		SV *sv = newSVpvn(buf, sz);
		av_push(pv->pv_argv, sv);
		Safefree(buf);
	    }
	    break;
	default:
	    /* Terminate commands. */
	    cmd_done = 1;
	    break;
	}
    }

    /* Run the perl code. */
    perl_run(g_perl);

    /* Terminate any forked children */
    if (getpid() != mypid) exit(0);

    /* Flush any stdout. */
    PerlIO_flush(PerlIO_stdout());
    PerlIO_flush(PerlIO_stderr());
                    
    /* Shutdown stdio */
    do_close(pv->pv_stdin,  TRUE);
    do_close(pv->pv_stdout, TRUE);
    do_close(pv->pv_stderr, TRUE);

    /* Get rid of any buffered stdin. */
    while (PerlIO_getc(PerlIO_stdin()) != -1)
        ;   

    /* Hack for CGI.pm */
    if (perl_get_cv(CGI_CLEANUP, 0)) {
	perl_call_argv(CGI_CLEANUP, G_DISCARD | G_NOARGS, emptyargs);
    }

    /* Reset signals */
    set_sigs();
}


/* Read in a string on stdin. */
static int get_string(PerlIO *pio_in, char **buf) {
    int sz;

    /* Read length of string */
    PerlIO_read(pio_in, &sz, sizeof(int));

    if (sz > 0) {
	/* Allocate space */
	New(123, *buf, sz+1, char);

	/* Read string and terminate */
	PerlIO_read(pio_in, *buf, sz);
	(*buf)[sz] = '\0';
    }
    return sz;
}


/* Wakeup on timeout */
static Signal_t wakeup(int x) {
    /* Try to exit */
    tryexit();

    /* Failed, try again later. */
    if (g_alarm > 0) alarm(g_alarm);
}


/* Try to exit cleanly.  If we are flagged as being in the queue, then */
/* we must still be listed there, otherwise someone is about to connect. */
static void tryexit() {
    if (!g_q || !g_pinfo || !g_queued || speedy_q_getme(g_q, g_pinfo) == NULL)
    {
	g_queued = 0;
	all_done();
    }
}


/* Exit unconditionally.  Try to cleanup first. */
static void doabort() {
    tryexit();
    all_done();
}

static Signal_t doabort_sig(int x) {
    doabort();
}

/* Shutdown and exit. */
static void all_done() {
    if (g_q) speedy_q_destroy(g_q);
    if (g_perl) {
	SV *sv = perl_get_sv("CGI::SpeedyCGI::_shutdown_handler", 0);
	if (sv) {
	    dSP;
	    PUSHMARK(SP);
	    perl_call_sv(sv, G_DISCARD | G_NOARGS);
	}
	perl_destruct(g_perl);
	perl_free(g_perl);
    }
    exit(0);
}

/* Catch pesky signals */
static void set_sigs() {
    rsignal(SIGPIPE, &doabort_sig);
    rsignal(SIGALRM, &doabort_sig);
    rsignal(SIGTERM, &doabort_sig);
    rsignal(SIGHUP,  &doabort_sig);
    rsignal(SIGINT,  &doabort_sig);
}
