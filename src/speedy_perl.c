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

typedef struct {
    GV	*pv_stdin;
    GV	*pv_stdout;
    GV	*pv_stderr;
    HV	*pv_env;
    AV	*pv_argv;
    SV	*pv_opts_changed;
    HV	*pv_opts;
} PerlVars;

static PerlInterpreter	*g_perl;	/* Perl interpreter */
static slotnum_t	gslotnum, bslotnum;
static PerlVars		pv;

static int get_pv_opts_changed(int create) {
    int retval = 0;

    if (!pv.pv_opts_changed) {
	pv.pv_opts_changed =
	    perl_get_sv("CGI::SpeedyCGI::_opts_changed", create);
    }
    if (pv.pv_opts_changed) {
	retval = SvIV(pv.pv_opts_changed);
	sv_setiv(pv.pv_opts_changed, 0);
    }
    return retval;
}

static int get_pv_opts(int create) {
    if (!pv.pv_opts) {
	pv.pv_opts = perl_get_hv("CGI::SpeedyCGI::_opts", create);
    }
    return pv.pv_opts != NULL;
}

/* Remove myself from the temp file */
static void remove_from_temp() {
    if (bslotnum && gslotnum) {
	speedy_file_set_state(FS_WRITING);
	speedy_backend_check_next(gslotnum, bslotnum);
	speedy_backend_dispose(gslotnum, bslotnum);
	speedy_group_cleanup(gslotnum);
	bslotnum = gslotnum = 0;
    }
}

/* Shutdown and exit. */
static void all_done(int exec_myself) {

    /* Re-open the file to make sure we're not going to remove a temp
     * file belonging to a newer backend using this slot in a newer
     * version of this file
     */
    speedy_file_set_state(FS_HAVESLOTS);
    speedy_file_need_reopen();

    if (exec_myself) {
	if (bslotnum) {
	    /* Lower the maturity level for this slot */
	    speedy_file_set_state(FS_WRITING);
	    FILE_SLOT(be_slot, bslotnum).maturity = 0;
	}
    } else {
	remove_from_temp();
    }
    speedy_file_set_state(FS_CLOSED);

    /* Call the shutdown handler if present */
    if (g_perl) {
	SV *sv = perl_get_sv("CGI::SpeedyCGI::_shutdown_handler", 0);
	if (sv && SvPV(sv, PL_na)[0]) {
	    dSP;
	    PUSHMARK(SP);
	    perl_call_sv(sv, G_DISCARD | G_NOARGS);
	}
	perl_destruct(g_perl);
	perl_free(g_perl);
    }
    if (exec_myself) {
	speedy_util_execvp((speedy_opt_orig_argv()[0]), speedy_opt_orig_argv());
	remove_from_temp();
    }
    exit(0);
}

/* Try to exit cleanly.  If someone is trying to connect on our socket,
 * then don't exit now.
 */
static void tryexit() {
    if (speedy_ipc_accept_ready(0)) 
	return;
    all_done(0);
}

/* Signal handler */
static Signal_t doabort_sig(int x) {
    all_done(0);
}

/* Wiat for a connection from a frontend */
static void backend_accept() {
    int s, e;

    /* Loop forever */
    while (1) {

	/* Wait for an accept or timeout */
	if (speedy_ipc_accept(OPTVAL_TIMEOUT*1000, &s, &e)) {
	    /* Got an accept */
	    dup2(s, 1);
	    return;
	}

	/* Timed out, try to exit */
	tryexit();
    }
}


/* Read in a string on stdin. */
static int get_string(PerlIO *pio_in, char **buf) {
    int sz;

    /* Read length of string */
    PerlIO_read(pio_in, &sz, sizeof(int));

    if (sz > 0) {
	/* Allocate space */
	*buf = speedy_malloc(sz+1);

	/* Read string and terminate */
	PerlIO_read(pio_in, *buf, sz);
	(*buf)[sz] = '\0';
    } else {
	*buf = NULL;
    }
    return sz;
}

/* Catch pesky signals */
static void set_sigs() {
    rsignal(SIGPIPE, &doabort_sig);
    rsignal(SIGALRM, &doabort_sig);
    rsignal(SIGTERM, &doabort_sig);
    rsignal(SIGHUP,  &doabort_sig);
    rsignal(SIGINT,  &doabort_sig);
}


/* One run of the perl process, do stdio using socket. */
static void onerun(pid_t mypid, int curdir) {
    int sz, cmd_done;
    char *buf, *s;
    char *emptyargs[] = {NULL};

    /* Set up perl STD* filehandles to have the PerlIO file pointers */
    IoIFP(GvIOp(pv.pv_stdin))  = IoOFP(GvIOp(pv.pv_stdin))  = PerlIO_stdin();
    IoIFP(GvIOp(pv.pv_stdout)) = IoOFP(GvIOp(pv.pv_stdout)) = PerlIO_stdout();
    IoIFP(GvIOp(pv.pv_stderr)) = IoOFP(GvIOp(pv.pv_stderr)) = PerlIO_stderr();

    /* Do "select STDOUT" */
    setdefout(pv.pv_stdout);

    /* Get commands from parent. */
    for (cmd_done = 0; !cmd_done; ) {
	switch(PerlIO_getc(PerlIO_stdin())) {
	case -1:
	    /* Exit. */
	    DIE_QUIET("speedy protocol error");
	    break;
	case 'E':	/* %ENV */
	    /* Undef it */
	    hv_undef(pv.pv_env);

	    /* Read in environment from stdin. */
	    while ((sz = get_string(PerlIO_stdin(), &buf))) {

		/* Find equals. Store key/val in %ENV */
		if ((s = strchr(buf, '='))) {
		    int i = s - buf;
		    SV *sv = newSVpvn(s+1, sz-(i+1));
		    hv_store(pv.pv_env, buf, i, sv, 0);
		    *s = '\0';
		    my_setenv(buf, s+1);
		}
		speedy_free(buf);
	    }
	    break;
	case 'A':	/* @ARGV */
	    /* Undef it. */
	    av_undef(pv.pv_argv);

	    /* Read in argv from stdin. */
	    while ((sz = get_string(PerlIO_stdin(), &buf))) {
		SV *sv = newSVpvn(buf, sz);
		av_push(pv.pv_argv, sv);
		speedy_free(buf);
	    }
	    break;
	default:
	    /* Terminate commands. */
	    cmd_done = 1;
	    break;
	}
    }

    /* Run the perl code.  Ignore return value since we want to stay persistent
     * no matter what the return code
     */
    (void) perl_run(g_perl);

    /* Terminate any forked children */
    if (getpid() != mypid) exit(0);

    /* Flush output, in case perl's stdio/reopen below don't */
    PerlIO_flush(PerlIO_stdout());
    PerlIO_flush(PerlIO_stderr());

    /* Close down perl's STD* files (might not be the same as PerlIO files) */
    do_close(pv.pv_stdout, FALSE);
    do_close(pv.pv_stderr, FALSE);
    do_close(pv.pv_stdin,  FALSE);

    /* Get stdio files back in shape */
    PerlIO_reopen("/dev/null", "r", PerlIO_stdin());
    PerlIO_reopen("/dev/null", "w", PerlIO_stdout());
    PerlIO_reopen("/dev/null", "w", PerlIO_stderr());
    close(0); close(1); close(2);

    /* Reset signals */
    set_sigs();

    /* Hack for CGI.pm */
    if (perl_get_cv(CGI_CLEANUP, 0)) {
	perl_call_argv(CGI_CLEANUP, G_DISCARD | G_NOARGS, emptyargs);
    }
}

void speedy_perl_run(slotnum_t _gslotnum, slotnum_t _bslotnum)
{
    extern void xs_init();
    int curdir, numrun;
    pid_t mypid = getpid();
    char **perl_argv;

    /* Copy into globals */
    gslotnum	= _gslotnum;
    bslotnum	= _bslotnum;

    /* Catch signals */
    set_sigs();

    /* Need to remember current directory */
    curdir = speedy_util_pref_fd(open(".", O_RDONLY, 0), PREF_FD_CWD);

    /* Allocate new perl */
    if (!(g_perl = perl_alloc())) {
	DIE_QUIET("Cannot allocate perl");
    }
    perl_construct(g_perl);

    /* Parse perl file. */
    perl_argv = speedy_opt_perl_argv();
    if (
	perl_parse(
	    g_perl, xs_init, speedy_util_argc((const char * const *)perl_argv),
	    perl_argv, NULL
	)
    )
    {
	DIE_QUIET("perl_parse error");
    }

    /* Find perl variables */
    if (!(pv.pv_env    = perl_get_hv("ENV", 0))
     || !(pv.pv_argv   = perl_get_av("ARGV", 0))
     ||	!(pv.pv_stdin  = gv_fetchpv("STDIN", TRUE, SVt_PVIO))
     ||	!(pv.pv_stdout = gv_fetchpv("STDOUT", TRUE, SVt_PVIO))
     ||	!(pv.pv_stderr = gv_fetchpv("STDERR", TRUE, SVt_PVIO)))
    {
	DIE_QUIET("Cannot locate perl globs (ENV, ARGV, etc)");
    }

    /* Create internal variable telling our library that we are really
     * SpeedyCGI
     */
    {
	SV *sv = perl_get_sv("CGI::SpeedyCGI::_i_am_speedy", TRUE);
	if (sv) sv_inc(sv);
    }

    /*
     * Options variables in our module.  If our module is not installed,
     * these may come back null.
     */
    (void) get_pv_opts_changed(1);
    if (get_pv_opts(1)) {
	int i;
	SV *sv;
	OptRec *o;

	for (i = 0; i < SPEEDY_NUMOPTS; ++i) {
	    o = speedy_optdefs + i;
	    if (o->type == OTYPE_STR) {
		if (!o->value) continue;
		sv = newSVpvn((char*)o->value, strlen((char*)o->value));
	    } else {
		sv = newSViv((int)o->value);
	    }
	    hv_store(pv.pv_opts, o->name, strlen(o->name), sv, 0);
	}
    }

    /* Time to close stderr */
    close(2);

    /* Start listening on our socket */
    speedy_ipc_listen(bslotnum);

    /* Main loop */
    for (numrun = 1; ; ++numrun) {
	
	/* Lock/mmap our temp file */
	speedy_file_set_state(FS_WRITING);

	/* If our group is invalid, exit quietly */
	if (!speedy_group_isvalid(gslotnum)) {
	    gslotnum = 0;
	    speedy_file_set_state(FS_HAVESLOTS);
	    all_done(0);
	}
	
	/* Check our backend siblings */
	speedy_backend_check_next(gslotnum, bslotnum);
	
	/* Update our maturity level */
	FILE_SLOT(be_slot, bslotnum).maturity = numrun > 1 ? 2 : 1;

	/* Put ourself onto the be_wait list */
	speedy_backend_be_wait_put(gslotnum, bslotnum);

	/* Send out alarm signal to frontends */
	speedy_group_sendsigs(gslotnum);

	/* Fix our listener fd */
	speedy_ipc_listen_fixfd(bslotnum);

	/* Unlock file */
	speedy_file_set_state(FS_HAVESLOTS);

	/* Do an accept on our socket */
	backend_accept();

	/* Run the perl code once */
	onerun(mypid, curdir);

	/* Tell our file code that its fd is suspect */
	speedy_file_fd_is_suspect();

	/* Make sure we cd back to the correct directory */
	if (curdir != -1) fchdir(curdir);

	/* See if we've gone over the maxruns */
	if (OPTVAL_MAXRUNS && numrun >= OPTVAL_MAXRUNS) {
	    close(curdir);
	    all_done(1);
	}

	/* See if we should get new options from the perl script */
	if (get_pv_opts_changed(0) && get_pv_opts(0)) {
	    int i;
	    SV **sv;
	    OptRec *o;

	    for (i = 0; i < SPEEDY_NUMOPTS; ++i) {
		o = speedy_optdefs + i;
		if ((sv = hv_fetch(pv.pv_opts, o->name, strlen(o->name), 0))) {
		    (void) speedy_opt_set(o, SvPV(*sv, PL_na));
		}
	    }
	}
    }
}

/*
 * Glue
 */

void speedy_abort(const char *s) {
    PerlIO_puts(PerlIO_stderr(), s);
    exit(1);
}
