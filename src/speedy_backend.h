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

void speedy_backend_dispose(slotnum_t gslotnum, slotnum_t bslotnum);
void speedy_backend_kill(slotnum_t gslotnum, slotnum_t bslotnum);
slotnum_t speedy_backend_be_wait_get(slotnum_t gslotnum);
void speedy_backend_be_wait_put(slotnum_t gslotnum, slotnum_t bslotnum);
void speedy_backend_kill(slotnum_t gslotnum, slotnum_t bslotnum);
void speedy_backend_check(slotnum_t gslotnum, slotnum_t bslotnum);
void speedy_backend_check_next(slotnum_t gslotnum, slotnum_t bslotnum);
slotnum_t speedy_backend_create_slot(slotnum_t gslotnum);
void speedy_backend_kill_idle(slotnum_t gslotnum);
