/*                         A E T . C
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
/** @file libged/aet.c
 *
 * The ae command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"
#include "./ged_view.h"

int
ged_aet_core(struct ged *gedp, int argc, const char *argv[])
{
    vect_t aet;
    int iflag = 0;
    static const char *usage = "[[-i] az el [tw]]";

    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get aet */
    if (argc == 1) {
	bn_encode_vect(gedp->ged_result_str, gedp->ged_gvp->gv_aet, 1);
	return BRLCAD_OK;
    }

    /* Check for -i option */
    if (argv[1][0] == '-' && argv[1][1] == 'i') {
	iflag = 1;  /* treat arguments as incremental values */
	++argv;
	--argc;
    }

    if (argc == 2) {
	/* set aet */
	int n;

	if ((n = bn_decode_vect(aet, argv[1])) == 2)
	    aet[2] = 0;
	else if (n != 3) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return BRLCAD_ERROR;
	}

	if (iflag) {
	    VADD2(gedp->ged_gvp->gv_aet, gedp->ged_gvp->gv_aet, aet);
	} else {
	    VMOVE(gedp->ged_gvp->gv_aet, aet);
	}
	bv_mat_aet(gedp->ged_gvp);
	bv_update(gedp->ged_gvp);

	return BRLCAD_OK;
    }

    if (argc == 3 || argc == 4) {
	double scan[3];

	if (sscanf(argv[1], "%lf", &scan[X]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad azimuth - %s\n", argv[0], argv[1]);
	    return BRLCAD_ERROR;
	}

	if (sscanf(argv[2], "%lf", &scan[Y]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad elevation - %s\n", argv[0], argv[2]);
	    return BRLCAD_ERROR;
	}

	if (argc == 4) {
	    if (sscanf(argv[3], "%lf", &scan[Z]) != 1) {
		bu_vls_printf(gedp->ged_result_str, "%s: bad twist - %s\n", argv[0], argv[3]);
		return BRLCAD_ERROR;
	    }
	} else
	    scan[Z] = 0.0;

	/* convert from double to fastf_t */
	VMOVE(aet, scan);

	if (iflag) {
	    VADD2(gedp->ged_gvp->gv_aet, gedp->ged_gvp->gv_aet, aet);
	} else {
	    VMOVE(gedp->ged_gvp->gv_aet, aet);
	}
	bv_mat_aet(gedp->ged_gvp);
	bv_update(gedp->ged_gvp);

	return BRLCAD_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
