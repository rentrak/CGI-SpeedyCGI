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

static int listener;
static struct stat listener_stbuf;
static PollInfo listener_pi;

static char *get_fname(pid_t pid, int do_unlink) {
    char *fname;
    fname = (char*) speedy_malloc(strlen(OPTVAL_TMPBASE) + 64);
    sprintf(fname, "%s.%x.%x.S",
	OPTVAL_TMPBASE, (int)pid, speedy_util_geteuid()
    );
    if (do_unlink)
	unlink(fname);
    return fname;
}

static int setup_sock(
    pid_t pid, struct sockaddr_un *sa, int pref_fd, int do_unlink
)
{
    char *fname = get_fname(pid, do_unlink);
    speedy_bzero(sa, sizeof(*sa));
    sa->sun_family = AF_UNIX;
    if (strlen(fname)+1 > sizeof(sa->sun_path)) {
	DIE_QUIET("Socket path %s is too long", fname);
    }
    strcpy(sa->sun_path, fname);
    speedy_free(fname);
    return speedy_util_pref_fd(socket(AF_UNIX, SOCK_STREAM, 0), pref_fd);
}

static int do_connect(pid_t pid, int pref_fd) {
    struct sockaddr_un sa;
    int talker;

    talker = setup_sock(pid, &sa, pref_fd, 0);
    if (talker != -1) {
	if (connect(talker, (struct sockaddr *)&sa, sizeof(sa)) != -1) {
	    return talker;
	}
	close(talker);
    }
    return -1;
}

void speedy_ipc_listen() {
    struct sockaddr_un sa;

    listener = -1;
    if (PREF_FD_LISTENER != -1) {
	int namelen = sizeof(sa);
	char *fname = get_fname(speedy_util_getpid(), 0);
	struct stat stbuf;

	if (getsockname(PREF_FD_LISTENER, (struct sockaddr *)&sa, &namelen) != -1 &&
	    sa.sun_family == AF_UNIX &&
	    strcmp(sa.sun_path, fname) == 0 &&
	    stat(fname, &stbuf) != -1 &&
	    stbuf.st_uid == speedy_util_geteuid())
	{
	    listener = PREF_FD_LISTENER;
	}
	speedy_free(fname);
    }
    if (listener == -1) {
	mode_t saved_umask = umask(077);
	listener = setup_sock(speedy_util_getpid(), &sa, PREF_FD_LISTENER, 1);
	if (listener == -1)
	    speedy_util_die("cannot create socket");
	if (bind(listener, (struct sockaddr*)&sa, sizeof(sa)) == -1)
	    speedy_util_die("cannot bind socket");
	umask(saved_umask);
    }
    if (listen(listener, LISTEN_BACKLOG) == -1)
	speedy_util_die("cannot listen on socket");
    fstat(listener, &listener_stbuf);
    speedy_poll_init(&listener_pi, listener);
}

void speedy_ipc_listen_fixfd() {
    struct stat stbuf;
    int status, test1, test2;

    /* Odd compiler bug - Solaris 2.7 plus gcc 2.95.2, can't put all of
     * this into one big "if" statment - returns false constantly.  Probably
     * has something to do with 64-bit values in st_dev/st_ino
     */
    status = fstat(listener, &stbuf);
    test1 = stbuf.st_dev != listener_stbuf.st_dev;
    test2 = stbuf.st_ino != listener_stbuf.st_ino;
    if (status == -1 || test1 || test2) {
	close(listener);
	speedy_ipc_listen();
    }
}

void speedy_ipc_cleanup(pid_t pid) {
    speedy_free(get_fname(pid, 1));
}

void speedy_ipc_unlisten() {
    (void) close(listener);
    speedy_poll_free(&listener_pi);
}

int speedy_ipc_connect(pid_t pid, int *s, int *e) {

    *s = do_connect(pid, PREF_FD_CONNECT_S);
    if (*s == -1) return 0;

    *e = do_connect(pid, PREF_FD_CONNECT_E);
    if (*e == -1) {
	close(*s);
	return 0;
    }
    return 1;
}

static int do_accept(int pref_fd) {
    struct sockaddr_un sa;
    int namelen, sock;

    namelen = sizeof(sa);
    sock = speedy_util_pref_fd(
	accept(listener, (struct sockaddr*)&sa, &namelen), pref_fd
    );
    if (sock == -1) speedy_util_die("accept failed");
    return sock;
}

int speedy_ipc_accept_ready(int wakeup) {
    speedy_poll_reset(&listener_pi);
    speedy_poll_set(&listener_pi, listener, SPEEDY_POLLIN);
    return speedy_poll_wait(&listener_pi, wakeup) > 0;
}

int speedy_ipc_accept(int wakeup, int *s, int *e) {
    if (speedy_ipc_accept_ready(wakeup)) {
	*s = do_accept(PREF_FD_ACCEPT_S);
	*e = do_accept(PREF_FD_ACCEPT_E);
	return 1;
    }
    return 0;
}
