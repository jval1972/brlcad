/*                        M I R R O R . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/mirror.c
 *
 * The mirror command.
 *
 */

#include "bu/getopt.h"
#include "bv/util.h"
#include "ged.h"


int
ged_mirror_core(struct ged *gedp, int argc, const char *argv[])
{
    /* trailing x|y|z intentionally not documented */
    static const char *usage = "[-h] [-p \"point\"] [-d \"dir\"] [-x|-y|-z] [-o offset] old new";

    int k;

    point_t mirror_pt = {0.0, 0.0, 0.0};
    vect_t mirror_dir = {1.0, 0.0, 0.0};

    /* intentionally double for scanning */
    double scanpt[3];
    double scandir[3];
    double mirror_offset = 0.0;

    int ret;
    struct rt_db_internal *ip;
    struct rt_db_internal internal;
    struct directory *dp;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    bu_optind = 1;
    while ((k = bu_getopt(argc, (char * const *)argv, (const char *)"d:D:hHo:O:p:P:xXyYzZ?")) != -1) {
	if (bu_optopt == '?') k='h';
	switch (k) {
	    case 'p':
	    case 'P':
		if (sscanf(bu_optarg, "%lf %lf %lf",
			   &scanpt[X],
			   &scanpt[Y],
			   &scanpt[Z]) != 3) {
		    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		    return BRLCAD_ERROR;
		}

		/* convert from double to fastf_t */
		VMOVE(mirror_pt, scanpt);

		break;
	    case 'd':
	    case 'D':
		if (sscanf(bu_optarg, "%lf %lf %lf",
			   &scandir[X],
			   &scandir[Y],
			   &scandir[Z]) != 3) {
		    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		    return BRLCAD_ERROR;
		}

		/* convert from double to fastf_t */
		VMOVE(mirror_dir, scandir);

		break;
	    case 'o':
	    case 'O':
		if (sscanf(bu_optarg, "%lf", &mirror_offset) != 1) {
		    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		    return BRLCAD_ERROR;
		}
		break;
	    case 'x':
	    case 'X':
		VSET(mirror_dir, 1.0, 0.0, 0.0);
		break;
	    case 'y':
	    case 'Y':
		VSET(mirror_dir, 0.0, 1.0, 0.0);
		break;
	    case 'z':
	    case 'Z':
		VSET(mirror_dir, 0.0, 0.0, 1.0);
		break;
	    case 'h':
	    case 'H':
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return GED_HELP;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return BRLCAD_ERROR;
		break;
	}
    }

    argc -= bu_optind;

    if (argc < 2 || argc > 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    } else if (argc == 3) {
	/* support a trailing x|y|z option as classic command
	 * behavior.  THIS IS INTENTIONALLY UNDOCUMENTED.  if users
	 * have to read the usage, they can learn the new form.
	 */
	switch (argv[bu_optind+2][0]) {
	    case 'x':
	    case 'X':
		VSET(mirror_dir, 1.0, 0.0, 0.0);
		break;
	    case 'y':
	    case 'Y':
		VSET(mirror_dir, 0.0, 1.0, 0.0);
		break;
	    case 'z':
	    case 'Z':
		VSET(mirror_dir, 0.0, 0.0, 1.0);
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return BRLCAD_ERROR;
		break;
	}
    }

    /* make sure object mirroring to does not already exist */
    if (db_lookup(gedp->dbip, argv[bu_optind+1], LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s already exists\n", argv[bu_optind+1]);
	return BRLCAD_ERROR;
    }

    /* look up the object being mirrored */
    if ((dp = db_lookup(gedp->dbip, argv[bu_optind], LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Unable to find solid [%s]\n", argv[bu_optind]);
	return BRLCAD_ERROR;
    }

    /* get object being mirrored */
    ret = rt_db_get_internal(&internal, dp, gedp->dbip, NULL, wdbp->wdb_resp);
    if (ret < 0) {
	bu_vls_printf(gedp->ged_result_str, "Unable to load solid [%s]\n", argv[bu_optind]);
	return BRLCAD_ERROR;
    }

    mirror_offset *= gedp->dbip->dbi_local2base;
    VUNITIZE(mirror_dir);
    VJOIN1(mirror_pt, mirror_pt, mirror_offset, mirror_dir);

    /* mirror the object */
    ip = rt_mirror(gedp->dbip,
		   &internal,
		   mirror_pt,
		   mirror_dir,
		   wdbp->wdb_resp);
    if (ip == NULL) {
	bu_vls_printf(gedp->ged_result_str, "Unable to mirror [%s]", argv[bu_optind]);
	return BRLCAD_ERROR;
    }

    /* add the mirrored object to the directory */
    dp = db_diradd(gedp->dbip, argv[bu_optind+1], RT_DIR_PHONY_ADDR, 0, dp->d_flags, (void *)&ip->idb_type);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Unable to add [%s] to the database directory", argv[bu_optind+1]);
	return BRLCAD_ERROR;
    }
    /* save the mirrored object to disk */
    if (rt_db_put_internal(dp, gedp->dbip, ip, wdbp->wdb_resp) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Unable to store [%s] to the database", argv[bu_optind+1]);
	return BRLCAD_ERROR;
    }

    {
	/* draw the new object */
	const char *object = (const char *)argv[bu_optind+1];
	const char *e_argv[2] = {"draw", NULL};
	e_argv[1] = object;
	(void)ged_exec_draw(gedp, 2, e_argv);
	bv_update(gedp->ged_gvp);
    }

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl mirror_cmd_impl = {
    "mirror",
    ged_mirror_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd mirror_cmd = { &mirror_cmd_impl };
const struct ged_cmd *mirror_cmds[] = { &mirror_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  mirror_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
