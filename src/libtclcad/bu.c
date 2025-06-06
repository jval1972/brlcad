/*                            B U . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
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
/**
 *
 * BRL-CAD's Tcl wrappers for libbu.
 *
 */

#include "common.h"

#define RESOURCE_INCLUDED 1
#include <tcl.h>
#ifdef HAVE_TK
#  include <tk.h>
#endif

#include "string.h" /* for strchr */

#include "vmath.h"
#include "bu.h"
#include "vmath.h"
#include "tclcad.h"


/* Private headers */
#include "brlcad_version.h"
#include "./tclcad_private.h"


static int
lwrapper_func(ClientData data, Tcl_Interp *interp, int argc, const char *argv[])
{
    struct bu_cmdtab *ctp = (struct bu_cmdtab *)data;

    return ctp->ct_func(interp, argc, argv);
}



#define TINYBUFSIZ 32
#define SMALLBUFSIZ 256

/**
 * Given arguments of alternating keywords and values and a specific
 * keyword ("Iwant"), return the value associated with that keyword.
 *
 * example: bu_get_value_by_keyword Iwant az 35 elev 25 temp 9.6
 *
 * If only one argument is given after the search keyword, it is
 * interpreted as a list in the same format.
 *
 * example: bu_get_value_by_keyword Iwant {az 35 elev 25 temp 9.6}
 *
 * Search order is left-to-right, only first match is returned.
 *
 * Sample use:
 * bu_get_value_by_keyword V8 [concat type [.inmem get box.s]]
 *
 * @param clientData	- associated data/state
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return BRLCAD_OK if successful, otherwise, BRLCAD_ERROR.
 */
static int
tcl_bu_get_value_by_keyword(void *clientData,
			    int argc,
			    const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    int i = 0;
    int listc = 0;
    const char *iwant = (const char *)NULL;
    const char **listv = (const char **)NULL;
    const char **tofree = (const char **)NULL;

    if (argc < 3) {
	char buf[TINYBUFSIZ];
	snprintf(buf, TINYBUFSIZ, "%d", argc);
	bu_log("bu_get_value_by_keyword: wrong # of args (%s).\n"
	       "Usage: bu_get_value_by_keyword iwant {list}\n"
	       "Usage: bu_get_value_by_keyword iwant key1 val1 key2 val2 ... keyN valN\n", buf);
	return BRLCAD_ERROR;
    }

    iwant = argv[1];

    if (argc == 3) {
	if (Tcl_SplitList(interp, argv[2], &listc, (const char ***)&listv) != TCL_OK) {
	    bu_log("bu_get_value_by_keyword: iwant='%s', unable to split '%s'\n", iwant, argv[2]);
	    return BRLCAD_ERROR;
	}
	tofree = listv;
    } else {
	/* Take search list from remaining arguments */
	listc = argc - 2;
	listv = argv + 2;
    }

    if ((listc & 1) != 0) {
	char buf[TINYBUFSIZ];
	snprintf(buf, TINYBUFSIZ, "%d", listc);
	bu_log("bu_get_value_by_keyword: odd # of items in list (%s).\n", buf);

	if (tofree)
	    Tcl_Free((char *)tofree); /* not bu_free() */
	return BRLCAD_ERROR;
    }

    for (i=0; i < listc; i += 2) {
	if (BU_STR_EQUAL(iwant, listv[i])) {
	    /* If value is a list, don't nest it in another list */
	    if (listv[i+1][0] == '{') {
		struct bu_vls str = BU_VLS_INIT_ZERO;

		/* Skip leading { */
		bu_vls_strcat(&str, &listv[i+1][1]);
		/* Trim trailing } */
		bu_vls_trunc(&str, -1);
		Tcl_AppendResult(interp, bu_vls_addr(&str), NULL);
		bu_vls_free(&str);
	    } else {
		Tcl_AppendResult(interp, listv[i+1], NULL);
	    }

	    if (tofree)
		Tcl_Free((char *)tofree); /* not bu_free() */
	    return BRLCAD_OK;
	}
    }

    /* Not found */

    if (tofree)
	Tcl_Free((char *)tofree); /* not bu_free() */
    return BRLCAD_ERROR;
}


/**
 * A wrapper for bu_rgb_to_hsv.
 *
 * @param clientData	- associated data/state
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return BRLCAD_OK if successful, otherwise, BRLCAD_ERROR.
 */
static int
tcl_bu_rgb_to_hsv(void *clientData,
		  int argc,
		  const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    int rgb_int[3];
    unsigned char rgb[3];
    fastf_t hsv[3];
    struct bu_vls result = BU_VLS_INIT_ZERO;

    if (argc != 4) {
	bu_log("Usage: bu_rgb_to_hsv R G B\n");
	return BRLCAD_ERROR;
    }
    if (sscanf(argv[1], "%d", &rgb_int[0]) != 1
	|| sscanf(argv[2], "%d", &rgb_int[1]) != 1
	|| sscanf(argv[3], "%d", &rgb_int[2]) != 1
	|| (rgb_int[0] < 0) || (rgb_int[0] > 255)
	|| (rgb_int[1] < 0) || (rgb_int[1] > 255)
	|| (rgb_int[2] < 0) || (rgb_int[2] > 255)) {
	bu_vls_printf(&result, "bu_rgb_to_hsv: Bad RGB (%s, %s, %s)\n",
		      argv[1], argv[2], argv[3]);
	bu_log("ERROR: %s", bu_vls_addr(&result));
	bu_vls_free(&result);
	return BRLCAD_ERROR;
    }
    rgb[0] = rgb_int[0];
    rgb[1] = rgb_int[1];
    rgb[2] = rgb_int[2];

    bu_rgb_to_hsv(rgb, hsv);
    bu_vls_printf(&result, "%g %g %g", hsv[0], hsv[1], hsv[2]);
    Tcl_AppendResult(interp, bu_vls_addr(&result), NULL);
    bu_vls_free(&result);
    return BRLCAD_OK;

}


/**
 * A wrapper for bu_hsv_to_rgb.
 *
 * @param clientData	- associated data/state
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return BRLCAD_OK if successful, otherwise, BRLCAD_ERROR.
 */
static int
tcl_bu_hsv_to_rgb(void *clientData,
		  int argc,
		  const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;

    double vals[3];
    fastf_t hsv[3];
    unsigned char rgb[3];
    struct bu_vls result = BU_VLS_INIT_ZERO;

    if (argc != 4) {
	bu_log("Usage: bu_hsv_to_rgb H S V\n");
	return BRLCAD_ERROR;
    }
    if (sscanf(argv[1], "%lf", &vals[0]) != 1
	|| sscanf(argv[2], "%lf", &vals[1]) != 1
	|| sscanf(argv[3], "%lf", &vals[2]) != 1)
    {
	bu_log("Bad HSV parsing (%s, %s, %s)\n", argv[1], argv[2], argv[3]);
	return BRLCAD_ERROR;
    }

    VMOVE(hsv, vals);
    if (bu_hsv_to_rgb(hsv, rgb) == 0) {
	bu_log("HSV to RGB conversion failed (%s, %s, %s)\n", argv[1], argv[2], argv[3]);
	return BRLCAD_ERROR;
    }

    bu_vls_printf(&result, "%d %d %d", rgb[0], rgb[1], rgb[2]);
    Tcl_AppendResult(interp, bu_vls_addr(&result), NULL);
    bu_vls_free(&result);
    return BRLCAD_OK;

}

const char *
_tclcad_bu_dir_print(const char *dirkey, int fail_quietly)
{
    static char result[MAXPATHLEN] = {0};
    if (BU_STR_EQUIV(dirkey, "curr") || BU_STR_EQUIV(dirkey, "cwd") ||
	    BU_STR_EQUAL(dirkey, "BU_DIR_CURR")) {
	snprintf(result, MAXPATHLEN, "%s", bu_dir(NULL, 0, BU_DIR_CURR, NULL));
	return result;
    }
    if (BU_STR_EQUIV(dirkey, "init") || BU_STR_EQUIV(dirkey, "iwd") ||
	    BU_STR_EQUAL(dirkey, "BU_DIR_INIT")) {
	snprintf(result, MAXPATHLEN, "%s", bu_dir(NULL, 0, BU_DIR_INIT, NULL));
	return result;
    }
    if (BU_STR_EQUIV(dirkey, "bin") || BU_STR_EQUAL(dirkey, "BU_DIR_BIN")) {
	snprintf(result, MAXPATHLEN, "%s", bu_dir(NULL, 0, BU_DIR_BIN, NULL));
	return result;
    }
    if (BU_STR_EQUIV(dirkey, "lib") || BU_STR_EQUAL(dirkey, "BU_DIR_LIB")) {
	snprintf(result, MAXPATHLEN, "%s", bu_dir(NULL, 0, BU_DIR_LIB, NULL));
	return result;
    }
    if (BU_STR_EQUIV(dirkey, "libexec") || BU_STR_EQUAL(dirkey, "BU_DIR_LIBEXEC")) {
	snprintf(result, MAXPATHLEN, "%s", bu_dir(NULL, 0, BU_DIR_LIBEXEC, NULL));
	return result;
    }
    if (BU_STR_EQUIV(dirkey, "include") || BU_STR_EQUAL(dirkey, "BU_DIR_INCLUDE")) {
	snprintf(result, MAXPATHLEN, "%s", bu_dir(NULL, 0, BU_DIR_INCLUDE, NULL));
	return result;
    }
    if (BU_STR_EQUIV(dirkey, "data") || BU_STR_EQUIV(dirkey, "share") ||
	    BU_STR_EQUAL(dirkey, "BU_DIR_DATA")) {
	snprintf(result, MAXPATHLEN, "%s", bu_dir(NULL, 0, BU_DIR_DATA, NULL));
	return result;
    }
    if (BU_STR_EQUIV(dirkey, "doc") || BU_STR_EQUAL(dirkey, "BU_DIR_DOC")) {
	snprintf(result, MAXPATHLEN, "%s", bu_dir(NULL, 0, BU_DIR_DOC, NULL));
	return result;
    }
    if (BU_STR_EQUIV(dirkey, "man") || BU_STR_EQUAL(dirkey, "BU_DIR_MAN")) {
	snprintf(result, MAXPATHLEN, "%s", bu_dir(NULL, 0, BU_DIR_MAN, NULL));
	return result;
    }
    if (BU_STR_EQUIV(dirkey, "temp") || BU_STR_EQUAL(dirkey, "BU_DIR_TEMP")) {
	snprintf(result, MAXPATHLEN, "%s", bu_dir(NULL, 0, BU_DIR_TEMP, NULL));
	return result;
    }
    if (BU_STR_EQUIV(dirkey, "home") || BU_STR_EQUAL(dirkey, "BU_DIR_HOME")) {
	snprintf(result, MAXPATHLEN, "%s", bu_dir(NULL, 0, BU_DIR_HOME, NULL));
	return result;
    }
    if (BU_STR_EQUIV(dirkey, "cache") || BU_STR_EQUAL(dirkey, "BU_DIR_CACHE")) {
	snprintf(result, MAXPATHLEN, "%s", bu_dir(NULL, 0, BU_DIR_CACHE, NULL));
	return result;
    }
    if (BU_STR_EQUIV(dirkey, "config") || BU_STR_EQUAL(dirkey, "BU_DIR_CONFIG")) {
	snprintf(result, MAXPATHLEN, "%s", bu_dir(NULL, 0, BU_DIR_CONFIG, NULL));
	return result;
    }
    if (BU_STR_EQUIV(dirkey, "ext") || BU_STR_EQUAL(dirkey, "BU_DIR_EXT")) {
	snprintf(result, MAXPATHLEN, "%s", bu_dir(NULL, 0, BU_DIR_EXT, NULL));
	return result;
    }
    if (BU_STR_EQUIV(dirkey, "libext") || BU_STR_EQUAL(dirkey, "BU_DIR_LIBEXT")) {
	snprintf(result, MAXPATHLEN, "%s", bu_dir(NULL, 0, BU_DIR_LIBEXT, NULL));
	return result;
    }
    if (!fail_quietly) {
	snprintf(result, MAXPATHLEN, "Unknown directory key %s", dirkey);
	return result;
    }
    return NULL;
}

/**
 * A wrapper for bu_dir.
 *
 * @param clientData	- associated data/state
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return BRLCAD_OK if successful, otherwise, BRLCAD_ERROR.
 */
static int
tcl_bu_dir(void *clientData,
		   int argc,
		   const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    if (argc != 2) {
	bu_log("Usage: bu_dir [curr|init|bin|lib|libexec|include|data|doc|man|temp|home|cache|config|ext|libext]\n");
	return BRLCAD_ERROR;
    }
    Tcl_AppendResult(interp, _tclcad_bu_dir_print(argv[1],1), NULL);
    return BRLCAD_OK;
}

/**
 * A wrapper for bu_units_conversion.
 *
 * @param clientData	- associated data/state
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return BRLCAD_OK if successful, otherwise, BRLCAD_ERROR.
 */
static int
tcl_bu_units_conversion(void *clientData,
			int argc,
			const char **argv)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    double conv_factor;
    struct bu_vls result = BU_VLS_INIT_ZERO;

    if (argc != 2) {
	bu_log("Usage: bu_units_conversion units_string\n");
	return BRLCAD_ERROR;
    }

    conv_factor = bu_units_conversion(argv[1]);
    if (conv_factor <= 0.0) {
	bu_log("ERROR: bu_units_conversion: Unrecognized units string: %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    bu_vls_printf(&result, "%.12e", conv_factor);
    Tcl_AppendResult(interp, bu_vls_addr(&result), NULL);
    bu_vls_free(&result);
    return BRLCAD_OK;
}


static void
register_cmds(Tcl_Interp *interp, struct bu_cmdtab *cmds)
{
    struct bu_cmdtab *ctp = NULL;

    for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
	(void)Tcl_CreateCommand(interp, ctp->ct_name, lwrapper_func, (ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
    }
}


int
Bu_Init(Tcl_Interp *interp)
{
    static struct bu_cmdtab cmds[] = {
	{"bu_units_conversion",		tcl_bu_units_conversion},
	{"bu_dir",		        tcl_bu_dir},
	{"bu_get_value_by_keyword",	tcl_bu_get_value_by_keyword},
	{"bu_rgb_to_hsv",		tcl_bu_rgb_to_hsv},
	{"bu_hsv_to_rgb",		tcl_bu_hsv_to_rgb},
	{(const char *)NULL, BU_CMD_NULL}
    };

    register_cmds(interp, cmds);

    Tcl_SetVar(interp, "BU_DEBUG_FORMAT", BU_DEBUG_FORMAT, TCL_GLOBAL_ONLY);
    Tcl_LinkVar(interp, "bu_debug", (char *)&bu_debug, TCL_LINK_INT);

    return BRLCAD_OK;
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
