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

int speedy_util_pref_fd(int oldfd, int newfd) {
    if (oldfd != newfd && oldfd != -1 && newfd != -1) {
	(void) dup2(oldfd, newfd);
	(void) close(oldfd);
	return newfd;
    } else {
	return oldfd;
    }
}

int speedy_util_geteuid() {
    static int euid = -1;
    if (euid == -1) euid = geteuid();
    return euid;
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

void speedy_util_die(const char *fmt, ...) {
    extern int errno;
    char buf[2048];
    va_list ap;

    sprintf(buf, "%s[%d]: ", MY_NAME, (int)getpid());
    va_start(ap, fmt);
    vsprintf(buf + strlen(buf), fmt, ap);
    va_end(ap);
    if (errno) {
	strcat(buf, ": ");
	strcat(buf, strerror(errno));
    }
    strcat(buf, "\n");
    speedy_abort(buf);
}

int speedy_util_execvp(const char *filename, const char *const *argv) {
    extern char **environ;

    /* Get original argv */
    environ = (char **)speedy_opt_exec_envp();

    /* Exec the backend */
    return speedy_execvp(filename, argv);
}
