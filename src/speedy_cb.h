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

typedef struct _CopyBuf {
    char *buf;
    int  sz;
    int  maxsz;
    int  eof;
    int  rdfd;
    int  wrfd;
    int	 write_err;
} CopyBuf;

#define CANREAD(buf)  ((buf).sz < (buf).maxsz && !(buf).eof && !(buf).write_err)
#define CANWRITE(buf) ((buf).sz > 0)
#define COPYDONE(buf) (((buf).eof != 0 && (buf).sz == 0) || (buf).write_err)

#define BUF_FULL(buf)			((buf).sz == (buf).maxsz)
#define BUF_SZ(buf)			((buf).sz)
#define BUF_EOF(buf)			((buf).eof)
#define BUF_SETEOF(buf)			(buf).eof = 1
#define BUF_SET_WRITE_ERR(buf,e)	(buf).write_err = (e)
#define BUF_GET_WRITE_ERR(buf)		((buf).write_err)

void speedy_cb_alloc(
    CopyBuf *bp, int maxsz, int rdfd, int wrfd, char *buf, int sz
);
void speedy_cb_free(CopyBuf *bp);
void speedy_cb_read(CopyBuf *bp);
void speedy_cb_write(CopyBuf *bp);
int speedy_cb_shift(CopyBuf *bp);
