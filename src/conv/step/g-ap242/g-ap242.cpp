/*                    G -  A P 2 4 2 . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2025 United States Government as represented by
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
/** @file step/g-ap242.cpp
 *
 * C++ main() for g-ap242 converter.
 *
 */

#include "common.h"

#include <iostream>

#include "bu/getopt.h"
#include "bu/vls.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"

//
// step-g related headers
//
#include <BRLCADWrapper.h>
#include <STEPWrapper.h>
#include <STEPfile.h>
#include <sdai.h>
#include <STEPcomplex.h>
#include <STEPattribute.h>
#include <SdaiHeaderSchema.h>
#include "AP242.h"
//#include "Comb_Tree.h"

//
// include NIST step related headers
//
#include <sdai.h>
#include <STEPfile.h>
#include "schema.h"

void
usage()
{
    std::cerr << "Usage: g-ap242 -o outfile.stp infile.g \n" << std::endl;
}


int
main(int argc, char *argv[])
{
    int ret = 0;

    bu_setprogname(argv[0]);

    // process command line arguments
    int c;
    char *output_file = (char *)NULL;
    while ((c = bu_getopt(argc, argv, "o:")) != -1) {
	switch (c) {
	    case 'o':
		output_file = bu_optarg;
		break;
	    default:
		usage();
		bu_exit(1, NULL);
		break;
	}
    }

    if (bu_optind >= argc || output_file == (char *)NULL) {
	usage();
	bu_exit(1, NULL);
    }

    argc -= bu_optind;
    argv += bu_optind;

    /* check our inputs/outputs */
    if (bu_file_exists(output_file, NULL)) {
	bu_exit(1, "ERROR: refusing to overwrite existing output file:\"%s\". Please remove file or change output file name and try again.", output_file);
    }

    if (!bu_file_exists(argv[0], NULL) && !BU_STR_EQUAL(argv[0], "-")) {
	bu_exit(2, "ERROR: unable to read input \"%s\" .g file", argv[0]);
    }

    if (argc < 2) {
	bu_exit(3, "ERROR: specify object to export");
    }


    std::string iflnm = argv[0];
    std::string oflnm = output_file;

    /* load .g file */
    BRLCADWrapper *dotg  = new BRLCADWrapper();
    if (!dotg) {
	std::cerr << "ERROR: unable to create BRL-CAD instance" << std::endl;
	return 3;
    }
    if (!dotg->load(iflnm)) {
	std::cerr << "ERROR: unable to open BRL-CAD input file [" << oflnm << "]" << std::endl;
	delete dotg;
	return 2;
    }

    struct db_i *dbip = dotg->GetDBIP();
    struct directory *dp = db_lookup(dbip, argv[1], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	std::cerr << "ERROR: cannot find " << argv[1] << "\n" << std::endl;
	delete dotg;
	return 1;
    }

    struct bu_vls scratch_string;
    bu_vls_init(&scratch_string);

    Registry *registry = new Registry(SchemaInit);
    InstMgr instance_list;
    STEPfile *sfile = new STEPfile(*registry, instance_list);

    registry->ResetSchemas();
    registry->ResetEntities();

    /* Populate the header instances */
    InstMgr *header_instances = sfile->HeaderInstances();

    /* 1 - Populate File_Name */
    SdaiFile_name * fn = (SdaiFile_name *)sfile->HeaderDefaultFileName();
    bu_vls_sprintf(&scratch_string, "'%s'", output_file);
    fn->name_(bu_vls_addr(&scratch_string));
    fn->time_stamp_("");
    StringAggregate_ptr author_tmp = new StringAggregate;
    author_tmp->AddNode(new StringNode("''"));
    fn->author_(author_tmp);
    StringAggregate_ptr org_tmp = new StringAggregate;
    org_tmp->AddNode(new StringNode("''"));
    fn->organization_(org_tmp);
    fn->preprocessor_version_("'BRL-CAD g-ap242 exporter'");
    fn->originating_system_("''");
    fn->authorization_("''");
    header_instances->Append((SDAI_Application_instance *)fn, completeSE);

    /* 2 - Populate File_Description */
    SdaiFile_description * fd = (SdaiFile_description *)sfile->HeaderDefaultFileDescription();
    StringAggregate_ptr description_tmp = new StringAggregate;
    description_tmp->AddNode(new StringNode("''"));
    fd->description_(description_tmp);
    fd->implementation_level_("'2;1'");
    header_instances->Append((SDAI_Application_instance *)fd, completeSE);

    /* 3 - Populate File_Schema */
    SdaiFile_schema *fs = (SdaiFile_schema *)sfile->HeaderDefaultFileSchema();
    StringAggregate_ptr schema_tmp = new StringAggregate;
    schema_tmp->AddNode(new StringNode("'Managed Model Based 3D Engineering'"));
    fs->schema_identifiers_(schema_tmp);
    header_instances->Append((SDAI_Application_instance *)fs, completeSE);

    /* Now, add actual DATA */
    struct rt_db_internal intern;
    rt_db_get_internal(&intern, dp, dbip, bn_mat_identity, &rt_uniresource);
    RT_CK_DB_INTERNAL(&intern);
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
    if (intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_COMBINATION) {
	//(void)AP242_Add_Tree(dp, wdbp, registry, &instance_list);
    } else {
	//(void)AP242_Add_Primitive(dp, wdbp, registry, &instance_list);
    }

    /* Write STEP file */
    if (!bu_file_exists(output_file, NULL)) {
	std::ofstream stepout(output_file);
	sfile->WriteExchangeFile(stepout);
    }

    /* Free memory */
    header_instances->DeleteInstances();
    //instance_list.DeleteInstances();
    delete dotg;
    delete registry;
    delete sfile;
    bu_vls_free(&scratch_string);

    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
