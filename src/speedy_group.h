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

void speedy_group_invalidate(slotnum_t gslotnum);
void speedy_group_sendsigs(slotnum_t gslotnum);
void speedy_group_cleanup(slotnum_t gslotnum);
slotnum_t speedy_group_create();
slotnum_t speedy_group_findname();

#define DOING_SINGLE_SCRIPT \
    OPTVAL_GROUP[0] == 'n' && \
    OPTVAL_GROUP[1] == 'o' && \
    OPTVAL_GROUP[2] == 'n' && \
    OPTVAL_GROUP[3] == 'e' && \
    OPTVAL_GROUP[4] == '\0'

#define speedy_group_isvalid(gslotnum) \
    (FILE_SLOT(gr_slot, (gslotnum)).script_head != 0)
