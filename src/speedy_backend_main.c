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

/*
 * Backend program that runs the perl interpreter
 */

#include "speedy.h"

/* Find our existing slot, or create a new one if not found in file
 */
static slotnum_t get_slot(slotnum_t gslotnum, pid_t pid) {
    gr_slot_t *gslot = &FILE_SLOT(gr_slot, gslotnum);
    be_slot_t *bslot;
    slotnum_t next_slot, bslotnum;

    /* Try to find our pid in the existing slots */
    for (bslotnum = gslot->be_head; bslotnum; bslotnum = next_slot) {
	bslot = &FILE_SLOT(be_slot, bslotnum);
	if (bslot->pid == pid)
	    return bslotnum;
	next_slot = bslot->next_slot;
    }

    /* Create a new backend slot and write our pid there */
    bslotnum = speedy_backend_create_slot(gslotnum);
    FILE_SLOT(be_slot, bslotnum).pid = pid;

    return bslotnum;
}

int main(int argc, char **argv, char **_junk) {
    extern char **environ;
    slotnum_t gslotnum, sslotnum, bslotnum;
    int i;

    /* Close off all I/O except for stderr (close it later) */
    for (i = 32; i >= 0; --i) {
	if (i != 2 && i != PREF_FD_LISTENER)
	    (void) close(i);
    }

    /* Initialize options */
    speedy_opt_init((const char * const *)argv, (const char * const *)environ);
    
    /* Open/Stat the script - this could hang */
    (void) speedy_script_open();
    speedy_opt_read_shbang();

    /* Lock/mmap our temp file */
    speedy_file_set_state(FS_LOCKED);

    /* Locate our script and group */
    speedy_script_find(&gslotnum, &sslotnum);

    speedy_file_set_state(FS_WRITING);

    /* Get a backend slot */
    bslotnum = get_slot(gslotnum, speedy_util_getpid());

    /* Done with the temp file for now */
    speedy_file_set_state(FS_HAVESLOTS);

    /* Run the perl backend */
    speedy_perl_run(gslotnum, bslotnum);

    return 0;
}

/*
 * Glue Functions
 */

void *speedy_malloc(int n) {
    void *s;
    New(123,s,n,char);
    return s;
}

void *speedy_realloc(void *ptr, size_t size) {
    Renew(ptr, size, char);
    return ptr;
}
