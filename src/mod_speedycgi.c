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

/* Based on apache's mod_cgi.c */

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

#include "speedy.h"

module MODULE_VAR_EXPORT speedycgi_module;

static request_rec *global_r;

/* Configuration stuff */

static int log_scripterror(request_rec *r, int ret, int show_errno, char *error)
{
    ap_log_rerror(APLOG_MARK, show_errno|APLOG_ERR, r, 
		"%s: %s", error, r->filename);
    return ret;
}

/* set_option is used as a callback in the cgi_cmds array */
static const char *set_option(cmd_parms *cmd, void *dummy, char *arg)
{
    /* cmd->info is a pointer to the OptRec */
    speedy_opt_set((OptRec*)cmd->info, arg);
    return NULL;
}

/* Include the definition of the cgi_cmds array */
#include "mod_speedycgi_cmds.c"

/****************************************************************
 *
 * Actual CGI handling...
 */


static int cgi_handler(request_rec *r)
{
    int retval, nph, s, e;
    BUFF *script_io, *script_err;
    int is_included = !strcmp(r->protocol, "INCLUDED");
    char *argsbuffer, *argv0, *script_argv[2];
    static int did_opt_init;
    int sendenv_size, alloc_size;

    /* Copy request_rec to global */
    global_r = r;

    /* Initialize speedy options 
     * Must be done *after* the config file speedy_opt_set calls (above)
     */
    if (!did_opt_init) {
	extern char **environ;
	const char *prog_argv[2];

	prog_argv[0] = "";
	prog_argv[1] = NULL;
	speedy_opt_init(
	    (const char * const *)prog_argv, (const char * const *)environ
	);
	did_opt_init = 1;
    }

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

    /* Put the CGI environment vars into r */
    ap_add_common_vars(r);
    ap_add_cgi_vars(r);

#ifdef CHARSET_EBCDIC
    /* Is the generated/included output ALWAYS in text/ebcdic format? */
    /* Or must we check the Content-Type first? */
    ap_bsetflag(r->connection->client, B_EBCDIC2ASCII, 1);
#endif /*CHARSET_EBCDIC*/

    /* Connect up to a speedycgi backend, creating a new one if necessary */
    script_argv[0] = r->filename;
    script_argv[1] = NULL;
    speedy_opt_set_script_argv((const char * const *)script_argv);
    speedy_frontend_connect(&(r->finfo), &s, &e);
    speedy_file_close();

    /*
     * Open up buffered files -- "s" contains stdin/stdout, "e" is stderr
     */
    /* stdin/stdout for script */
    script_io = ap_bcreate(r->pool, B_RDWR|B_SOCKET);
    ap_note_cleanups_for_fd(r->pool, s);
    ap_bpushfd(script_io, s, s);

    /* stderr from script */
    script_err = ap_bcreate(r->pool, B_RD|B_SOCKET);
    ap_note_cleanups_for_fd(r->pool, e);
    ap_bpushfd(script_err, e, e);

    /* Fill in argsbuffer with the env/argv data to send */
    argsbuffer = speedy_frontend_mkenv(
	(const char * const *)ap_create_environment(r->pool, r->subprocess_env),
	(const char * const *)script_argv,
	HUGE_STRING_LEN, 0,
	&sendenv_size, &alloc_size
    );

    /* Send over env/argv data */
    ap_bwrite(script_io, argsbuffer, sendenv_size);

    /* Transfer any put/post args, CERN style...
     * Note that we already ignore SIGPIPE in the core server.
     */

    if (ap_should_client_block(r)) {
	int len_read;

	ap_hard_timeout("copy script args", r);

	while ((len_read =
		ap_get_client_block(r, argsbuffer, alloc_size)) > 0) {
	    ap_reset_timeout(r);
	    if (ap_bwrite(script_io, argsbuffer, len_read) < len_read) {
		/* silly script stopped reading, soak up remaining message */
		while (ap_get_client_block(r, argsbuffer, alloc_size) > 0) {
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
	    while (ap_bgets(argsbuffer, alloc_size, script_io) > 0) {
		continue;
	    }
	    while (ap_bgets(argsbuffer, alloc_size, script_err) > 0) {
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
	while (ap_bgets(argsbuffer, alloc_size, script_err) > 0) {
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
    NULL,			/* server config */
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


/*
 * Glue Functions
 */

void speedy_abort(const char *s) {
    ap_log_error(APLOG_MARK, APLOG_ERR, NULL, "mod_speedycgi failed: %s", s);
    exit(1);
}

int speedy_execvp(const  char *filename, const char *const *argv)
{
    RAISE_SIGSTOP(CGI_CHILD);

#ifndef WIN32
    if (global_r)
	ap_chdir_file(global_r->filename);
#endif

    ap_cleanup_for_exec();

    return execvp(filename, (char *const*)argv);
}
