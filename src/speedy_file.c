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

/* Open/create, mmap and lock the speedy temp file */

#include "speedy.h"

speedy_file_t		*speedy_file_maddr;
static int		file_fd = -1;
static int		maplen;
static int		file_locked;
static char		*file_name;
static struct stat	file_stat;
static int		cur_state;
static time_t		last_reopen;
#ifdef SPEEDY_BACKEND
static int		fd_is_suspect;
#endif

#define fillin_fl(fl)		\
    fl.l_whence	= SEEK_SET;	\
    fl.l_start	= 0;		\
    fl.l_len	= 0

static void file_unmap() {
    if (maplen) {
	(void) munmap((void*)speedy_file_maddr, maplen);
	speedy_file_maddr = 0;
	maplen = 0;
    }
}

static void file_map(unsigned int len) {
    if (maplen != len) {
	file_unmap();
	maplen = len;
	if (len) {
	    speedy_file_maddr = (speedy_file_t*)mmap(
		0, len, PROT_READ | PROT_WRITE, MAP_SHARED, file_fd, 0
	    );
	    if (speedy_file_maddr == (speedy_file_t*)MAP_FAILED)
		speedy_util_die("mmap failed");
	}
    }
}

static void file_unlock() {
    struct flock fl;

    if (!file_locked) return;

    FILE_HEAD.file_corrupt = 0;

    fillin_fl(fl);
    fl.l_type = F_UNLCK;
    if (fcntl(file_fd, F_SETLK, &fl) == -1) speedy_util_die("unlock file");
    file_locked = 0;
}

/* Only call this if you're sure the fd is not suspect */
static void file_close2() {

#ifdef SPEEDY_BACKEND
    if (fd_is_suspect)
	DIE_QUIET("file_close2: assertion failed - fd_is_suspect");
#endif

    file_unlock();
    file_unmap();
    if (file_fd != -1) {
	(void) close(file_fd);
	file_fd = -1;
    }
}


#ifdef SPEEDY_BACKEND
SPEEDY_INLINE void speedy_file_fd_is_suspect() {
    fd_is_suspect = 1;
}

static void fix_suspect_fd() {
    if (fd_is_suspect) {
	if (file_fd != -1) {
	    struct stat stbuf;

	    if (fstat(file_fd, &stbuf) == -1 ||
		stbuf.st_dev != file_stat.st_dev ||
		stbuf.st_ino != file_stat.st_ino)
	    {
		file_unmap();
		file_fd = -1;
	    }
	}
	fd_is_suspect = 0;
    }
}
#endif


#define get_stat() \
    if (fstat(file_fd, &file_stat) == -1) speedy_util_die("fstat")

static void remove_file() {
    FILE_HEAD.file_removed = 1;
    unlink(file_name);
}

static void file_lock() {
    static struct timeval file_create_time;
    struct flock fl;
    int tries = 5;
    time_t now;

    if (file_locked)
	return;

#ifdef SPEEDY_BACKEND
    fix_suspect_fd();
#endif

    /* Re-open the temp file occasionally */
    if ((now = speedy_util_time()) - last_reopen > OPTVAL_RESTATTIMEOUT) {
	last_reopen = now;
	file_close2();
    }

    while (tries--) {
	/* If file is not open, open it */
	if (file_fd == -1) {
	    if (!file_name)
		file_name = speedy_util_fname(FILE_REV, 'F');
	    file_fd = speedy_util_pref_fd(
		open(file_name, O_RDWR | O_CREAT, 0600), PREF_FD_FILE
	    );
	    if (file_fd == -1) speedy_util_die("open temp file");
	}

	/* Lock the file */
	fillin_fl(fl);
	fl.l_type = F_WRLCK;
	if (fcntl(file_fd, F_SETLKW, &fl) == -1) speedy_util_die("lock file");

	/* Fstat the file, now that it's locked down */
	get_stat();

	/* Map into memory */
	file_map(file_stat.st_size);

	/* If file is too small (0 or below MIN_SLOTS_FREE), extend it */
	if (file_stat.st_size < sizeof(file_head_t) ||
	    file_stat.st_size < sizeof(file_head_t) +
		sizeof(slot_t) * (FILE_HEAD.slots_alloced + MIN_SLOTS_FREE))
	{
	    if (ftruncate(file_fd, file_stat.st_size + FILE_ALLOC_CHUNK) == -1)
		speedy_util_die("ftruncate");
	    get_stat();
	    file_map(file_stat.st_size);
	}

	/* Initialize file's create time if necessary */
	if (!FILE_HEAD.create_time.tv_sec)
	    speedy_util_gettimeofday(&(FILE_HEAD.create_time));

	/* Initialize our copy of the create-time if necessary */
	if (!file_create_time.tv_sec || cur_state < FS_HAVESLOTS) {
	    file_create_time = FILE_HEAD.create_time;
	}
	/* Check whether this file is a different version  */
	else if ((file_create_time.tv_sec  != FILE_HEAD.create_time.tv_sec ||
	          file_create_time.tv_usec != FILE_HEAD.create_time.tv_usec))
	{
	    DIE_QUIET("temp file is corrupt");
	}

	/* If file is corrupt (didn't finish all writes), remove it */
	if (FILE_HEAD.file_corrupt)
	    remove_file();

	/* If file has not been removed then all done */
	if (!FILE_HEAD.file_removed)
	    break;

	/* File is invalid */
	if (cur_state >= FS_HAVESLOTS) {
	    /* Too late for this proc - slotnums have changed, can't recover */
	    DIE_QUIET("temp file is invalid");
	} else {
	    /* Bad luck - the file was unlinked after we opened it, but before
	     * we locked it.  Try again.
	     */
	    file_close2();
	}
    }
    if (!tries) {
	DIE_QUIET("could not open temp file");
    }
    file_locked = 1;
}

static void file_close() {
    /* If no groups left, remove the file */
    if (cur_state >= FS_HAVESLOTS) {
	file_lock();
	if (!FILE_HEAD.group_head)
	    remove_file();
    }
    file_close2();
    if (file_name)
	speedy_free(file_name);
    file_name = NULL;
}

int speedy_file_size() {
    return maplen;
}

static void switch_state(int new_state) {
    switch(new_state) {
    case FS_CLOSED:
	file_close();
	break;
    case FS_OPEN:
	file_unlock();
	break;
    case FS_HAVESLOTS:
	file_unlock();
	break;
    case FS_LOCKED:
	if (!file_locked)
	    file_lock();
	FILE_HEAD.file_corrupt = 0;
	break;
    case FS_WRITING:
	if (!file_locked)
	    file_lock();
	FILE_HEAD.file_corrupt = 1;
	break;
    }
}

SPEEDY_INLINE void speedy_file_set_state(int new_state) {
    if (new_state == cur_state)
	return;
    switch_state(new_state);
    cur_state = new_state;
}

#ifdef SPEEDY_BACKEND
void speedy_file_need_reopen() {
    last_reopen = 0;
}
#endif
