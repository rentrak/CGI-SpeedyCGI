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

#define LISTEN_BACKLOG		2

#ifndef PREF_FD_ACCEPT_S
#   define PREF_FD_ACCEPT_S -1
#endif
#ifndef PREF_FD_ACCEPT_E
#   define PREF_FD_ACCEPT_E -1
#endif
#ifndef PREF_FD_LISTENER
#   define PREF_FD_LISTENER -1
#endif
#ifndef PREF_FD_CONNECT_S
#   define PREF_FD_CONNECT_S -1
#endif
#ifndef PREF_FD_CONNECT_E
#   define PREF_FD_CONNECT_E -1
#endif

void speedy_ipc_listen(slotnum_t slotnum);
void speedy_ipc_listen_fixfd(slotnum_t slotnum);
void speedy_ipc_unlisten();
int speedy_ipc_connect(slotnum_t slotnum, int s, int e);
void speedy_ipc_connect_prepare(int *s, int *e);
int speedy_ipc_accept_ready(int wakeup);
int speedy_ipc_accept(int wakeup, int *s, int *e);
void speedy_ipc_cleanup(slotnum_t slotnum);
