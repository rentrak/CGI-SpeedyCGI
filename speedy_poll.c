
#include "speedy.h"

#ifdef USE_POLL

/*
 * Poll Section
 */

void speedy_poll_init(PollInfo *pi, int maxfd) {
    pi->maxfd	= maxfd;
    New(123, pi->fds,   maxfd+1, struct pollfd);
    New(123, pi->fdmap, maxfd+1, struct pollfd *);
    speedy_poll_reset(pi);
}

void speedy_poll_free(PollInfo *pi) {
    Safefree(pi->fds);
}

void speedy_poll_reset(PollInfo *pi) {
    pi->numfds = 0;
    Zero(pi->fdmap, pi->maxfd + 1, struct pollfd *);
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
}

int speedy_poll_wait(PollInfo *pi, int msecs) {
    return poll(pi->fds, pi->numfds, msecs);
}

int speedy_poll_isset(PollInfo *pi, int fd, int flag) {
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
	tv.tv_usec = (msecs - tv.tv_sec) * 1000;
	tvp = &tv;
    }
    return select(pi->maxfd+1, pi->fdset + 0, pi->fdset + 1, NULL, tvp);
}

int speedy_poll_isset(PollInfo *pi, int fd, int flag) {
    int i;
    for (i = 0; i < 2; ++i) {
	if (flag & (1<<i)) {
	    return FD_ISSET(fd, pi->fdset + i);
	}
    }
    return 0;
}

#endif
