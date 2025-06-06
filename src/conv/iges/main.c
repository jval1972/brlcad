/*                          M A I N . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2025 United States Government as represented by
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
 */
/** @file iges/main.c
 *
 * IGES to BRL-CAD converter
 *
 */

#include "common.h"

#include "bu/app.h"
#include "bu/debug.h"
#include "bu/getopt.h"
/* private */
#include "./iges_struct.h"
#include "./iges_types.h"
#include "brlcad_ident.h"


int do_projection = 1;
char eord = 0;
char eofd = -1;
char card[256] = {0};
fastf_t scale = 0.0;
fastf_t inv_scale = 0.0;
fastf_t conv_factor = 0.0;
int units = 0;
int counter = 0;
int pstart = 0;
int dstart = 0;
size_t totentities = 0;
size_t dirarraylen = 0;
FILE *fd = NULL;
struct rt_wdb *fdout = NULL;
char brlcad_file[256] = {0};
int reclen = 0;
int currec = 0;
size_t ntypes = 0;
int brlcad_att_de = 0;
struct iges_directory **dir = NULL;
struct reglist *regroot = NULL;
struct iges_edge_list *edge_root = NULL;
struct iges_vertex_list *vertex_root = NULL;
struct bn_tol tol = BN_TOL_INIT_ZERO;
char *solid_name = NULL;
struct file_list iges_list = IGES_FILE_LIST_INIT_ZERO;
struct file_list *curr_file = NULL;
struct name_list *name_root = NULL;

char operators[] = {
    ' ',
    'u',
    '+',
    '-' };

mat_t *identity = NULL;


static int do_splines = 0;
static int do_drawings = 0;
static int trimmed_surf = 0;
int do_bots = 1;

static char *iges_file = NULL;

static char *msg1 =
"\nThis IGES file contains solid model entities, but your options do not permit\n\
converting them to BRL-CAD. You may want to try 'iges-g -o file.g %s' to\n\
convert the solid model elements\n";

static char *msg2 =
"\nThis IGES file contains drawing entities, but no solid model entities. You may\n\
convert the drawing to BRL-CAD by 'iges-g -d -o file.g %s'. Note that the resulting\n\
BRL-CAD object will be a 2D drawing, not a solid object. You might also try the\n\
'-3' option to get 3D drawings\n";

static char *msg3 =
"\nThis IGES file contains spline surfaces, but no solid model entities. All the spline\n\
surfaces in the IGES file may be combined into a single BRL-CAD spline solid by\n\
'iges-g -n -o file.g %s'\n";

static char *msg4 =
"\nThis IGES file contains trimmed surfaces, but no solid model entities.\n\
Try the '-t' option to convert all the trimmed surfaces into one BRL-CAD solid.\n\
'iges-g -t -o file.g %s'\n";

void
Suggestions(void)
{
    int i;
    int csg = 0;
    int b_rep = 0;
    int splines = 0;
    int tsurfs = 0;
    int drawing = 0;

    /* categorize the elements in the IGES file as to whether they are
     * CSG, BREP, Trimmed surfaces, Spline surfaces, or drawing elements
     */
    for (i = 0; i < NTYPES; i++) {
	if ((typecount[i].type >= 150 && typecount[i].type <= 184) ||
	    typecount[i].type == 430)
	    csg += typecount[i].count;
	else if (typecount[i].type == 186 ||
		 (typecount[i].type >= 502 && typecount[i].type <= 514))
	    b_rep += typecount[i].count;
	else if (typecount[i].type == 128)
	    splines += typecount[i].count;
	else if (typecount[i].type == 144)
	    tsurfs += typecount[i].count;
	else if ((typecount[i].type >= 100 && typecount[i].type <= 112) ||
		 typecount[i].type == 126 ||
		 (typecount[i].type >= 202 && typecount[i].type <= 230) ||
		 typecount[i].type == 404 || typecount[i].type == 410)
	    drawing += typecount[i].count;
    }

    if ((csg || b_rep) && (do_splines || do_drawings || trimmed_surf))
	bu_log(msg1, iges_file);

    if (drawing && csg == 0 && b_rep == 0 && !do_drawings)
	bu_log(msg2, iges_file);

    if (splines && csg == 0 && b_rep == 0 && !do_splines)
	bu_log(msg3, iges_file);

    if (tsurfs && csg == 0 && b_rep == 0 && !trimmed_surf)
	bu_log(msg4, iges_file);
}


int
main(int argc, char *argv [])
{
    int i;
    int c;
    int file_count = 0;
    char *output_file = (char *)NULL;
    struct bu_list *vlfree = &rt_vlfree;

    bu_setprogname(argv[0]);

    while ((c = bu_getopt(argc, argv, "3dntpo:x:X:N:")) != -1) {
	switch (c) {
	    case '3':
		do_drawings = 1;
		do_projection = 0;
		break;
	    case 'd':
		do_drawings = 1;
		break;
	    case 'n':
		do_splines = 1;
		break;
	    case 'o':
		output_file = bu_optarg;
		break;
	    case 't':
		trimmed_surf = 1;
		break;
	    case 'p':
		do_bots = 0;
		break;
	    case 'N':
		solid_name = bu_optarg;
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *)&rt_debug);
		break;
	    case 'X':
		sscanf(bu_optarg, "%x", (unsigned int *)&nmg_debug);
		break;
	    default:
		usage(argv[0]);
		break;
	}
    }

    if (bu_optind >= argc || output_file == (char *)NULL || do_drawings+do_splines+trimmed_surf > 1) {
	usage(argv[0]);
    }

    bu_log("%s", brlcad_ident("IGES to BRL-CAD Translator"));
    bu_log("Please direct bug reports to <bugs@brlcad.org>\n\n");

    /* Initialize some variables */
    ntypes = NTYPES;
    regroot = NULL;
    edge_root = NULL;
    vertex_root = NULL;
    name_root = NULL;
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    Initstack();	/* Initialize node stack */

    BU_ALLOC(identity, mat_t);
    for (i = 0; i < 16; i++) {
	if (!(i%5))
	    (*identity)[i] = 1.0;
	else
	    (*identity)[i] = 0.0;
    }

    if ((fdout = wdb_fopen(output_file)) == NULL) {
	bu_log("Cannot open %s\n", output_file);
	perror("iges-g");
	usage(argv[0]);
    }
    bu_strlcpy(brlcad_file,  output_file, sizeof(brlcad_file));

    argc -= bu_optind;
    argv += bu_optind;

    if (argc <= 0) {
	bu_exit(BRLCAD_ERROR, "Need filename\n");
    }

    BU_LIST_INIT(&iges_list.l);

    BU_ALLOC(curr_file, struct file_list);
    if (solid_name)
	bu_strlcpy(curr_file->obj_name, Make_unique_brl_name(solid_name), NAMESIZE+1);
    else
	bu_strlcpy(curr_file->obj_name, Make_unique_brl_name("all"), NAMESIZE+1);

    curr_file->file_name = (char *)bu_malloc(strlen(argv[0])+1, "iges-g: curr_file->file_name");
    bu_strlcpy(curr_file->file_name, argv[0], strlen(argv[0])+1);
    BU_LIST_APPEND(&iges_list.l, &curr_file->l);

    while (BU_LIST_NON_EMPTY(&iges_list.l)) {
	curr_file = BU_LIST_FIRST(file_list, &iges_list.l);
	iges_file = curr_file->file_name;

	fd = fopen(iges_file, "rb");	/* open IGES file */
	if (fd == NULL) {
	    bu_log("Cannot open %s\n", iges_file);
	    perror("iges-g");
	    usage(argv[0]);
	}

	bu_log("\n\n\nIGES FILE: %s\n", iges_file);

	reclen = Recsize() * sizeof(char); /* Check length of records */
	if (reclen == 0)
	    bu_exit(1, "File (%s) not in IGES ASCII format\n", iges_file);

	Freestack();	/* Set node stack to empty */

	Zero_counts();	/* Set summary information to all zeros */

	Readstart();	/* Read start section */

	Readglobal(file_count);	/* Read global section */

	pstart = Findp();	/* Find start of parameter section */

	Makedir();	/* Read directory section and build a linked list of entries */

	Summary();	/* Print a summary of what is in the IGES file */

	Docolor();	/* Get color info from color definition entities */

	Get_att();	/* Look for a BRL-CAD attribute definition */

	Evalxform();	/* Accumulate the transformation matrices */

	Check_names();	/* Look for name entities */

	if (do_drawings)
	    Conv_drawings(vlfree);	/* convert drawings to wire edges */
	else if (trimmed_surf) {
	    Do_subfigs();		/* Look for Singular Subfigure Instances */

	    Convtrimsurfs(vlfree);	/* try to convert trimmed surfaces to a single solid */
	} else if (do_splines)
	    Convsurfs();		/* Convert NURBS to a single solid */
	else {
	    Convinst();	/* Handle Instances */

	    Convsolids(vlfree);	/* Convert solid entities */

	    Convtree();	/* Convert Boolean Trees */

	    Convassem();	/* Convert solid assemblies */
	}

	Free_dir();

	BU_LIST_DEQUEUE(&curr_file->l);
	bu_free((char *)curr_file->file_name, "iges-g: curr_file->file_name");
	bu_free((char *)curr_file, "iges-g: curr_file");
	file_count++;
    }

    iges_file = argv[0];
    Suggestions();
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
