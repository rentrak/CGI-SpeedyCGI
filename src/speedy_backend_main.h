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

/* Glue */

#define speedy_memcpy(d,s,n)	Copy(s,d,n,char)
#define speedy_memmove(d,s,n)	Move(s,d,n,char)
#define speedy_bzero(s,n)	Zero(s,n,char)
#define speedy_free(s)		Safefree(s)
#define speedy_execvp(f,a)	execvp(f,(char * const*)a)

void *speedy_malloc(int n);
void *speedy_realloc(void *ptr, size_t size);

/* Our preferred file descriptors */

#define PREF_FD_ACCEPT_S	0
#define PREF_FD_ACCEPT_E	2
#define PREF_FD_FILE		17
#define PREF_FD_LISTENER	18
#define PREF_FD_CWD		19
