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

static struct timeval saved_time;

int speedy_util_pref_fd(int oldfd, int newfd) {
    if (oldfd != newfd && oldfd != -1 && newfd != -1) {
	(void) dup2(oldfd, newfd);
	(void) close(oldfd);
	return newfd;
    } else {
	return oldfd;
    }
}

static int euid = -1;

int speedy_util_geteuid() {
    if (euid == -1)
	euid = geteuid();
    return euid;
}

int speedy_util_seteuid(int id) {
    int retval = seteuid(id);
    if (retval != -1)
	euid = id;
    return retval;
}

int speedy_util_getuid() {
    static int uid = -1;
    if (uid == -1)
	uid = getuid();
    return uid;
}

int speedy_util_argc(const char * const * argv) {
    int retval;
    for (retval = 0; *argv++; ++retval) 
	;
    return retval;
}

int speedy_util_getpid() {
    static int saved_pid;
    if (!saved_pid) saved_pid = getpid();
    return saved_pid;
}

int speedy_util_kill(pid_t pid, int sig) {
    return (pid && pid != speedy_util_getpid()) ? kill(pid, sig) : 0;
}

static void just_die(const char *fmt, va_list ap) {
    extern int errno;
    char buf[2048];

    sprintf(buf, "%s[%d]: ", SPEEDY_PROGNAME, (int)getpid());
    vsprintf(buf + strlen(buf), fmt, ap);
    if (errno) {
	strcat(buf, ": ");
	strcat(buf, strerror(errno));
    }
    strcat(buf, "\n");
    speedy_abort(buf);
}

void speedy_util_die(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    just_die(fmt, ap);
    va_end(ap);
}

void speedy_util_die_quiet(const char *fmt, ...) {
    va_list ap;

    errno = 0;
    va_start(ap, fmt);
    just_die(fmt, ap);
    va_end(ap);
}

int speedy_util_execvp(const char *filename, const char *const *argv) {
    extern char **environ;

    /* Get original argv */
    environ = (char **)speedy_opt_exec_envp();

    /* Exec the backend */
    return speedy_execvp(filename, argv);
}

char *speedy_util_strndup(const char *s, int len) {
    char *buf = speedy_malloc(len+1);
    speedy_memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

void speedy_util_gettimeofday(struct timeval *tv) {
    if (!saved_time.tv_sec)
	gettimeofday(&saved_time, NULL);
    tv->tv_sec = saved_time.tv_sec;
    tv->tv_usec = saved_time.tv_usec;
}

time_t speedy_util_time() {
    struct timeval tv;
    speedy_util_gettimeofday(&tv);
    return tv.tv_sec;
}

void speedy_util_time_invalidate() {
    saved_time.tv_sec = 0;
}

char *speedy_util_fname(int num, char type) {
    char *fname = (char*) speedy_malloc(strlen(OPTVAL_TMPBASE) + 80);
    int uid = speedy_util_getuid(), euid = speedy_util_geteuid();

    if (euid == uid)
	sprintf(fname, "%s.%x.%x.%c", OPTVAL_TMPBASE, num, euid, type);
    else
	sprintf(fname, "%s.%x.%x.%x.%c", OPTVAL_TMPBASE, num, euid, uid, type);

    return fname;
}
