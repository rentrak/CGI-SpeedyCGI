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

typedef unsigned short slotnum_t;

#define GR_NAMELEN	max(sizeof(gr_slot_t), sizeof(scr_slot_t))

typedef struct _scr_slot { /* 18 bytes for a 64-bit dev_t, 14 if 32-bit */
    slotnum_t	next_slot;
    dev_t	dev_num;
    ino_t	ino_num;
    time_t	mtime;
} scr_slot_t;

#define PROC_SLOT_COMMON \
    slotnum_t	next_slot; \
    slotnum_t	prev_slot; \
    pid_t	pid;
    
typedef struct _proc_slot { /* 8 bytes */
    PROC_SLOT_COMMON
} proc_slot_t;

typedef struct _be_slot { /* 13 bytes */
    PROC_SLOT_COMMON
    slotnum_t	be_wait_next;
    slotnum_t	be_wait_prev;
    char	maturity;
} be_slot_t;

typedef struct _fe_slot { /* 13 bytes */
    PROC_SLOT_COMMON
    char	sent_sig;
} fe_slot_t;

typedef struct _gr_slot { /* 14 bytes */
    slotnum_t	next_slot;
    slotnum_t	script_head;
    slotnum_t	name;
    slotnum_t	be_head;
    slotnum_t	be_wait;
    slotnum_t	fe_wait;
    slotnum_t	fe_tail;
} gr_slot_t;

typedef struct _free_slot { /* 2 bytes */
    slotnum_t	next_slot;
} free_slot_t;

typedef struct _grnm_slot {
    char name[GR_NAMELEN];
} grnm_slot_t;

typedef union _slot {
    scr_slot_t	scr_slot;
    proc_slot_t	proc_slot;
    be_slot_t	be_slot;
    fe_slot_t	fe_slot;
    gr_slot_t	gr_slot;
    grnm_slot_t	grnm_slot;
    free_slot_t	free_slot;
} slot_t;

#define MAX_SLOTS ((1<<(sizeof(slotnum_t)*8))-6)
#define SLOT_CHECK(n) ((n) > FILE_HEAD.slots_alloced ? speedy_slot_check(n) : (n))

slotnum_t speedy_slot_alloc();
void speedy_slot_free(slotnum_t slotnum);
slotnum_t speedy_slot_check(slotnum_t slotnum);
