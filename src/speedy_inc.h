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

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

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
#include "speedy_util.h"
#include "speedy_cb.h"
#include "speedy_perl.h"
