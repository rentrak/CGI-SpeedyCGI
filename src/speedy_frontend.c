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

void speedy_frontend_dispose(slotnum_t gslotnum, slotnum_t fslotnum) {
    if (fslotnum) {
	gr_slot_t *gslot = &FILE_SLOT(gr_slot, gslotnum);
	fe_slot_t *fslot = &FILE_SLOT(fe_slot, fslotnum);
	slotnum_t next = fslot->next_slot;
	slotnum_t prev = fslot->prev_slot;

	/* Update tail if we're the tail */
	if (gslot->fe_tail == fslotnum)
	    gslot->fe_tail = prev;

	if (prev) {
	    FILE_SLOT(fe_slot, prev).next_slot = next;
	} else {
	    gslot->fe_wait = next;
	}
	if (next) {
	    FILE_SLOT(fe_slot, next).prev_slot = prev;
	}
	speedy_slot_free(fslotnum);
    }
}

#ifdef SPEEDY_FRONTEND

#define BE_SUFFIX "_backend"
static int did_spawns;

static void backend_spawn(slotnum_t gslotnum) {
    int pid;
    const char * const *argv;
    slotnum_t bslotnum;

    /* Get args for exec'ing backend */
    argv = speedy_opt_exec_argv();

    /* Create a backend slot for this pid so we have the slot in the
     * queue right away and can track this backend - no guarantee on when
     * the child proc will do this otherwise.
     */
    bslotnum = speedy_backend_create_slot(gslotnum);

    /* Fork */
    pid = fork();

    if (pid) {
	/* Parent */

	int child_status;

	if (pid == -1 || waitpid(pid, &child_status, 0) != pid ||
	    !WIFEXITED(child_status) || WEXITSTATUS(child_status) != 0)
	{
	    speedy_util_die("fork failed");
	}
    } else {
	/* Child */

	/* Fork */
	pid = fork();

	if (pid == -1) {
	    speedy_util_exit(1,1);
	}
	else if (pid) {
	    /* Parent of Grandchild */

	    /* We don't hold the lock on the temp file, but our parent does,
	     * and it's waiting for us to exit before proceeding, so it's
	     * safe to write to the file here
	     */
	    FILE_SLOT(be_slot, bslotnum).pid = pid;

	    speedy_util_exit(0,1);
	}
	else {
	    /* Grandchild */

	    /* We should be in our own session */
	    setsid();

	    /* Exec the backend */
	    speedy_util_execvp(argv[0], argv);

	    /* Failed.  Try the original argv[0] + "_backend" */
	    {
		const char *orig_file = speedy_opt_orig_argv()[0];
		if (orig_file && *orig_file) {
		    char *fname;
		    
		    speedy_new(
			fname, strlen(orig_file)+sizeof(BE_SUFFIX)+1, char
		    );
		    sprintf(fname, "%s%s", orig_file, BE_SUFFIX);
		    speedy_util_execvp(fname, argv);
		}
	    }
	    speedy_util_die(argv[0]);
	}
    }
}

/* Count the number of backends total and the number spawning */
/* Maybe need summary variables for these counts in the group struct? */
static void count_bes(gr_slot_t *gslot, int *num, int *spawning) {
    slotnum_t next_slot, bslotnum;
    *num = *spawning = 0;

    for (bslotnum = gslot->be_head; bslotnum; bslotnum = next_slot) {
	be_slot_t *bslot = &FILE_SLOT(be_slot, bslotnum);
	next_slot = bslot->next_slot;
	++(*num);
	if (bslot->maturity == 0)
	    ++(*spawning);
    }
}

/* Count the number of fe's in the queue.  Stop when we get to a max */
/* Maybe need a summary variable for this count in the group struct? */
static int count_fes(gr_slot_t *gslot, int max) {
    slotnum_t next_slot, fslotnum;
    int retval = 0;

    for (fslotnum = gslot->fe_wait; fslotnum; fslotnum = next_slot) {
	fe_slot_t *fslot = &FILE_SLOT(fe_slot, fslotnum);
	next_slot = fslot->next_slot;
	if (++retval >= max)
	    break;
    }
    return retval;
}

/* Check on / spawn backends.  Should only be done by the fe at the
 * head of the list (think 100+ fe's in the queue)
 */
static int backend_check(slotnum_t gslotnum, int in_queue) {
    gr_slot_t *gslot = &FILE_SLOT(gr_slot, gslotnum);
    int be_count, be_spawning, fe_count;

    /* Check for dead backend processes.  If this group contains no
     * fe's or be's it may be removed here, so don't call this function
     * unless the group is populated.
     */
    speedy_backend_check(gslotnum, gslot->be_head);

    /* Count the backends */
    count_bes(gslot, &be_count, &be_spawning);
	
    /* If we did spawns in the past, but no be's now, and we are still in
     * the queue, then backend spawn must be broken.
     */
    if (!be_count && did_spawns && in_queue)
	return 0;

    /* Count the number of frontends waiting, up to a max */
    fe_count = count_fes(gslot, OPTVAL_BESPAWNS);

    /* Start spawning */
    while (fe_count > be_spawning &&
	(OPTVAL_MAXBACKENDS == 0 || be_count < OPTVAL_MAXBACKENDS))
    {
	backend_spawn(gslotnum);
	++be_spawning;
	++be_count;
	did_spawns = 1;
    }
    return 1;
}

/* Go up the fe_wait list, going to the next group if we're at the
 * begininng of the list.  Wrap to the first group if we go off the end
 * of the group list.  Worst case we wrap around and return ourself.
 */
static void fe_prev_slot(slotnum_t *gslotnum, slotnum_t *fslotnum) {
    *fslotnum = FILE_SLOT(fe_slot, *fslotnum).prev_slot;
    while (!*fslotnum) {
	if (!(*gslotnum = FILE_SLOT(gr_slot, *gslotnum).next_slot) &&
	    !(*gslotnum = FILE_HEAD.group_head))
	{
	    DIE_QUIET("Group list or frontend lists are corrupt");
	}
	*fslotnum = FILE_SLOT(gr_slot, *gslotnum).fe_tail;
    }
}

static void frontend_check_prev(slotnum_t gslotnum, slotnum_t fslotnum) {
    fe_prev_slot(&gslotnum, &fslotnum);

    while (speedy_util_kill(FILE_SLOT(fe_slot, fslotnum).pid, 0) == -1) {
	slotnum_t g_prev = gslotnum, f_prev = fslotnum;

	/* Must do "prev" function while this slot/group is still valid */
	fe_prev_slot(&g_prev, &f_prev);

	/* This frontend is not running so dispose of it */
	speedy_frontend_dispose(gslotnum, fslotnum);

	/* Try to remove this group if possible */
	speedy_group_cleanup(gslotnum);

	/* If we wrapped around to ourself, then all done */
	if (f_prev == fslotnum)
	    break;

	gslotnum = g_prev;
	fslotnum = f_prev;
    }
}

/* Check that the frontend in front of is running, or run backend check
 * if we're at the beginning
 */
static void frontend_ping(slotnum_t gslotnum, slotnum_t fslotnum) {

    /* Check the frontend previous to us.  This may remove it */
    frontend_check_prev(gslotnum, fslotnum);

    /* If we're not at the beginning of the list, then all done */
    if (FILE_SLOT(fe_slot, fslotnum).prev_slot)
	return;

    /* Do a check of backends */
    if (!backend_check(gslotnum, 1)) {
	speedy_frontend_dispose(gslotnum, fslotnum);
	speedy_group_cleanup(gslotnum);
	speedy_file_set_state(FS_CLOSED);
	DIE_QUIET("Cannot spawn backend process");
    }
}

/*
 * Signal handling routines
 */

#define NUMSIGS (sizeof(signum) / sizeof(int))

static const int	signum[] = {SIGALRM};
static struct sigaction	sigact_save[NUMSIGS];
static char		sig_setup_done;
static volatile char	got_sig;
static time_t		next_alarm;
static sigset_t		unblock_sigs, sigset_save;


static void sig_handler_teardown() {
    int i;

    if (!sig_setup_done)
	return;
    
    alarm(0);

    /* Put signal mask back to orig - we may get some pending signals
     * delivered when we do this, so do it before removing handlers
     */
    sigprocmask(SIG_SETMASK, &sigset_save, NULL);

    /* Install old handlers */
    for (i = 0; i < NUMSIGS; ++i) {
	sigaction(signum[i], &(sigact_save[i]), NULL);
    }

    /* Put back alarm */
    if (next_alarm) {
	next_alarm -= speedy_util_time();
	alarm(next_alarm > 0 ? next_alarm : 1);
    }

    sig_setup_done = 0;
}

static void sig_handler(int sig) {
    got_sig = 1;
}

static void sig_handler_setup() {
    struct sigaction sigact;
    sigset_t block_sigs;
    int i;

    sig_handler_teardown();

    /* Save alarm for later */
    if ((next_alarm = alarm(0))) {
	next_alarm += speedy_util_time();
    }

    /* Set up handlers and save old action setting */
    for (i = 0; i < NUMSIGS; ++i) {
	sigact.sa_handler = &sig_handler;
	sigact.sa_flags = SA_RESTART;
	sigemptyset(&sigact.sa_mask);
	sigaction(signum[i],  &sigact, &(sigact_save[i]));
    }

    /* Block our signals.  Save original mask */
    sigemptyset(&block_sigs);
    for (i = 0; i < NUMSIGS; ++i) {
	sigaddset(&block_sigs, signum[i]);
    }
    sigprocmask(SIG_BLOCK, &block_sigs, &sigset_save);

    /* Make an unblock mask for our signals */
    unblock_sigs = sigset_save;
    for (i = 0; i < NUMSIGS; ++i) {
	sigdelset(&unblock_sigs, signum[i]);
    }

    sig_setup_done = 1;
}

#define sig_wait() \
	for (got_sig = 0; !got_sig;) sigsuspend(&unblock_sigs)

/*
 * End of Signal handling routines
 */

/* Get a backend the hard-way - by queueing up
*/
static slotnum_t get_a_backend_hard(slotnum_t gslotnum, slotnum_t sslotnum) {
    slotnum_t fslotnum, retval = 0, tail;
    gr_slot_t *gslot = &FILE_SLOT(gr_slot, gslotnum);
    fe_slot_t *fslot;
    int file_changed;

    /* Install sig handlers */
    sig_handler_setup();

    /* Allocate a frontend slot */
    fslotnum = speedy_slot_alloc();
    fslot = &FILE_SLOT(fe_slot, fslotnum);
    fslot->pid = speedy_util_getpid();

    /* Add ourself to the end of the fe queue */
    if ((tail = gslot->fe_tail))
	FILE_SLOT(fe_slot, tail).next_slot = fslotnum;
    fslot->prev_slot = tail;
    fslot->next_slot = 0;
    gslot->fe_tail = fslotnum;
    if (!gslot->fe_wait)
	gslot->fe_wait = fslotnum;
    did_spawns = 0;

    while (1) {
	/* Send signals */
	speedy_group_sendsigs(gslotnum);

	/* If our sent_sig flag is set, and there are be's for us to use 
	 * then all done.
	*/
	if (fslot->sent_sig && (retval = speedy_backend_be_wait_get(gslotnum)))
	    break;

	/* Check on frontends/backends running */
	frontend_ping(gslotnum, fslotnum);

	/* Unlock the file */
	speedy_file_set_state(FS_HAVESLOTS);

	/* Set an alarm for one-second. */
	alarm(OPTVAL_BECHECKTIMEOUT);

	/* Wait for a signal */
	sig_wait();
	speedy_util_time_invalidate();

	/* Find out if our file changed.  Do this while unlocked */
	file_changed = speedy_script_changed();

	/* Map in & lock down temp file */
	speedy_file_set_state(FS_WRITING);

	/* File may have been mmap'ed elsewhere so get fresh pointers */
	fslot = &FILE_SLOT(fe_slot, fslotnum);
	gslot = &FILE_SLOT(gr_slot, gslotnum);

	/* If file changed or our group slot is invalid, break out of loop */
	if (file_changed || !speedy_group_isvalid(gslotnum)) {
	    break;
	}
    }

    {
	int at_front = !fslot->prev_slot;

	/* Remove our FE slot from the queue.  */
	speedy_frontend_dispose(gslotnum, fslotnum);

#ifdef ALTERNATE_SPAWNING
	if (retval && at_front && gslot->fe_wait) {
	    int be_count, be_spawning;

	    count_bes(gslot, &be_count, &be_spawning);
	    if (!be_spawning) {
		speedy_util_kill(
		    FILE_SLOT(fe_slot, gslot->fe_wait).pid, SIGALRM
		);
	    }
	}
#else
	/* If we were at the beginning of the list and there are other fe's
	 * waiting, we should assist them by starting more backends.
	 */
	if (retval && at_front && FILE_SLOT(gr_slot, gslotnum).fe_wait)
	    (void) backend_check(gslotnum, 0);
#endif
    }

    /* Put sighandlers back to their original state */
    sig_handler_teardown();

    return retval;
}

static slotnum_t get_a_backend(pid_t *pid) {
    slotnum_t sslotnum, gslotnum, bslotnum = 0;

    /* Map in & lock down temp file, intially only for reading */
    speedy_file_set_state(FS_LOCKED);

    /* Locate the group and script slot */
    speedy_script_find(&gslotnum, &sslotnum);

    /* Might start writing to the temp file now */
    speedy_file_set_state(FS_WRITING);

    /* Try to quickly grab a backend without queueing */
    if (!FILE_SLOT(gr_slot, gslotnum).fe_wait)
	bslotnum = speedy_backend_be_wait_get(gslotnum);

    /* If that failed, use the queue */
    if (!bslotnum)
	bslotnum = get_a_backend_hard(gslotnum, sslotnum);

    if (bslotnum)
	*pid = FILE_SLOT(be_slot, bslotnum).pid;
    
    /* Clean up the group if necessary */
    speedy_group_cleanup(gslotnum);

    /* Unlock the file */
    speedy_file_set_state(FS_OPEN);

    return bslotnum;
}


void speedy_frontend_connect(int *s, int *e) {

    /* Create sockets in preparation for connect.  This may take a while */
    speedy_ipc_connect_prepare(s, e);

    /* May need options from the #! line in the script */
    speedy_opt_read_shbang();

    while (1) {
	slotnum_t bslotnum;
	pid_t pid;

	/* Find a backend */
	if ((bslotnum = get_a_backend(&pid))) {
	    
	    /* Try to talk to this backend.  If successful, return */
	    if (speedy_ipc_connect(bslotnum, *s, *e))
		break;

	    /* Backend is not responding.  Kill it to make sure it's gone */
	    speedy_util_kill(pid, SIGKILL);

	    /* Connect failed, so we'll need new sockets */
	    speedy_ipc_connect_prepare(s, e);
	}
    }
    speedy_script_close();
}

typedef struct {
    char *buf;
    int	 alloced;
    int  len;
} SpeedyBuf;

/* Return size of the buffer needed to send a string of the given length */
#define STR_BUFSIZE(l) (1 + (l >= MAX_SHORT_STR ? sizeof(int) : 0) + l)

/* Add something to the buffer */
#define BUF_ENLARGE(b,l) \
    if ((b)->len + (l) > (b)->alloced) \
	enlarge_buf((b),(l))

#define BUF_REALLOC(b,sz) \
    do { \
	int newsz = (sz); \
	(b)->alloced = newsz; \
	speedy_renew((b)->buf, newsz, char); \
    } while (0)

#define BUF_SHRINK(b) BUF_REALLOC(b, (b)->len)

#define ADD2(b,s,l) \
    speedy_memcpy((b)->buf + (b)->len, (s), (l)); \
    (b)->len += (l)

#define ADD(b,s,l) BUF_ENLARGE(b,l); ADD2(b,s,l)

#define ADDCHAR2(b,c) ((b)->buf)[(b)->len++] = (char)c
    
#define ADDCHAR(b,c) BUF_ENLARGE(b,1); ADDCHAR2(b,c)

#define ADD_DEVINO(b,stbuf) \
    do { \
	SpeedyDevIno devino = speedy_util_stat_devino(stbuf); \
	ADD((b), &devino, sizeof(SpeedyDevIno)); \
    } while (0)

#define ADD_STRING(b, s, l) \
    do { \
	if ((l) >= MAX_SHORT_STR) { \
	    BUF_ENLARGE(b, (sizeof(int)+1)); \
	    ADDCHAR2(b, MAX_SHORT_STR); \
	    ADD2(b, &(l), sizeof(int)); \
	} else { \
	    ADDCHAR(b, l); \
	} \
	ADD(b, s, l); \
    } while (0)

static void enlarge_buf(SpeedyBuf *b, int min_to_add) {
    int new_size = b->alloced * 2;
    new_size = max(new_size, b->len + min_to_add);
    BUF_REALLOC(b, new_size);
}

static void alloc_buf(SpeedyBuf *b, int bytes) {
    b->alloced = bytes;
    speedy_new(b->buf, bytes, char);
    b->len = 0;
}

/* Add a string to the buffer */
static void add_string(SpeedyBuf *b, const char *s, int l) {
    ADD_STRING(b, s, l);
}

/* Copy a block of strings into the buffer,  */
/* Profiling shows this is the top function for cpu time */
static void add_strings(register SpeedyBuf *b, register const char * const * p)
{
    int l;
    register const char *s;

    /* Add strings in p array */
    for (; (s = *p); ++p) {
	if ((l = strlen(s))) {
	    ADD_STRING(b, s, l);
	}
    }

    /* Terminate with zero-length string */
    ADDCHAR(b, 0);
}

char *speedy_frontend_mkenv(
    const char * const * envp, const char * const * scr_argv, int min_alloc,
    int min_free, int *env_size, int *buf_size, int script_has_cwd
)
{
    SpeedyBuf b;
    struct stat dir_stat;
    const char *script_fname = speedy_opt_script_fname();

    if (!script_fname)
	speedy_script_missing();

    /* Create buffer */
    alloc_buf(&b, min_alloc);

    /* Add env and argv */
    add_strings(&b, envp);
    add_strings(&b, scr_argv+1);

    /* Put script filename into buffer */
    add_string(&b, script_fname, strlen(script_fname));

    /* Put script device/inode into buffer */
    ADD_DEVINO(&b, speedy_script_getstat());

    /* Handle passing over cwd */
    if (script_has_cwd) {
	ADDCHAR(&b, SPEEDY_CWD_IN_SCRIPT);
    }
    else if (stat(".", &dir_stat) != -1) {
	ADDCHAR(&b, SPEEDY_CWD_DEVINO);
	ADD_DEVINO(&b, &dir_stat);
    } else {
	ADDCHAR(&b, SPEEDY_CWD_UNKNOWN);
    }

    /* Save size of the buffer with just the environment */
    *env_size = b.len;

    /* Must leave a minimum amount of free space in the buffer */
    if (b.alloced - b.len < min_free) {
	int needed = min_free - (b.alloced - b.len);
	enlarge_buf(&b, needed);
	b.len += needed;
    }

    BUF_SHRINK(&b);
    *buf_size = b.alloced;

    return b.buf;
}

void speedy_frontend_proto2(int err_sock, int first_byte) {
    int n, cwd_len, buflen;
    char *bp, *cwd;
    PollInfo pi;
    SpeedyBuf b;

    if (!first_byte)
	return;

    /* Get current directory */
    cwd = speedy_util_getcwd();
    cwd_len = cwd ? strlen(cwd) : 0;

    /* Create buffer for the string */
    alloc_buf(&b, STR_BUFSIZE(cwd_len));

    /* Put cwd into the buffer */
    if (cwd) {
	add_string(&b, cwd, cwd_len);
	speedy_free(cwd);
    } else {
	add_string(&b, "", 0);
    }

    /* Send it over */
    speedy_poll_init(&pi, err_sock);
    bp = b.buf;
    buflen = b.len;
    while (1) {

	/* TEST - send over one byte at a time to test the poll */
	/* n = write(err_sock, bp, 1); */

	n = write(err_sock, bp, buflen);
	if (n == -1 && SP_NOTREADY(errno))
	    n = 0;
	if (n == -1)
	    break;

	if (!(buflen -= n))
	    break;
	bp += n;

	/* Do this instead of bothering to change socket to non-blocking */
	speedy_poll_quickwait(&pi, err_sock, SPEEDY_POLLOUT, 1000);
    }
    speedy_poll_free(&pi);
    speedy_free(b.buf);

    shutdown(err_sock, 1);
}

#endif /* SPEEDY_FRONTEND */
