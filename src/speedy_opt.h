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

typedef struct _OptRect {
    char		*name;
    char		letter;
    unsigned char	type;
    void		*value;
    char		changed;	/* No longer default */
} OptRec;

void speedy_opt_init(const char * const *argv, const char * const *envp);
void speedy_opt_read_shbang();
int speedy_opt_got_shbang();
int speedy_opt_set(OptRec *optrec, const char *value);
int speedy_opt_set_byname(const char *optname, const char *value);
const char *speedy_opt_get(OptRec *optrec);
void speedy_opt_set_script_argv(const char * const *argv);
const char * const *speedy_opt_script_argv();
char **speedy_opt_perl_argv();
const char * const *speedy_opt_exec_argv();
const char * const *speedy_opt_exec_envp();
const char * const *speedy_opt_orig_argv();
