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
/* ====================================================================
 * Copyright (c) 1995-1999 The Apache Group.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */

/*
 * http_script: keeps all script-related ramblings together.
 * 
 * Compliant to CGI/1.1 spec
 * 
 * Adapted by rst from original NCSA code by Rob McCool
 *
 * Apache adds some new env vars; REDIRECT_URL and REDIRECT_QUERY_STRING for
 * custom error responses, and DOCUMENT_ROOT because we found it useful.
 * It also adds SERVER_ADMIN - useful for scripts to know who to mail when 
 * they fail.
 */

#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_log.h"
#include "util_script.h"
#include "http_conf_globals.h"
#include "libspeedy.h"

/*
 * Interface between libspeedy and C-lib.  If the C-lib functions have
 * been overridden with macros, then make a function that calls the macro.
 * Otherwise, use the C-lib function directly.
 */
#ifdef memcpy
    static void ls_memcpy(void *d, void *s, int n) {memcpy(d,s,n);}
#else
#   define ls_memcpy memcpy
#endif

#ifdef memmove
    static void ls_memmove(void *d, void *s, int n) {memmove(d,s,n);}
#else
#   define ls_memmove memmove
#endif

static void ls_bzero(void *s, int n) {memset(s, 0, n);}

#ifdef malloc
    static void *ls_malloc(int n) {return malloc(n);}
#else
#   define ls_malloc malloc
#endif

#ifdef free
    static void ls_free(void *s) {free(s);}
#else
#   define ls_free free
#endif

/*
 * This global var is used by libspeedy to find the above functions
 */
LS_funcs speedy_libfuncs = {
    ls_memcpy,
    ls_memmove,
    ls_bzero,
    ls_malloc,
    ls_free,
};

/* End of libspeedy interface info */


module MODULE_VAR_EXPORT speedycgi_module;

/* Configuration stuff */

#define OPT_TIMEOUT 0
#define OPT_MAXRUNS 1
#define OPT_TMPBASE 2
#define NUM_OPTS 3
static char *opt_switches[NUM_OPTS] = {"-t", "-r", "-T"};

static void *create_cgi_config(pool *p, server_rec *s)
{
    /* Create an array of char*'s to hold the options */
    return (void*)ap_pcalloc(p, NUM_OPTS * sizeof(char*));
}

static const char *set_option(cmd_parms *cmd, void *dummy, char *arg)
{
    char **opts = (char **)ap_get_module_config(
        cmd->server->module_config, &speedycgi_module
    );
    /* cmd->info contains one of the OPT_ numbers */
    opts[(int)(cmd->info)] = arg;
    return NULL;
}

static const command_rec cgi_cmds[] =
{
    {"SpeedyTimeout", set_option, (void*)OPT_TIMEOUT, OR_ALL, TAKE1,
     "the amount of time a perl interpreter will wait for a new request"},
    {"SpeedyMaxruns", set_option, (void*)OPT_MAXRUNS, OR_ALL, TAKE1,
     "the maximum number of runs a perl interpreter will make before exiting"},
    {"SpeedyTmpbase", set_option, (void*)OPT_TMPBASE, OR_ALL, TAKE1,
     "the base filename used to enqueue speedycgi perl interpreters"},
    {NULL}
};

static int log_scripterror(request_rec *r, int ret, int show_errno, char *error)
{
    ap_log_rerror(APLOG_MARK, show_errno|APLOG_ERR, r, 
		"%s: %s", error, r->filename);
    return ret;
}

/****************************************************************
 *
 * Actual CGI handling...
 */


struct cgi_child_stuff {
    request_rec *r;
    int lstn;
    char **opts;
};

static int cgi_child(void *child_stuff, child_info *pinfo)
{
    struct cgi_child_stuff *cld = (struct cgi_child_stuff *) child_stuff;
    request_rec *r = cld->r;
    int child_pid, argc, did_dashdash, i;
    char *argv[NUM_OPTS+5];
    char **opts = cld->opts;

    RAISE_SIGSTOP(CGI_CHILD);

#ifndef WIN32
    ap_chdir_file(r->filename);
#endif

    ap_cleanup_for_exec();

    /* Fill in argv */
    argv[0] = ""; argc = 1;
    did_dashdash = 0;
    for (i = 0; i < NUM_OPTS; ++i) {
	if (opts[i]) {
	    if (!did_dashdash++) argv[argc++] = "--";
	    argv[argc++] = ap_psprintf(
	        r->pool, "%s%s", opt_switches[i], opts[i]
	    );
	}
    }
    argv[argc++] = r->filename;
    argv[argc++] = NULL;

    /* Avoid zombies */
    if (fork() > 0) _exit(0);

    /* Exec the perl backend */
    speedy_exec_client(argv, cld->lstn);
    child_pid = getpid();
#ifdef WIN32
    return (child_pid);
#else

    /* Uh oh.  Still here.  Where's the kaboom?  There was supposed to be an
     * EARTH-shattering kaboom!
     *
     * Oh, well.  Muddle through as best we can...
     *
     * Note that only stderr is available at this point, so don't pass in
     * a server to aplog_error.
     */

    ap_log_error(APLOG_MARK, APLOG_ERR, NULL, "exec of %s failed", r->filename);
    exit(0);
    /* NOT REACHED */
    return (0);
#endif
}

static int cgi_handler(request_rec *r)
{
    int retval, nph, dbpos = 0, s, e;
    BUFF *script_io, *script_err;
    int is_included = !strcmp(r->protocol, "INCLUDED");
    SpeedyQueue q;
    struct timeval start_time;
    char *err, **env, *argsbuffer, *argv0;
    char **opts = (char**)ap_get_module_config(
	r->server->module_config, &speedycgi_module
    );
    struct cgi_child_stuff cld;
    char *tmpbase = opts[OPT_TMPBASE] ? opts[OPT_TMPBASE] : "/tmp/speedy";

    if (r->method_number == M_OPTIONS) {
	/* 99 out of 100 CGI scripts, this is all they support */
	r->allowed |= (1 << M_GET);
	r->allowed |= (1 << M_POST);
	return DECLINED;
    }

    if ((argv0 = strrchr(r->filename, '/')) != NULL)
	argv0++;
    else
	argv0 = r->filename;

    nph = !(strncmp(argv0, "nph-", 4));

    if (!(ap_allow_options(r) & OPT_EXECCGI))
	return log_scripterror(r, FORBIDDEN, APLOG_NOERRNO,
			       "Options ExecCGI is off in this directory");
    if (nph && is_included)
	return log_scripterror(r, FORBIDDEN, APLOG_NOERRNO,
			       "attempt to include NPH CGI script");

#if defined(OS2) || defined(WIN32)
    /* Allow for cgi files without the .EXE extension on them under OS/2 */
    if (r->finfo.st_mode == 0) {
	struct stat statbuf;
	char *newfile;

	newfile = ap_pstrcat(r->pool, r->filename, ".EXE", NULL);

	if ((stat(newfile, &statbuf) != 0) || (!S_ISREG(statbuf.st_mode))) {
	    return log_scripterror(r, NOT_FOUND, 0,
				   "script not found or unable to stat");
	} else {
	    r->filename = newfile;
	}
    }
#else
    if (r->finfo.st_mode == 0)
	return log_scripterror(r, NOT_FOUND, APLOG_NOERRNO,
			       "script not found or unable to stat");
#endif
    if (S_ISDIR(r->finfo.st_mode))
	return log_scripterror(r, FORBIDDEN, APLOG_NOERRNO,
			       "attempt to invoke directory as script");
    if (!ap_suexec_enabled) {
	if (!ap_can_exec(&r->finfo))
	    return log_scripterror(r, FORBIDDEN, APLOG_NOERRNO,
				   "file permissions deny server execution");
    }

    if ((retval = ap_setup_client_block(r, REQUEST_CHUNKED_ERROR)))
	return retval;

    ap_add_common_vars(r);
    cld.r = r;
    cld.opts = opts;

#ifdef CHARSET_EBCDIC
    /* XXX:@@@ Is the generated/included output ALWAYS in text/ebcdic format? */
    /* Or must we check the Content-Type first? */
    ap_bsetflag(r->connection->client, B_EBCDIC2ASCII, 1);
#endif /*CHARSET_EBCDIC*/

    /* Create a speedy Queue object */
    start_time.tv_sec = start_time.tv_usec = r->request_time;
    err = speedy_q_init(&q, tmpbase, r->filename, &start_time, &(r->finfo));
    if (err) {
	ap_log_rerror(APLOG_MARK, APLOG_ERR, r,
		"couldn't open speedydcgi queue: %s", err);
	return HTTP_INTERNAL_SERVER_ERROR;
    }

    /* Find a speedy client to connect to */
    err = speedy_getclient(&q, &s, &e);

    /*
     * There was no client, so start a listening socket, then fork/exec
     * a backend that we can talk to
     */
    if (err) {
	PersistInfo pinfo;
	int lstn;

	/*
	 * Start a listener
	 */
	if (err = speedy_do_listen(&q, &pinfo, &lstn)) {
	    ap_log_rerror(APLOG_MARK, APLOG_ERR, r,
		    "couldn't start speedydcgi listener: %s", err);
	    return HTTP_INTERNAL_SERVER_ERROR;
	}
	cld.lstn = lstn;

	/*
	 * we spawn out of r->main if it's there so that we can avoid
	 * waiting for free_proc_chain to cleanup in the middle of an
	 * SSI request -djg
	 */
	if (!ap_bspawn_child(r->main ? r->main->pool : r->pool, cgi_child,
			     (void *) &cld, kill_never,
			     NULL, NULL, NULL)) {
	    ap_log_rerror(APLOG_MARK, APLOG_ERR, r,
			"couldn't spawn child process: %s", r->filename);
	    return HTTP_INTERNAL_SERVER_ERROR;
	}

	/* Listener is now in the child process, so safe to close here */
	close(lstn);

	/* Connect up with child (go direct, do not use queue) */
	if (err = speedy_comm_init(pinfo.port, &s, &e)) {
	    ap_log_rerror(APLOG_MARK, APLOG_ERR, r,
			"couldn't connect to speedycgi backend: %s", err);
	    return HTTP_INTERNAL_SERVER_ERROR;
	}
    }

    /* Free up Speedy Queue */
    speedy_q_free(&q);

    /*
     * Open up buffered files -- "s" contains stdin/stdout, "e" is stderr
     */
    /* stdin/stdout to script (stdin from script's perspective) */
    script_io = ap_bcreate(r->pool, B_RDWR|B_SOCKET);
    ap_note_cleanups_for_fd(r->pool, s);
    ap_bpushfd(script_io, s, s);

    /* stderr from script */
    script_err = ap_bcreate(r->pool, B_RD|B_SOCKET);
    ap_note_cleanups_for_fd(r->pool, e);
    ap_bpushfd(script_err, e, e);


    /* Do speedy protocol - send over enviorment, etc */
    {
	char *argv[2], **env;
	int sendenv_size;
	int alloc_size = HUGE_STRING_LEN;

	/* Get environment */
	ap_add_cgi_vars(r);
	env = ap_create_environment(r->pool, r->subprocess_env);

	/* Fill in argsbuffer with the data */
	argv[0] = argv[1] = NULL;
	sendenv_size = speedy_sendenv_size(env, argv);
	if (sendenv_size > alloc_size) alloc_size = sendenv_size;
	argsbuffer = ap_palloc(r->pool, alloc_size);
	speedy_sendenv_fill(argsbuffer, env, argv, q.secret_word);

	/* Send it over */
	ap_bwrite(script_io, argsbuffer, sendenv_size);
    }

    /* Transfer any put/post args, CERN style...
     * Note that we already ignore SIGPIPE in the core server.
     */

    if (ap_should_client_block(r)) {
	int dbsize, len_read;

	ap_hard_timeout("copy script args", r);

	while ((len_read =
		ap_get_client_block(r, argsbuffer, HUGE_STRING_LEN)) > 0) {
	    ap_reset_timeout(r);
	    if (ap_bwrite(script_io, argsbuffer, len_read) < len_read) {
		/* silly script stopped reading, soak up remaining message */
		while (ap_get_client_block(r, argsbuffer, HUGE_STRING_LEN) > 0) {
		    /* dump it */
		}
		break;
	    }
	}

	ap_bflush(script_io);

	ap_kill_timeout(r);
    }

    ap_bflush(script_io);
    shutdown(s, 1);

    /* Handle script return... */
    if (script_io && !nph) {
	const char *location;
	char sbuf[MAX_STRING_LEN];
	int ret;

	if ((ret = ap_scan_script_header_err_buff(r, script_io, sbuf))) {
	    return ret;
	}

#ifdef CHARSET_EBCDIC
        /* Now check the Content-Type to decide if conversion is needed */
        ap_checkconv(r);
#endif /*CHARSET_EBCDIC*/

	location = ap_table_get(r->headers_out, "Location");

	if (location && location[0] == '/' && r->status == 200) {

	    /* Soak up all the script output */
	    ap_hard_timeout("read from script", r);
	    while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_io) > 0) {
		continue;
	    }
	    while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_err) > 0) {
		continue;
	    }
	    ap_kill_timeout(r);


	    /* This redirect needs to be a GET no matter what the original
	     * method was.
	     */
	    r->method = ap_pstrdup(r->pool, "GET");
	    r->method_number = M_GET;

	    /* We already read the message body (if any), so don't allow
	     * the redirected request to think it has one.  We can ignore 
	     * Transfer-Encoding, since we used REQUEST_CHUNKED_ERROR.
	     */
	    ap_table_unset(r->headers_in, "Content-Length");

	    ap_internal_redirect_handler(location, r);
	    return OK;
	}
	else if (location && r->status == 200) {
	    /* XX Note that if a script wants to produce its own Redirect
	     * body, it now has to explicitly *say* "Status: 302"
	     */
	    return REDIRECT;
	}

	ap_send_http_header(r);
	if (!r->header_only) {
	    ap_send_fb(script_io, r);
	}
	ap_bclose(script_io);

	ap_soft_timeout("soaking script stderr", r);
	while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_err) > 0) {
	    continue;
	}
	ap_kill_timeout(r);
	ap_bclose(script_err);
    }

    if (script_io && nph) {
	ap_send_fb(script_io, r);
    }

    return OK;			/* NOT r->status, even if it has changed. */
}

static const handler_rec cgi_handlers[] =
{
    {"speedycgi-script", cgi_handler},
    {NULL}
};

module MODULE_VAR_EXPORT speedycgi_module =
{
    STANDARD_MODULE_STUFF,
    NULL,			/* initializer */
    NULL,			/* dir config creater */
    NULL,			/* dir merger --- default is to override */
    create_cgi_config,		/* server config */
    NULL,			/* merge server config */
    cgi_cmds,			/* command table */
    cgi_handlers,		/* handlers */
    NULL,			/* filename translation */
    NULL,			/* check_user_id */
    NULL,			/* check auth */
    NULL,			/* check access */
    NULL,			/* type_checker */
    NULL,			/* fixups */
    NULL,			/* logger */
    NULL,			/* header parser */
    NULL,			/* child_init */
    NULL,			/* child_exit */
    NULL			/* post read-request */
};
