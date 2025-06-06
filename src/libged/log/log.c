/*                           L O G . C
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
/** @file libged/log.c
 *
 * The log command.
 *
 */

#include "common.h"

#include <string.h>

#include "ged.h"


/**
 * Get the output from bu_log and append it to clientdata vls.
 */
static int
log_hook(void *clientdata,
	 void *str)
{
    struct bu_vls *vp = (struct bu_vls *)clientdata;
    size_t len;

    BU_CK_VLS(vp);
    len = bu_vls_strlen(vp);
    bu_vls_strcat(vp, (const char *)str);
    len = bu_vls_strlen(vp) - len;

    return (int)len;
}


int
ged_log_core(struct ged *gedp, int argc, const char *argv[])
{
    static char *usage = "get|start|stop";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (argv[1][0] == 'g' && BU_STR_EQUAL(argv[1], "get")) {
	bu_vls_vlscatzap(gedp->ged_result_str, gedp->ged_log);
	return BRLCAD_OK;
    }

    if (argv[1][0] == 's' && BU_STR_EQUAL(argv[1], "start")) {
	bu_log_add_hook(log_hook, (void *)gedp->ged_log);
	return BRLCAD_OK;
    }

    if (argv[1][0] == 's' && BU_STR_EQUAL(argv[1], "stop")) {
	bu_log_delete_hook(log_hook, (void *)gedp->ged_log);
	return BRLCAD_OK;
    }

    bu_log("Usage: %s %s ", argv[0], usage);
    return BRLCAD_ERROR;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl log_cmd_impl = {
    "log",
    ged_log_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd log_cmd = { &log_cmd_impl };
const struct ged_cmd *log_cmds[] = { &log_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  log_cmds, 1 };

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
