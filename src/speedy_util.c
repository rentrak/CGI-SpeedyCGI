/*
 * Copyright (C) 2001  Daemon Consulting Inc.
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
static int my_euid = -1;

int speedy_util_pref_fd(int oldfd, int newfd) {
    if (newfd == oldfd || newfd == PREF_FD_DONTCARE || oldfd == -1)
	return oldfd;
    (void) dup2(oldfd, newfd);
    (void) close(oldfd);
    return newfd;
}

#ifdef SPEEDY_PROFILING
static void end_profiling(int dowrites) {
    char *cwd;
    
    if (dowrites)
	cwd = getcwd(NULL, 0);

    mkdir(SPEEDY_PROFILING, 0777);
    chdir(SPEEDY_PROFILING);

    if (dowrites) {
	_mcleanup();
	__bb_exit_func();
	chdir(cwd);
	free(cwd);
    }
}
#endif

SPEEDY_INLINE int speedy_util_geteuid() {
    if (my_euid == -1)
	my_euid = geteuid();
    return my_euid;
}

#ifdef IAMSUID
int speedy_util_seteuid(int id) {
    int retval = seteuid(id);
    if (retval != -1)
	my_euid = id;
    return retval;
}
#endif

SPEEDY_INLINE int speedy_util_getuid() {
    static int uid = -1;
    if (uid == -1)
	uid = getuid();
    return uid;
}

#ifdef SPEEDY_BACKEND
int speedy_util_argc(const char * const * argv) {
    int retval;
    for (retval = 0; *argv++; ++retval) 
	;
    return retval;
}
#endif

SPEEDY_INLINE int speedy_util_getpid() {
    static int saved_pid;
    if (!saved_pid) saved_pid = getpid();
    return saved_pid;
}

static void just_die(const char *fmt, va_list ap) {
    extern int errno;
    char buf[2048];

    sprintf(buf, "%s[%u]: ", SPEEDY_PROGNAME, (int)getpid());
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

#ifdef SPEEDY_PROFILING
    end_profiling(1);
#endif

    /* Exec the backend */
    return speedy_execvp(filename, argv);
}

char *speedy_util_strndup(const char *s, int len) {
    char *buf;
    speedy_new(buf, len+1, char);
    speedy_memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

SPEEDY_INLINE void speedy_util_gettimeofday(struct timeval *tv) {
    if (!saved_time.tv_sec)
	gettimeofday(&saved_time, NULL);
    *tv = saved_time;
}

SPEEDY_INLINE int speedy_util_time() {
    struct timeval tv;
    speedy_util_gettimeofday(&tv);
    return tv.tv_sec;
}

SPEEDY_INLINE void speedy_util_time_invalidate() {
    saved_time.tv_sec = 0;
}

char *speedy_util_fname(int num, char type) {
    char *fname;
    int uid = speedy_util_getuid(), my_euid = speedy_util_geteuid();

    speedy_new(fname, strlen(OPTVAL_TMPBASE) + 80, char);

    if (my_euid == uid)
	sprintf(fname, "%s.%x.%x.%c", OPTVAL_TMPBASE, num, my_euid, type);
    else
	sprintf(fname, "%s.%x.%x.%x.%c", OPTVAL_TMPBASE, num, my_euid, uid, type);

    return fname;
}

char *speedy_util_getcwd() {
    char *buf, *cwd_ret;
    int size = 512, too_small;

    /* TEST - see if memory alloc works */
    /* size = 10; */

    while (1) {
	speedy_new(buf, size, char);
	cwd_ret = getcwd(buf, size);

	/* TEST - simulate getcwd failure due to unreable directory */
	/* cwd_ret = NULL; errno = EACCES; */

	if (cwd_ret != NULL)
	    break;

	/* Must test errno here in case speedy_free overwrites it */
	too_small = (errno == ERANGE);

	speedy_free(buf);

	if (!too_small)
	    break;

	size *= 2;
    }
    return cwd_ret;
}

void speedy_util_mapout(SpeedyMapInfo *mi) {
    if (mi->addr) {
	if (mi->is_mmaped)
	    (void) munmap(mi->addr, mi->maplen);
	else
	    speedy_free(mi->addr);
	mi->addr = NULL;
    }
    speedy_free(mi);
}

static int readall(int fd, void *addr, int len) {
    int numread, n;

    for (numread = 0; len - numread; numread += n) {
	n = read(fd, ((char*)addr) + numread, len - numread);
	if (n == -1)
	    return -1;
	if (n == 0)
	    break;
    }
    return numread;
}

SpeedyMapInfo *speedy_util_mapin(int fd, int max_size, int file_size)
{
    SpeedyMapInfo *mi;
    
    speedy_new(mi, 1, SpeedyMapInfo);

    mi->maplen = max_size == -1 ? file_size : min(file_size, max_size);
    mi->addr = mmap(0, mi->maplen, PROT_READ, MAP_SHARED, fd, 0);
    mi->is_mmaped = (mi->addr != (void*)MAP_FAILED);

    if (!mi->is_mmaped) {
	speedy_new(mi->addr, mi->maplen, char);
	lseek(fd, 0, SEEK_SET);
	mi->maplen = readall(fd, mi->addr, mi->maplen);
	if (mi->maplen == -1) {
	    speedy_util_mapout(mi);
	    return NULL;
	}
    }
    return mi;
}

SPEEDY_INLINE SpeedyDevIno speedy_util_stat_devino(const struct stat *stbuf) {
    SpeedyDevIno retval;
    retval.d = stbuf->st_dev;
    retval.i = stbuf->st_ino;
    return retval;
}

SPEEDY_INLINE int speedy_util_open_stat(const char *path, struct stat *stbuf)
{
    int fd = open(path, O_RDONLY);
    if (fd != -1 && fstat(fd, stbuf) == -1) {
       close(fd);
       fd = -1;
    }
    return fd;
}

void speedy_util_exit(int status, int underbar_exit) {

#ifdef SPEEDY_PROFILING
    end_profiling(underbar_exit);
#endif

    if (underbar_exit)
	_exit(status);
    else
	exit(status);
}
