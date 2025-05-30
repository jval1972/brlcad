/*                         P A T H S U M . C
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
/** @file libged/pathsum.c
 *
 * The paths and listeval command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"

#include "../ged_private.h"


int
ged_pathsum_core(struct ged *gedp, int argc, const char *argv[])
{
    int i, pos_in;
    int verbose;
    struct _ged_trace_data gtd;

    /* listeval */
    static const char *usage1 =
	"[-t] {path}\n{path} may be specified by '/' or space separated components, but not both";

    /* paths */
    static const char *usage2 =
	"{path_start}\n{path_start} may be specified by '/' or space separated components, but not both";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /*
     * paths are matched up to last input member
     * ANY path the same up to this point is considered as matching
     */

    /* initialize gtd */
    memset((char *)(&gtd), 0, sizeof(struct _ged_trace_data));
    gtd.gtd_gedp = gedp;
    gtd.gtd_flag = _GED_CPEVAL;
    gtd.gtd_prflag = 0;

    /* find out which command was entered */
    if (BU_STR_EQUAL(argv[0], "listeval")) {
	/* want to list evaluated solid[s] */
	gtd.gtd_flag = _GED_LISTEVAL;
    }
    if (BU_STR_EQUAL(argv[0], "paths")) {
	/* want to list all matching paths */
	gtd.gtd_flag = _GED_LISTPATH;
    }

    /* must be wanting help */
    if (argc == 1) {
	if (gtd.gtd_flag == _GED_LISTEVAL) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage1); /* listeval */
	} else {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage2); /* paths */
	}
	return GED_HELP;
    }

    if (BU_STR_EQUAL(argv[1], "-t") && gtd.gtd_flag == _GED_LISTEVAL) {
	pos_in = 2;
	verbose = 0;
    } else {
	pos_in = 1;
	verbose = 1;
    }

    gtd.gtd_objpos = 0;
    if (argc == (pos_in + 1) && strchr(argv[pos_in], '/')) {
	char *tok;
	tok = strtok((char *)argv[pos_in], "/");
	while (tok) {
	    if (gtd.gtd_objpos >= _GED_TRACE_MAX_LEVELS)
		break;
	    if ((gtd.gtd_obj[gtd.gtd_objpos++] = db_lookup(gedp->dbip, tok, LOOKUP_NOISY)) == RT_DIR_NULL) {
		return BRLCAD_ERROR;
	    }
	    tok = strtok((char *)NULL, "/");
	}
    } else {
	gtd.gtd_objpos = argc - pos_in;
	/* build directory pointer array for desired path */
	for (i = 0; i < gtd.gtd_objpos; i++) {
	    if ((gtd.gtd_obj[i] = db_lookup(gedp->dbip, argv[pos_in+i], LOOKUP_NOISY)) == RT_DIR_NULL) {
		return BRLCAD_ERROR;
	    }
	}
    }

    if (!gtd.gtd_objpos) {
	bu_vls_printf(gedp->ged_result_str, "Invalid path\n");
	return BRLCAD_ERROR;
    }

    MAT_IDN(gtd.gtd_xform);

    ged_trace(gtd.gtd_obj[0], 0, bn_mat_identity, &gtd, verbose);

    if (gtd.gtd_prflag == 0) {
	/* path not found */
	bu_vls_printf(gedp->ged_result_str, "PATH:  ");
	/* NOTE: gtd.gtd_obj size is limited to _BV_MAX_LEVLES - make sure our loop bounds dont exceed */
	for (i = 0; i < FMIN(gtd.gtd_objpos, _GED_TRACE_MAX_LEVELS); i++) {
	    bu_vls_printf(gedp->ged_result_str, "/%s", gtd.gtd_obj[i]->d_namep);
	}
	bu_vls_printf(gedp->ged_result_str, "  NOT FOUND\n");
    }

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl pathsum_cmd_impl = {"pathsum", ged_pathsum_core, GED_CMD_DEFAULT};
const struct ged_cmd pathsum_cmd = { &pathsum_cmd_impl };

struct ged_cmd_impl listeval_cmd_impl = {"listeval", ged_pathsum_core, GED_CMD_DEFAULT};
const struct ged_cmd listeval_cmd = { &listeval_cmd_impl };

struct ged_cmd_impl paths_cmd_impl = {"paths", ged_pathsum_core, GED_CMD_DEFAULT};
const struct ged_cmd paths_cmd = { &paths_cmd_impl };

const struct ged_cmd *pathsum_cmds[] = { &pathsum_cmd, &listeval_cmd, &paths_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  pathsum_cmds, 3 };

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
