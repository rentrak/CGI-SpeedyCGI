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

/* Allocate a slot */
slotnum_t speedy_slot_alloc() {
    slotnum_t slotnum;

    /* Try to get a slot from the beginning of the free list */
    if ((slotnum = FILE_HEAD.slot_free)) {

	/* Got it - remove it from the free list */
	FILE_HEAD.slot_free = FILE_SLOT(free_slot, slotnum).next_slot;
    } else {
	/* Allocate a new slot */
	slotnum = FILE_HEAD.slots_alloced + 1;

	/* Abort if too many slots */
	if (slotnum >= MAX_SLOTS)
	    DIE_QUIET("Out of slots");

	/* Check here if the file is large enough to hold this slot.
	 * The speedy_file code is supposed to allocate enough extra
	 * slots (MIN_SLOTS_FREE) when the file is locked to satisfy
	 * all slot_alloc's until the file is unlocked.  But if the
	 * code starts allocating too many slots for whatever reason,
	 * that will not work, and we'll drop off the end of the file.
	 * In that case, either fix the code or bump MIN_SLOTS_FREE
         */
	if (sizeof(file_head_t)+slotnum*sizeof(slot_t) > speedy_file_size()) {
	    speedy_util_die(
		"File too small for another slot while allocating slotnum %d. File size=%d. Try increasing MIN_SLOTS_FREE.",
		slotnum, speedy_file_size()
	    );
	}

	/* Successfully got a slot, so bump the count in the header */
	FILE_HEAD.slots_alloced++;
    }
    return slotnum;
}


/* Free a slot */
void speedy_slot_free(slotnum_t slotnum) {
    if (slotnum) {
	/* Put at beginning of free list */
	FILE_SLOT(free_slot, slotnum).next_slot = FILE_HEAD.slot_free;
	FILE_HEAD.slot_free = slotnum;
    }
}

slotnum_t speedy_slot_check(slotnum_t slotnum) {
    if (BAD_SLOTNUM(slotnum)) {
	DIE_QUIET("slotnum %d out of range, only %d alloced",
	    slotnum, FILE_HEAD.slots_alloced
	);
    }
    return slotnum;
}
