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

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

#ifndef MAP_FAILED
#   define MAP_FAILED (-1)
#endif

#ifdef __GNUC__
#define SPEEDY_INLINE __inline__
#else
#define SPEEDY_INLINE
#endif

#ifdef EWOULDBLOCK
#   define SP_EWOULDBLOCK(e) ((e) == EWOULDBLOCK)
#else
#   define SP_EWOULDBLOCK(e) 0
#endif
#ifdef EAGAIN
#   define SP_EAGAIN(e) ((e) == EAGAIN)
#else
#   define SP_EAGAIN(e) 0
#endif
#define SP_NOTREADY(e) (SP_EAGAIN(e) || SP_EWOULDBLOCK(e))

typedef struct {
    dev_t	d;
    ino_t	i;
} SpeedyDevIno;

#include "speedy_util.h"
#include "speedy_opt.h"
#include "speedy_optdefs.h"
#include "speedy_poll.h"
#include "speedy_slot.h"
#include "speedy_ipc.h"
#include "speedy_group.h"
#include "speedy_backend.h"
#include "speedy_frontend.h"
#include "speedy_file.h"
#include "speedy_script.h"
#include "speedy_cb.h"
#include "speedy_perl.h"
