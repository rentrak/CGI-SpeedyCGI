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

#define DIE_QUIET errno = 0; speedy_util_die

int speedy_util_pref_fd(int oldfd, int newfd);
int speedy_util_geteuid();
int speedy_util_argc(const char * const * argv);
int speedy_util_getpid();
void speedy_util_die(const char *fmt, ...);
int speedy_util_kill(pid_t pid, int sig);
int speedy_util_execvp(const char *filename, const char *const *argv);
