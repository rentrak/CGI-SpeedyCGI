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

static struct stat	script_stat;
static int		script_fd;
static time_t		last_open;

void speedy_script_close() {
    if (last_open)
	close(script_fd);
    last_open = 0;
}

int speedy_script_open() {
    time_t now = speedy_util_time();
    const char *fname;

    if (!last_open || now - last_open > OPTVAL_RESTATTIMEOUT) {

	speedy_script_close();

	if (!(fname = speedy_opt_script_argv()[0]))
	    DIE_QUIET("Missing script filename");

	if ((script_fd = open(fname, O_RDONLY)) == -1)
	    speedy_util_die(fname);

	(void) fstat(script_fd, &script_stat);
	last_open = now;
    }
    return script_fd;
}

int speedy_script_changed() {
    struct stat stbuf;

    if (!last_open)
	return 0;
    speedy_memcpy(&stbuf, &script_stat, sizeof(script_stat));
    (void) speedy_script_open();
    return
	stbuf.st_mtime != script_stat.st_mtime ||
	stbuf.st_ino != script_stat.st_ino ||
	stbuf.st_dev != script_stat.st_dev;
}

struct stat *speedy_script_getstat() {
    speedy_script_open();
    return &script_stat;
}

void speedy_script_find(slotnum_t *gslotnum_p, slotnum_t *sslotnum_p) {
    slotnum_t gslotnum, sslotnum;
    gr_slot_t *gslot = NULL;
    scr_slot_t *sslot = NULL;

    /* Find the slot for this script in the file */
    speedy_file_set_state(FS_LOCKED);
    for (gslotnum = FILE_HEAD.group_head, sslotnum = 0; gslotnum;
         gslotnum = gslot->next_slot)
    {
	gslot = &FILE_SLOT(gr_slot, gslotnum);

	if (speedy_group_isvalid(gslotnum)) {

	    for (sslotnum = gslot->script_head; sslotnum;
		 sslotnum = sslot->next_slot)
	    {
		sslot = &FILE_SLOT(scr_slot, sslotnum);
		if (sslot->dev_num == script_stat.st_dev &&
		    sslot->ino_num == script_stat.st_ino)
		{
		    break;
		}
	    }
	    if (sslotnum)
		break;
	}
    }

    /* If mtime has changed, throw away this group */
    if (sslotnum && sslot->mtime != script_stat.st_mtime) {
	slotnum_t bslotnum, next_slot;
	be_slot_t *bslot;

	speedy_file_set_state(FS_WRITING);

	/* Invalidate this group */
	speedy_group_invalidate(gslotnum);

	/* Shutdowns all the BE's in the idle queue */
	for (bslotnum = gslot->be_wait; bslotnum; bslotnum = next_slot) {
	    bslot = &FILE_SLOT(be_slot, bslotnum);
	    next_slot = bslot->be_wait_next;
	    speedy_backend_kill(gslotnum, bslotnum);
	}

	/* Shutdown any dead BE's that are not idle. */
	speedy_backend_check(gslotnum, gslot->be_head);

	/* Try to get rid of the group altogether */
	speedy_group_cleanup(gslotnum);

	gslotnum = sslotnum = 0;
    }

    /* If no slot found for this script, create one */
    if (!sslotnum) {
	speedy_file_set_state(FS_WRITING);
	sslotnum = speedy_slot_alloc();
	sslot = &FILE_SLOT(scr_slot, sslotnum);
	sslot->dev_num = script_stat.st_dev;
	sslot->ino_num = script_stat.st_ino;
	sslot->mtime = script_stat.st_mtime;
	sslot->next_slot = 0;

	/* Need to find our group by name... */
    }

    /* Group not found, create a new one */
    if (!gslotnum) {
	speedy_file_set_state(FS_WRITING);
	gslotnum = speedy_slot_alloc();
	gslot = &FILE_SLOT(gr_slot, gslotnum);
	gslot->be_head = gslot->be_wait = gslot->fe_wait =
	    gslot->fe_tail = gslot->name = 0;
	gslot->script_head = sslotnum;
	gslot->next_slot = FILE_HEAD.group_head;
	FILE_HEAD.group_head = SLOT_CHECK(gslotnum);
    }

    speedy_file_set_state(FS_LOCKED);

    *gslotnum_p = gslotnum;
    *sslotnum_p = sslotnum;
}
