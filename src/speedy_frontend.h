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

void speedy_frontend_connect(int *s, int *e);
void speedy_frontend_dispose(slotnum_t gslotnum, slotnum_t fslotnum);
char *speedy_frontend_mkenv(
    const char * const * envp, const char * const * scr_argv,
    int min_alloc, int min_free,
    int *env_size, int *buf_size, int script_has_cwd
);
void speedy_frontend_proto2(int err_sock, int first_byte);


/* For strings shorter than this, use a one-byte string length when sending
 * strings from the frontend to the backend
 */
#define MAX_SHORT_STR 255

/* Bytes that tell the backend how to find the cwd */
#define SPEEDY_CWD_IN_SCRIPT	0	/* Cwd is in path to script */
#define SPEEDY_CWD_DEVINO	1	/* Cwd dev/inode to follow */
#define SPEEDY_CWD_UNKNOWN	2	/* Cwd dev/inode is unknown */

