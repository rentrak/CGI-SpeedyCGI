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

#ifdef USE_POLL

/*
 * Poll Section
 */

void speedy_poll_init(PollInfo *pi, int maxfd) {
    pi->maxfd	= maxfd;
    pi->fds   = speedy_malloc((maxfd+1) * sizeof(struct pollfd));
    pi->fdmap = speedy_malloc((maxfd+1) * sizeof(struct pollfd *));
    speedy_poll_reset(pi);
}

void speedy_poll_free(PollInfo *pi) {
    speedy_free(pi->fds);
    speedy_free(pi->fdmap);
}

void speedy_poll_reset(PollInfo *pi) {
    pi->numfds = 0;
    speedy_bzero(pi->fdmap, (pi->maxfd + 1) * sizeof(struct pollfd *));
}

void speedy_poll_set(PollInfo *pi, int fd, int flags) {
    struct pollfd **fdm = pi->fdmap;
    struct pollfd *pfd = fdm[fd];

    if (!pfd) {
	/* Allocate new */
	pfd = fdm[fd] = pi->fds + pi->numfds++;
	pfd->fd = fd;
	pfd->events = pfd->revents = 0;
    }
    pfd->events |= flags;
    pfd->revents |= flags;
}

int speedy_poll_wait(PollInfo *pi, int msecs) {
    return poll(pi->fds, pi->numfds, msecs);
}

int speedy_poll_isset(const PollInfo *pi, int fd, int flag) {
    struct pollfd *pfd = (pi->fdmap)[fd];
    return pfd ? ((pfd->revents & flag) != 0) : 0;
}

#else

/*
 * Select Section
 */

void speedy_poll_init(PollInfo *pi, int maxfd) {
    pi->maxfd = maxfd;
    speedy_poll_reset(pi);
}

void speedy_poll_free(PollInfo *pi) { }

void speedy_poll_reset(PollInfo *pi) {
    FD_ZERO(pi->fdset + 0);
    FD_ZERO(pi->fdset + 1);
}

void speedy_poll_set(PollInfo *pi, int fd, int flags) {
    int i;
    for (i = 0; i < 2; ++i) {
	if (flags & (1<<i)) {
	    FD_SET(fd, pi->fdset + i);
	}
    }
}

int speedy_poll_wait(PollInfo *pi, int msecs) {
    struct timeval tv, *tvp;
    if (msecs == -1) {
	tvp = NULL;
    } else {
	tv.tv_sec  = msecs / 1000;
	tv.tv_usec = (msecs % 1000) * 1000;
	tvp = &tv;
    }
    return select(pi->maxfd+1, pi->fdset + 0, pi->fdset + 1, NULL, tvp);
}

int speedy_poll_isset(const PollInfo *pi, int fd, int flag) {
    int i;
    for (i = 0; i < 2; ++i) {
	if (flag & (1<<i)) {
	    return FD_ISSET(fd, pi->fdset + i);
	}
    }
    return 0;
}

#endif
