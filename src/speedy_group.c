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

static void setname(gr_slot_t *gslot, const char *bytes, int len) {
    if (!gslot->name) {
	gslot->name = speedy_slot_alloc();
    }
    speedy_memcpy(
	FILE_SLOT(grnm_slot, gslot->name).name, bytes, min(len, GR_NAMELEN)
    );
}

static void remove_scripts(gr_slot_t *gslot) {
    slotnum_t snum, next;

    for (snum = gslot->script_head; snum; snum = next) {
	next = FILE_SLOT(scr_slot, snum).next_slot;
	speedy_slot_free(snum);
    }
    gslot->script_head = 0;
}

void speedy_group_invalidate(slotnum_t gslotnum) {
    gr_slot_t *gslot = &FILE_SLOT(gr_slot, gslotnum);
    slotnum_t bslotnum, next_slot;

    /* Remove scripts from the script list */
    remove_scripts(gslot);

    /* Remove the group name if any */
    speedy_slot_free(gslot->name);

    /* Shutdowns all the BE's in the idle queue */
    for (bslotnum = gslot->be_wait; bslotnum; bslotnum = next_slot) {
	next_slot = FILE_SLOT(be_slot, bslotnum).be_wait_next;
	speedy_backend_kill(gslotnum, bslotnum);
    }
}

int speedy_group_isvalid(slotnum_t gslotnum) {
    return FILE_SLOT(gr_slot, gslotnum).script_head;
}

void speedy_group_setname(slotnum_t gslotnum, const char *name) {
    setname(&FILE_SLOT(gr_slot, gslotnum), name, strlen(name)+1);
}

void speedy_group_sendsigs(slotnum_t gslotnum) {
    gr_slot_t *gslot = &FILE_SLOT(gr_slot, gslotnum);
    slotnum_t fslotnum, bslotnum;
    be_slot_t *bslot;
    fe_slot_t *fslot;

    /* Get first slot in the fe_wait list */
    fslotnum = gslot->fe_wait;

    /* Loop over each backend slot in the wait list */
    for (bslotnum = gslot->be_wait;
         bslotnum && fslotnum;
         bslotnum = bslot->next_slot)
    {
	slotnum_t next_slot;

	bslot = &FILE_SLOT(be_slot, bslotnum);

	for (; fslotnum; fslotnum = next_slot) {
	    /* Get next FE */
	    fslot = &FILE_SLOT(fe_slot, fslotnum);
	    next_slot = fslot->next_slot;

	    /* If it's not us send an ALRM signal */
	    if (speedy_util_kill(fslot->pid, SIGALRM) != -1) {
		fslot->sent_sig = 1;
		break;
	    }

	    /* Failed, remove this FE and try again */
	    speedy_frontend_dispose(gslotnum, fslotnum);
	}
    }
}

static void remove_group(slotnum_t *ptr, slotnum_t gslotnum) {
    while (*ptr != gslotnum && *ptr) {
	ptr = &(FILE_SLOT(gr_slot, *ptr).next_slot);
    }
    if (*ptr)
	*ptr = SLOT_CHECK(FILE_SLOT(gr_slot, gslotnum).next_slot);
}

static void group_dispose(slotnum_t gslotnum) {
    speedy_group_invalidate(gslotnum);
    remove_group(&(FILE_HEAD.group_head), gslotnum);
    speedy_slot_free(gslotnum);
}

/* Cleanup this group after an fe/be has been removed */
void speedy_group_cleanup(slotnum_t gslotnum) {
    gr_slot_t *gslot = &FILE_SLOT(gr_slot, gslotnum);

    /* No cleanup if there are still be's or fe's */
    if (!gslot->be_head && !gslot->fe_wait)
	group_dispose(gslotnum);
}
