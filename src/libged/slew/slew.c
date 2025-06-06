/*                         S L E W . C
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
/** @file libged/slew.c
 *
 * The slew command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_slew_core(struct ged *gedp, int argc, const char *argv[])
{
    vect_t svec;
    static const char *usage = "x y [z]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc == 2) {
	int n;

	if ((n = bn_decode_vect(svec, argv[1])) != 3) {
	    if (n != 2) {
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return BRLCAD_ERROR;
	    }

	    svec[Z] = 0.0;
	}

	return _ged_do_slew(gedp, svec);
    }

    if (argc == 3 || argc == 4) {
	double scan[3];

	if (sscanf(argv[1], "%lf", &scan[X]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "slew: bad X value %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}

	if (sscanf(argv[2], "%lf", &scan[Y]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "slew: bad Y value %s\n", argv[2]);
	    return BRLCAD_ERROR;
	}

	if (argc == 4) {
	    if (sscanf(argv[3], "%lf", &scan[Z]) != 1) {
		bu_vls_printf(gedp->ged_result_str, "slew: bad Z value %s\n", argv[3]);
		return BRLCAD_ERROR;
	    }
	} else
	    scan[Z] = 0.0;

	/* convert from double to fastf_t */
	VMOVE(svec, scan);

	return _ged_do_slew(gedp, svec);
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl slew_cmd_impl = {"slew", ged_slew_core, GED_CMD_DEFAULT};
const struct ged_cmd slew_cmd = { &slew_cmd_impl };

struct ged_cmd_impl sv_cmd_impl = {"sv", ged_slew_core, GED_CMD_DEFAULT};
const struct ged_cmd sv_cmd = { &sv_cmd_impl };

struct ged_cmd_impl vslew_cmd_impl = {"vslew", ged_slew_core, GED_CMD_DEFAULT};
const struct ged_cmd vslew_cmd = { &vslew_cmd_impl };

const struct ged_cmd *slew_cmds[] = { &slew_cmd, &sv_cmd, &vslew_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  slew_cmds, 3 };

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
