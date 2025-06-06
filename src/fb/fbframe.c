/*                       F B F R A M E . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file fbframe.c
 *
 * Overwrite a frame (border) on the current framebuffer.  CCW from
 * the bottom: Red, Green, Blue, White
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bio.h"

#include "bu/app.h"
#include "bu/color.h"
#include "bu/getopt.h"
#include "bu/exit.h"
#include "bu/malloc.h"
#include "dm.h"

char *Usage="[-F framebuffer] [-s|S squareframesize] [-w|W frame_width] [-n|N frame_height]\n";

#define USAGE_EXIT(p) { fprintf(stderr, "Usage: %s %s\n", (p), Usage); \
	bu_exit(-1, NULL); }

int
main(int argc, char **argv)
{
    int c;
    int x;
    struct fb *fbp;
    int xsize, ysize;
    int len;
    char *framebuffer = (char *)NULL;
    unsigned char *line;
    static RGBpixel white = { 255, 255, 255 };
    static RGBpixel red = { 255, 0, 0 };
    static RGBpixel green = { 0, 255, 0 };
    static RGBpixel blue = { 0, 0, 255 };

    bu_setprogname(argv[0]);

    xsize = ysize = 0;
    while ((c = bu_getopt(argc, argv, "F:s:w:n:S:W:N:h?")) != -1) {
	switch (c) {
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    case 's':
	    case 'S':
		/* square file size */
		if ((len=atoi(bu_optarg)) > 0)
		    xsize = ysize = len;
		else
		    USAGE_EXIT(*argv);
		break;
	    case 'w':
	    case 'W':
		if ((len=atoi(bu_optarg)) > 0)
		    xsize = len;
		else
		    USAGE_EXIT(*argv);
		break;
	    case 'n':
	    case 'N':
		if ((len=atoi(bu_optarg)) > 0)
		    ysize = len;
		else
		    USAGE_EXIT(*argv);
		break;
	    default:	/* '?' 'h' */
		USAGE_EXIT(*argv);
		break;
	}
    }

    if (argc == 1 && isatty(fileno(stdin)) && isatty(fileno(stdout)))
	USAGE_EXIT(*argv);

    if ((fbp = fb_open(framebuffer, xsize, ysize)) == FB_NULL)
	bu_exit(1, NULL);

    if (xsize <= 0)
	xsize = fb_getwidth(fbp);
    if (ysize <= 0)
	ysize = fb_getheight(fbp);

    /* malloc buffer for pixel lines */
    len = (xsize > ysize) ? xsize : ysize;
    line = (unsigned char *)bu_calloc(len, sizeof(RGBpixel), "line");
    if (line == RGBPIXEL_NULL) {
	fprintf(stderr, "fbframe:  malloc failure\n");
	return 1;
    }

#define FLOOD(col) { for (x=len-1; x >= 0; x--) {COPYRGB(&line[3*x], col);} }

    /*
     * Red:	(0->510,      0)
     * Green:	(511, 0->510)
     * Blue:	(511->1,    511)
     * White:	(0, 511->1)
     */
    FLOOD(red);
    fb_writerect(fbp, 0, 0, xsize-1, 1, line);
    FLOOD(green);
    fb_writerect(fbp, xsize-1, 0, 1, ysize-1, line);
    FLOOD(blue);
    fb_writerect(fbp, 1, ysize-1, xsize-1, 1, line);
    FLOOD(white);
    fb_writerect(fbp, 0, 1, 1, ysize-1, line);

    fb_close(fbp);

    bu_free(line, "line");

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
