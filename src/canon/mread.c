/*                       P N G - I P U . C
 * BRL-CAD
 *
 * Copyright (c) 1992-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file mread.c
 *
 * Provide a general means to a read some count of items from a file
 * descriptor reading multiple times until the quantity desired is
 * obtained.  This is necessary for pipes.
 */

#include "common.h"

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif


/** Read multiple times until quantity is obtained.  Necessary for
 *  pipes.
 */
int mread(int fd, char *bufp, int n)
{
    register int	count = 0;
    register int	nread;

    do {
	nread = read(fd, bufp, (unsigned)n-count);
	if(nread < 0)  {
	    return nread;
	}
	if(nread == 0)
	    return((int)count);
	count += (unsigned)nread;
	bufp += nread;
    } while(count < n);

    return((int)count);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */