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

static struct stat	script_stat;
static int		script_fd;
static time_t		last_open;

void speedy_script_close() {
    if (last_open)
	close(script_fd);
    last_open = 0;
}

void speedy_script_missing() {
    DIE_QUIET("Missing script filename.  "
	"Type \"perldoc CGI::SpeedyCGI\" for SpeedyCGI documentation.");
}

int speedy_script_open_failure() {
    time_t now = speedy_util_time();
    const char *fname;

    if (!last_open || now - last_open > OPTVAL_RESTATTIMEOUT) {

	speedy_script_close();

	if (!(fname = speedy_opt_script_fname()))
	    return 1;

	if ((script_fd = speedy_util_open_stat(fname, &script_stat)) == -1)
	    return 2;

	last_open = now;
    }
    return 0;
}

int speedy_script_open() {
    switch (speedy_script_open_failure()) {
	case 1:
	    speedy_script_missing();
	    break;
	case 2:
	    speedy_util_die(speedy_opt_script_fname());
	    break;
    }
    return script_fd;
}

#ifdef SPEEDY_FRONTEND
int speedy_script_changed() {
    struct stat stbuf;

    if (!last_open)
	return 0;
    stbuf = script_stat;
    (void) speedy_script_open();
    return
	stbuf.st_mtime != script_stat.st_mtime ||
	stbuf.st_ino != script_stat.st_ino ||
	stbuf.st_dev != script_stat.st_dev;
}
#endif

const struct stat *speedy_script_getstat() {
    speedy_script_open();
    return &script_stat;
}

void speedy_script_find(slotnum_t *gslotnum_p, slotnum_t *sslotnum_p) {
    slotnum_t gslotnum, sslotnum;
    gr_slot_t *gslot = NULL;
    scr_slot_t *sslot = NULL;
    
    (void) speedy_script_getstat();

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
	speedy_file_set_state(FS_WRITING);

	/* Invalidate this group */
	speedy_group_invalidate(gslotnum);

	/* Shutdowns all the BE's in the idle queue */
	speedy_backend_kill_idle(gslotnum);

	if (FILE_SLOT(gr_slot, gslotnum).be_head) {
	    /* Shutdown any dead BE's.  May remove the group */
	    speedy_backend_check(gslotnum, gslot->be_head);
	} else {
	    /* Try to remove the group directly */
	    speedy_group_cleanup(gslotnum);
	}

	gslotnum = sslotnum = 0;
    }

    /* Slot not found... */
    if (!sslotnum) {
	speedy_file_set_state(FS_WRITING);

	/* Try to find our group by name... */
	gslotnum = speedy_group_findname();

	/* Group not found so create one */
	if (!gslotnum) {
	    speedy_file_set_state(FS_WRITING);
	    gslotnum = speedy_group_create();
	}

	/* Create a new slot */
	sslotnum = speedy_slot_alloc();
	sslot = &FILE_SLOT(scr_slot, sslotnum);
	sslot->dev_num = script_stat.st_dev;
	sslot->ino_num = script_stat.st_ino;
	sslot->mtime = script_stat.st_mtime;

	/* Add to this group */
	gslot = &FILE_SLOT(gr_slot, gslotnum);
	sslot->next_slot = gslot->script_head;
	gslot->script_head = sslotnum;
    }

    speedy_file_set_state(FS_LOCKED);

    *gslotnum_p = gslotnum;
    *sslotnum_p = sslotnum;
}

static SpeedyMapInfo *script_mapinfo;

void speedy_script_munmap() {
    if (script_mapinfo) {
	speedy_util_mapout(script_mapinfo);
	script_mapinfo = NULL;
    }
}

SpeedyMapInfo *speedy_script_mmap(int max_size) {
    speedy_script_munmap();
    script_mapinfo = speedy_util_mapin(
	speedy_script_open(), max_size, speedy_script_getstat()->st_size
    );
    return script_mapinfo;
}
