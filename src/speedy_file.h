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

typedef struct _file_head {
    struct timeval	create_time;
    slotnum_t		group_head;
    slotnum_t		slot_free;
    slotnum_t		slots_alloced;
    unsigned char	file_corrupt;
    unsigned char	file_removed;
} file_head_t;

typedef struct _file {
    file_head_t		file_head;
    slot_t		slots[1];
} speedy_file_t;

#define FILE_ALLOC_CHUNK	512
#define FILE_REV		2
#define FILE_HEAD		(speedy_file_maddr->file_head)
#define FILE_SLOTS		(speedy_file_maddr->slots)
#define FILE_SLOT(member, n)	(FILE_SLOTS[SLOT_CHECK(n)-1].member)
#define MIN_SLOTS_FREE		5

/* File access states */
#define FS_CLOSED	0	/* File is closed, not mapped */
#define FS_OPEN		1	/* Keep open for performance only */
#define FS_HAVESLOTS	2	/* Keep open - we are holding onto slots in
				   this file */
#define FS_LOCKED	3	/* Locked, mmaped, read-only */
#define FS_WRITING	4	/* Locked, mmaped, writing to file */

#ifndef PREF_FD_FILE
#   define PREF_FD_FILE -1
#endif

extern speedy_file_t *speedy_file_maddr;
void speedy_file_fd_is_suspect();
int speedy_file_size();
void speedy_file_set_state(int new_state);
void speedy_file_need_reopen();
