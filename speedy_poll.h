
#if defined(USE_POLL) || ((defined(sun) || defined(SYSV)) && !defined(USE_SELECT))
#   define USE_POLL
#   undef  USE_SELECT
#else
#   define USE_SELECT
#   undef  USE_POLL
#endif

#ifdef USE_POLL

/*******************
 * Poll Section
 *******************/

#include <sys/poll.h>

#define SPEEDY_POLLIN	(POLLIN  | POLLHUP | POLLERR | POLLNVAL)
#define SPEEDY_POLLOUT	(POLLOUT | POLLHUP | POLLERR | POLLNVAL)

typedef struct {
    struct pollfd	*fds, **fdmap;
    int			maxfd, numfds;
} PollInfo;

#else

/*******************
 * Select Section
 *******************/

#define SPEEDY_POLLIN	1
#define SPEEDY_POLLOUT	2

typedef struct {
    int		maxfd;
    fd_set	fdset[2];
} PollInfo;

#endif

/*******************
 * Common to Both
 *******************/

void speedy_poll_init(PollInfo *pi, int maxfd);
void speedy_poll_free(PollInfo *pi);
void speedy_poll_reset(PollInfo *pi);
void speedy_poll_set(PollInfo *pi, int fd, int flags);
int speedy_poll_wait(PollInfo *pi, int msecs);
int speedy_poll_isset(PollInfo *pi, int fd, int flag);
