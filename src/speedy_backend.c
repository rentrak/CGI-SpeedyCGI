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

static void be_wait_remove(gr_slot_t *gslot, slotnum_t bslotnum) {
    be_slot_t *bslot = &FILE_SLOT(be_slot, bslotnum);
    slotnum_t prev = bslot->be_wait_prev;
    slotnum_t next = bslot->be_wait_next;

    /* If we are the only be on this list our prev/next will both be zero */
    if (next || prev || gslot->be_wait == bslotnum) {
	if (prev)
	    FILE_SLOT(be_slot, prev).be_wait_next = next;
	else
	    gslot->be_wait = next;
	if (next)
	    FILE_SLOT(be_slot, next).be_wait_prev = prev;
    }
}

static void be_list_remove(gr_slot_t *gslot, be_slot_t *bslot) {
    slotnum_t prev = bslot->prev_slot;
    slotnum_t next = bslot->next_slot;

    if (gslot->be_head) {
	if (prev)
	    FILE_SLOT(be_slot, prev).next_slot = next;
	else
	    gslot->be_head = next;
	if (next)
	    FILE_SLOT(be_slot, next).prev_slot = prev;
    }
}

void speedy_backend_dispose(slotnum_t gslotnum, slotnum_t bslotnum) {
    if (gslotnum && bslotnum) {
	be_slot_t *bslot = &FILE_SLOT(be_slot, bslotnum);
	gr_slot_t *gslot = &FILE_SLOT(gr_slot, gslotnum);

	be_wait_remove(gslot, bslotnum);
	be_list_remove(gslot, bslot);
	speedy_ipc_cleanup(bslot->pid);
	speedy_slot_free(bslotnum);
    }
}

slotnum_t speedy_backend_be_wait_get(slotnum_t gslotnum) {
    gr_slot_t *gslot = &FILE_SLOT(gr_slot, gslotnum);
    slotnum_t head = gslot->be_wait;

    if (head) {
	be_slot_t *bslot = &FILE_SLOT(be_slot, head);
	slotnum_t next = bslot->be_wait_next;

	if (next)
	    FILE_SLOT(be_slot, next).be_wait_prev = 0;
	gslot->be_wait = next;
	bslot->be_wait_next = bslot->be_wait_prev = 0;
    }
    return head;
}

/* Insert into be_wait list, order reverse by maturity.  We want to go
 * at the beginning of our maturity list to preserve LIFO ordering so
 * inactive be's will die off.
 *
 * Do not use "next_slot" or "prev_slot" in the code below.  This is
 * the be_wait list (be_wait_next, be_wait_prev)
 */
void speedy_backend_be_wait_put(slotnum_t gslotnum, slotnum_t bslotnum) {
    gr_slot_t *gslot = &FILE_SLOT(gr_slot, gslotnum);
    be_slot_t *bslot = &FILE_SLOT(be_slot, bslotnum);
    int our_mat = bslot->maturity;

    if (!gslot->be_wait ||
	our_mat >= FILE_SLOT(be_slot, gslot->be_wait).maturity)
    {
	if (gslot->be_wait)
	    FILE_SLOT(be_slot, gslot->be_wait).be_wait_prev = bslotnum;
	bslot->be_wait_next = gslot->be_wait;
	bslot->be_wait_prev = 0;
	gslot->be_wait = bslotnum;
    } else {
	slotnum_t after = gslot->be_wait;

	while (1) {
	    be_slot_t *aft = &FILE_SLOT(be_slot, after);

	    if (!aft->be_wait_next ||
		our_mat >= FILE_SLOT(be_slot, aft->be_wait_next).maturity)
	    {
		if (aft->be_wait_next) {
		    FILE_SLOT(be_slot, aft->be_wait_next).be_wait_prev =
			bslotnum;
		}
		bslot->be_wait_next = aft->be_wait_next;
		bslot->be_wait_prev = after;
		aft->be_wait_next = bslotnum;
		break;
	    }
	    after = aft->be_wait_next;
	}
    }
}

void speedy_backend_kill(slotnum_t gslotnum, slotnum_t bslotnum) {
    speedy_util_kill(FILE_SLOT(be_slot, bslotnum).pid, SIGKILL);
    speedy_backend_dispose(gslotnum, bslotnum);
}


/* Run a check starting at this bslotnum.  Remove any empty groups
 * along the way.
 */
void speedy_backend_check(slotnum_t gslotnum, slotnum_t bslotnum) {
    int alldone = 0;
    slotnum_t gnext, bnext;

    /* Check first backend.  If dead, continue down the list */
    while (!alldone && gslotnum) {
	gr_slot_t *gslot = &FILE_SLOT(gr_slot, gslotnum);
	gnext = gslot->next_slot;

	if (!bslotnum)
	    bslotnum = gslot->be_head;

	while (!alldone && bslotnum) {
	    be_slot_t *bslot = &FILE_SLOT(be_slot, bslotnum);
	    bnext = bslot->next_slot;

	    /* If it's our pid, skip it */
	    if (bslot->pid != speedy_util_getpid()) {
		if (speedy_util_kill(bslot->pid, 0) == -1) {
		    /* This backend is not running, dispose of it */
		    speedy_backend_dispose(gslotnum, bslotnum);
		} else {
		    alldone = 1;
		}
	    }
	    bslotnum = bnext;
	}

	/* Try to remove this group if possible */
	speedy_group_cleanup(gslotnum);

	gslotnum = gnext;
    }
}

/* Check starting at the first backend in the file */
void speedy_backend_check_first() {
    slotnum_t gslotnum;

    if ((gslotnum = FILE_HEAD.group_head)) {
	speedy_backend_check(gslotnum, FILE_SLOT(gr_slot, gslotnum).be_head);
    }
}

slotnum_t speedy_backend_create_slot(slotnum_t gslotnum, pid_t pid) {
    gr_slot_t *gslot = &FILE_SLOT(gr_slot, gslotnum);
    be_slot_t *bslot;
    slotnum_t next_slot, bslotnum, front;

    /* Try to find our pid in the existing slots */
    for (bslotnum = gslot->be_head; bslotnum; bslotnum = next_slot) {
	bslot = &FILE_SLOT(be_slot, bslotnum);
	if (bslot->pid == pid)
	    return bslotnum;
	next_slot = bslot->next_slot;
    }

    /* Create our backend slot */
    bslotnum = speedy_slot_alloc();
    bslot = &FILE_SLOT(be_slot, bslotnum);
    bslot->pid = pid;
    bslot->maturity = bslot->be_wait_next = bslot->be_wait_prev = 0;

    /* Put our slot onto the group's be list */
    if ((front = gslot->be_head)) {
	FILE_SLOT(be_slot, front).prev_slot = bslotnum;
    }
    gslot->be_head = bslotnum;
    bslot->next_slot = front;
    bslot->prev_slot = 0;

    return bslotnum;
}
