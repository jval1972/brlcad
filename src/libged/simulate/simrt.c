/*                         S I M R T . C
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
/*
 * Implements the raytrace based manifold generator.
 *
 * Used for returning the manifold points to physics.
 *
 */

/* Private Header */
#include "simrt.h"

/*
 * Global lists filled up while raytracing : remove these as in the forward
 * progression of a ray, the y needs to be increased gradually, no need to
 * record other info
 */
#define MAX_OVERLAPS 4
#define MAX_HITS 4

int num_hits = 0;
int num_overlaps = 0;
struct overlap overlap_list[MAX_OVERLAPS];
struct hit_reg hit_list[MAX_HITS];
struct rayshot_results rt_result;


void
print_rayshot_results(void)
{

    bu_log("print_rayshot_results: -------\n");

    bu_log("X bounds xr_min_x :%f, %f, %f \n",
	   V3ARGS(rt_result.xr_min_x));
    bu_log("X bounds xr_max_x :%f, %f, %f \n",
	   V3ARGS(rt_result.xr_max_x));
    bu_log("Y bounds xr_min_y :%f, %f, %f  >>> (%f, %f, %f)\n",
	   V3ARGS(rt_result.xr_min_y_in), V3ARGS(rt_result.xr_min_y_out));
    bu_log("Y bounds xr_max_y :%f, %f, %f >>> (%f, %f, %f)\n",
	   V3ARGS(rt_result.xr_max_y_in), V3ARGS(rt_result.xr_max_y_out));
    bu_log("Z bounds xr_min_z :%f, %f, %f\n",
	   V3ARGS(rt_result.xr_min_z_in));
    bu_log("Z bounds xr_max_z :%f, %f, %f\n",
	   V3ARGS(rt_result.xr_max_z_in));


}


int
cleanup_lists(void)
{
    num_hits = 0;
    num_overlaps = 0;

    return GED_OK;
}


int
get_overlap(struct rigid_body *rbA, struct rigid_body *rbB, vect_t overlap_min, vect_t overlap_max)
{
    bu_log("Calculating overlap between BB of %s(%f, %f, %f):(%f,%f,%f) \
			and %s(%f, %f, %f):(%f,%f,%f)",
	   rbA->rb_namep,
	   V3ARGS(rbA->btbb_min), V3ARGS(rbA->btbb_max),
	   rbB->rb_namep,
	   V3ARGS(rbB->btbb_min), V3ARGS(rbB->btbb_max)
	   );

    VMOVE(overlap_max, rbA->btbb_max);
    VMIN(overlap_max, rbB->btbb_max);

    VMOVE(overlap_min, rbA->btbb_min);
    VMAX(overlap_min, rbB->btbb_min);

    bu_log("Overlap volume between %s & %s is (%f, %f, %f):(%f,%f,%f)",
	   rbA->rb_namep,
	   rbB->rb_namep,
	   V3ARGS(overlap_min),
	   V3ARGS(overlap_max)
	   );


    return GED_OK;
}


int
exists_normal(vect_t n)
{
    int i;
    vect_t a;
    for(i=0; i<rt_result.num_normals; i++){
	VMOVE(a, rt_result.normals[i]);
	if(VEQUAL(a, n))
	    return 1;
    }

    return 0;
}


int
if_hit(struct application *ap, struct partition *part_headp, struct seg *UNUSED(segs))
{

    /* iterating over partitions, this will keep track of the current
     * partition we're working on.
     */
    struct partition *pp;

    /* will serve as a pointer for the entry and exit hitpoints */
    struct hit *hitp;

    /* will serve as a pointer to the solid primitive we hit */
    struct soltab *stp;

    /* will contain surface curvature information at the entry */
    struct curvature cur;

    /* will contain our hit point coordinate */
    point_t in_pt, out_pt;

    /* will contain normal vector where ray enters geometry */
    vect_t inormal;

    /* will contain normal vector where ray exits geometry */
    vect_t onormal;

    int i = 0;

    /* iterate over each partition until we get back to the head.
     * each partition corresponds to a specific homogeneous region of
     * material.
     */
    for (pp=part_headp->pt_forw; pp != part_headp; pp = pp->pt_forw) {

	if(num_hits < MAX_HITS){

	    i = num_hits;

	    /* print the name of the region we hit as well as the name of
	     * the primitives encountered on entry and exit.
	     */
	    /*	bu_log("\n--- Hit region %s (in %s, out %s)\n",
		pp->pt_regionp->reg_name,
		pp->pt_inseg->seg_stp->st_name,
		pp->pt_outseg->seg_stp->st_name );*/

	    /* Insert solid data into list node */
	    if(pp->pt_regionp->reg_name[0] == '/')
		hit_list[i].reg_name = (pp->pt_regionp->reg_name) + 1;
	    else
		hit_list[i].reg_name = pp->pt_regionp->reg_name;
	    hit_list[i].in_stp   = pp->pt_inseg->seg_stp;
	    hit_list[i].out_stp  = pp->pt_outseg->seg_stp;

	    /* entry hit point, so we type less */
	    hitp = pp->pt_inhit;

	    /* construct the actual (entry) hit-point from the ray and the
	     * distance to the intersection point (i.e., the 't' value).
	     */
	    VJOIN1(in_pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);

	    /* primitive we encountered on entry */
	    stp = pp->pt_inseg->seg_stp;

	    /* compute the normal vector at the entry point, flipping the
	     * normal if necessary.
	     */
	    RT_HIT_NORMAL(inormal, hitp, stp, &(ap->a_ray), pp->pt_inflip);

	    /* print the entry hit point info */
	    /*	rt_pr_hit("  In", hitp);
		VPRINT(   "  Ipoint", in_pt);
		VPRINT(   "  Inormal", inormal);*/


	    if (pp->pt_overlap_reg) {
		struct region *pp_reg;
		int j = -1;

		bu_log("    Claiming regions:\n");
		while ((pp_reg = pp->pt_overlap_reg[++j]))
		    bu_log("        %s\n", pp_reg->reg_name);
	    }

	    /* This next macro fills in the curvature information which
	     * consists of a principle direction vector, and the inverse
	     * radii of curvature along that direction and perpendicular
	     * to it.  Positive curvature bends toward the outward
	     * pointing normal.
	     */
	    RT_CURVATURE(&cur, hitp, pp->pt_inflip, stp);

	    /* print the entry curvature information */
	    /*	VPRINT("PDir", cur.crv_pdir);
		bu_log(" c1=%g\n", cur.crv_c1);
		bu_log(" c2=%g\n", cur.crv_c2);*/

	    /* Insert the data about the input point into the list */
	    VMOVE(hit_list[i].in_point, in_pt);
	    VMOVE(hit_list[i].in_normal, inormal);
	    hit_list[i].in_dist = hitp->hit_dist;
	    hit_list[i].cur = cur;

	    /* exit point, so we type less */
	    hitp = pp->pt_outhit;

	    /* construct the actual (exit) hit-point from the ray and the
	     * distance to the intersection point (i.e., the 't' value).
	     */
	    VJOIN1(out_pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);

	    /* primitive we exited from */
	    stp = pp->pt_outseg->seg_stp;

	    /* compute the normal vector at the exit point, flipping the
	     * normal if necessary.
	     */
	    RT_HIT_NORMAL(onormal, hitp, stp, &(ap->a_ray), pp->pt_outflip);

	    /* print the exit hit point info */
	    /*	rt_pr_hit("  Out", hitp);
		VPRINT(   "  Opoint", out_pt);
		VPRINT(   "  Onormal", onormal);*/

	    /* Insert the data about the input point into the list */
	    VMOVE(hit_list[i].out_point, out_pt);
	    VMOVE(hit_list[i].out_normal, onormal);
	    hit_list[i].out_dist = hitp->hit_dist;

	    /* Insert the new hit region into the list of hit regions */
	    hit_list[i].index = i;

	    num_hits++;

	}
	else{
	    bu_log("if_hit: WARNING Skipping hit region as maximum hits reached");
	}
    }

    return HIT;
}


int
if_miss(struct application *UNUSED(ap))
{
    bu_log("MISS");
    return MISS;
}


int
if_overlap(struct application *ap, struct partition *pp, struct region *reg1,
	   struct region *reg2, struct partition *InputHdp)
{
    int i = 0;

    bu_log("if_overlap: OVERLAP between %s and %s", reg1->reg_name, reg2->reg_name);


    if(num_overlaps < MAX_OVERLAPS){
	i = num_overlaps;
	overlap_list[i].ap = ap;
	overlap_list[i].pp = pp;
	overlap_list[i].reg1 = reg1;
	overlap_list[i].reg2 = reg2;
	overlap_list[i].in_dist = pp->pt_inhit->hit_dist;
	overlap_list[i].out_dist = pp->pt_outhit->hit_dist;
	VJOIN1(overlap_list[i].in_point, ap->a_ray.r_pt, pp->pt_inhit->hit_dist,
	       ap->a_ray.r_dir);
	VJOIN1(overlap_list[i].out_point, ap->a_ray.r_pt, pp->pt_outhit->hit_dist,
	       ap->a_ray.r_dir);


	/* compute the normal vector at the exit point, flipping the
	 * normal if necessary.
	 */
	RT_HIT_NORMAL(overlap_list[i].in_normal, pp->pt_inhit,
		      pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip);


	/* compute the normal vector at the exit point, flipping the
	 * normal if necessary.
	 */
	RT_HIT_NORMAL(overlap_list[i].out_normal, pp->pt_outhit,
		      pp->pt_outseg->seg_stp, &(ap->a_ray), pp->pt_outflip);

	overlap_list[i].index = i;
	num_overlaps++;

	bu_log("if_overlap: Set overlap %d, in_normal: %f,%f,%f , out_normal: %f,%f,%f",
	       overlap_list[i].index,
	       V3ARGS(overlap_list[i].in_normal),
	       V3ARGS(overlap_list[i].out_normal) );
    }
    else{
	bu_log("if_overlap: WARNING Skipping overlap region as maximum overlaps reached");
    }

    bu_log("if_overlap: Entering at (%f,%f,%f) at distance of %f",
	   V3ARGS(overlap_list[i].in_point), overlap_list[i].in_dist);
    bu_log("if_overlap: Exiting  at (%f,%f,%f) at distance of %f",
	   V3ARGS(overlap_list[i].out_point), overlap_list[i].out_dist);


    return rt_defoverlap (ap, pp, reg1, reg2, InputHdp);
}


int
shoot_ray(struct rt_i *rtip, point_t pt, point_t dir)
{
    struct application ap;

    /* Initialize the table of resource structures */
    /* rt_init_resource(&res_tab, 0, rtip); */

    /* initialization of the application structure */
    RT_APPLICATION_INIT(&ap);
    ap.a_hit = if_hit;        /* branch to if_hit routine */
    ap.a_miss = if_miss;      /* branch to if_miss routine */
    ap.a_overlap = if_overlap;/* branch to if_overlap routine */
    /*ap.a_logoverlap = rt_silent_logoverlap;*/
    ap.a_onehit = 0;          /* continue through shotline after hit */
    ap.a_purpose = "Manifold ray";
    ap.a_rt_i = rtip;         /* rt_i pointer */
    ap.a_zero1 = 0;           /* sanity check, sayth raytrace.h */
    ap.a_zero2 = 0;           /* sanity check, sayth raytrace.h */

    /* Set the ray start point and direction rt_shootray() uses these
     * two to determine what ray to fire.
     * It's worth nothing that librt assumes units of millimeters.
     * All geometry is stored as millimeters regardless of the units
     * set during editing.  There are libbu routines for performing
     * unit conversions if desired.
     */
    VMOVE(ap.a_ray.r_pt, pt);
    VMOVE(ap.a_ray.r_dir, dir);

    /* Simple debug printing */
    VPRINT("Pnt", ap.a_ray.r_pt);
    VPRINT("Dir", ap.a_ray.r_dir);

    /* Shoot the ray. */
    (void)rt_shootray(&ap);

    return GED_OK;
}


int
init_raytrace(struct simulation_params *sim_params)
{
    struct rigid_body *rb;

    /* Add all sim objects to raytrace instance */

    /* Add all the sim objects to the rt_i */
    for (rb = sim_params->head_node; rb != NULL; rb = rb->next) {
	if (rt_gettree(sim_params->rtip, rb->rb_namep) < 0)
	    bu_log("generate_manifolds: Failed to load geometry for [%s]\n",
		   rb->rb_namep);
	else
	    bu_log("generate_manifolds: Added [%s] to raytracer\n", rb->rb_namep);
    }

    /* This next call causes some values to be pre-computed, sets up space
     * partitioning, computes bounding volumes, etc.
     */
    rt_prep_parallel(sim_params->rtip, 1);

    bu_log("Simulation objects bounding box (%f, %f, %f):(%f,%f,%f)",
	   V3ARGS(sim_params->rtip->mdl_min), V3ARGS(sim_params->rtip->mdl_max));

    num_hits = 0;
    num_overlaps = 0;

    return GED_OK;
}


int
init_rayshot_results(void)
{
    /* Initialize the result structure */
    rt_result.xr_min_x[X]  = MAX_FASTF;
    rt_result.xr_max_x[X]  = -MAX_FASTF;
    rt_result.xr_min_y_in[Y] = MAX_FASTF;
    rt_result.xr_max_y_in[Y] = -MAX_FASTF;

    VSETALL(rt_result.force, SMALL_FASTF);
    rt_result.num_normals = 0;


    return GED_OK;
}


void
clear_bad_chars(struct bu_vls *vp)
{
    unsigned int i = 0;
    for(i = vp->vls_offset; i<(vp->vls_offset + vp->vls_len); i++){
	if( vp->vls_str[i] == '-' ||
	    vp->vls_str[i] == '/')
	    vp->vls_str[i] = 'R';

    }
}


int
traverse_xray_lists(struct simulation_params *sim_params,
		    point_t pt, point_t dir)
{
    int i;
    vect_t a;

    /*struct hit_reg *hrp;*/
    struct bu_vls reg_vls = BU_VLS_INIT_ZERO;

    /* quellage */
    bu_log("traverse_lists : From : (%f,%f,%f), towards(%f,%f,%f)", V3ARGS(pt),  V3ARGS(dir));

    /* Draw all the overlap regions : lines are added for overlap segments
     * to help visual debugging
     */
    for(i=0; i<num_overlaps; i++){

	bu_vls_sprintf(&reg_vls, "ray_overlap_%s_%s_%d_%f_%f_%f_%f_%f_%f",
		       overlap_list[i].reg1->reg_name,
		       overlap_list[i].reg2->reg_name,
		       overlap_list[i].index,
		       V3ARGS(pt), V3ARGS(dir));

	clear_bad_chars(&reg_vls);

	line(sim_params->gedp, bu_vls_addr(&reg_vls),
	     overlap_list[i].in_point,
	     overlap_list[i].out_point,
	     0, 210, 0);

	bu_log("traverse_xray_lists: %s", bu_vls_addr(&reg_vls));

	add_to_comb(sim_params->gedp, sim_params->sim_comb_name, bu_vls_addr(&reg_vls));


	/* Fill up the result structure */

	/* The min x in the direction of the ray where it hit an overlap */
	if(overlap_list[i].in_point[X] < rt_result.xr_min_x[X]){
	    VMOVE(rt_result.xr_min_x, overlap_list[i].in_point);
	}

	/* The max x in the direction of the ray where it hit an overlap,
	 * could be on a different ray from above
	 */
	if((overlap_list[i].out_point[X] > rt_result.xr_max_x[X])){
	    VMOVE(rt_result.xr_max_x, overlap_list[i].out_point);
	}

	/* The min y where the x rays encountered overlap */
	if(overlap_list[i].in_point[Y] < rt_result.xr_min_y_in[Y]){
	    VMOVE(rt_result.xr_min_y_in, overlap_list[i].in_point);
	    VMOVE(rt_result.xr_min_y_out, overlap_list[i].out_point);
	}

	/* The max y for the same */
	if(overlap_list[i].in_point[Y] > rt_result.xr_max_y_in[Y]){
	    VMOVE(rt_result.xr_max_y_in, overlap_list[i].in_point);
	    VMOVE(rt_result.xr_max_y_out, overlap_list[i].out_point);
	}

	/* The min z where the x rays encountered overlap */
	if(overlap_list[i].in_point[Y] < rt_result.xr_min_z_in[Z]){
	    /* Not need currently, may be removed when z-rays are shot */
	}

	/* The max z for the same */
	if(overlap_list[i].in_point[Y] > rt_result.xr_max_z_in[Z]){
	    /* Not need currently, may be removed when z-rays are shot */
	}

	/* Add force from entry normal */
	if(!exists_normal(overlap_list[i].in_normal)){
	    VREVERSE(a, overlap_list[i].in_normal);
	    VADD2(rt_result.force, rt_result.force, a);
	}

	/* Add force from exit normal */
	if(!exists_normal(overlap_list[i].out_normal)){
	    VREVERSE(a, overlap_list[i].out_normal);
	    VADD2(rt_result.force, rt_result.force, a);
	}

	bu_log("traverse_lists : force = %f,%f,%f",
	       V3ARGS(rt_result.force));

    }

    /* Draw all the hit regions : not really needed to be visualized */
    /*	if (hit_list.forw != &hit_list) {

	hrp = hit_list.forw;

	while (hrp != &hit_list) {
	bu_vls_sprintf(&reg_vls, "ray_hit_%s_%d_%f_%f_%f_%f_%f_%f",
	hrp->reg_name,
	hrp->index,
	V3ARGS(pt), V3ARGS(dir));
	line(gedp, bu_vls_addr(&reg_vls),
	hrp->in_point,
	hrp->out_point,
	0, 210, 0);

	add_to_comb(gedp, sim_params->sim_comb_name, bu_vls_addr(&reg_vls));
	hrp = hrp->forw;
	}
	}*/

    bu_vls_free(&reg_vls);

    return GED_OK;
}


int
shoot_x_rays(struct sim_manifold *current_manifold,
	     struct simulation_params *sim_params,
	     vect_t overlap_min,
	     vect_t overlap_max)
{
    point_t r_pt, r_dir;
    fastf_t startz, starty, y, z;
    vect_t diff;

    /* Set direction as straight down X-axis */
    VSET(r_dir, 1.0, 0.0, 0.0);


    bu_log("Querying overlap between %s & %s",
	   current_manifold->rbA->rb_namep,
	   current_manifold->rbB->rb_namep);

    /* Determine the width along z axis */
    VSUB2(diff, overlap_max, overlap_min);

    startz = overlap_min[Z] - TOL;

    /* If it's thinner than TOLerance, shoot rays only in a single plane in
     * the middle of the overlap region, this is done by starting in the middle
     */
    if(diff[Z] < TOL){
		startz += diff[Z]*0.5;
    }

	/* Shoot rays vertically and across the yz plane */
	for(z=startz; z<overlap_max[Z]; ){

		z += TOL;
		if(z > overlap_max[Z])
			z = overlap_max[Z];

		starty = overlap_min[Y] - TOL;

		for(y=starty; y<overlap_max[Y]; ){

			y += TOL;
			if(y > overlap_max[Y])
				y = overlap_max[Y];

			/* Shooting towards lower x, so start from max x outside of overlap box */
			VSET(r_pt, overlap_min[X], y, z);

			bu_log("*****shoot_x_rays : From : (%f,%f,%f) >>> towards(%f,%f,%f)*******",
			   V3ARGS(r_pt),  V3ARGS(r_dir));

			shoot_ray(sim_params->rtip, r_pt, r_dir);

			/* Traverse the hit list and overlap list, drawing the ray segments
			 * for the current ray
			 */
			traverse_xray_lists(sim_params, r_pt, r_dir);

			/* print_rayshot_results(); */

			/* Cleanup the overlap and hit lists and free memory */
			cleanup_lists();

			bu_log("Last y ray fired from y = %f, overlap_max[Y]=%f", y, overlap_max[Y]);

		}

		bu_log("Last z ray fired from z = %f, overlap_max[Z]=%f", z, overlap_max[Z]);

    }


    return GED_OK;
}


int
create_contact_pairs(struct sim_manifold *mf, vect_t overlap_min, vect_t overlap_max)
{
    /* Prepare the overlap prim name */
    bu_log("create_contact_pairs : between %s & %s (%f,%f,%f), (%f,%f,%f)",
	   mf->rbA->rb_namep, mf->rbB->rb_namep,
	   V3ARGS(overlap_min), V3ARGS(overlap_max));


    bu_log("create_contact_pairs : force = %f,%f,%f", V3ARGS(rt_result.force));
    VUNITIZE(rt_result.force);
    bu_log("create_contact_pairs : force = %f,%f,%f", V3ARGS(rt_result.force));

    return GED_OK;
}


int
generate_forces(struct simulation_params *sim_params,
		struct rigid_body *rbA,
		struct rigid_body *rbB)
{
    /* The manifold between rbA & rbB will be stored in B */
    struct sim_manifold *rt_mf = &(rbB->rt_manifold);

    vect_t overlap_min, overlap_max;
    char *prefix_overlap = "overlap_";
    struct bu_vls overlap_name = BU_VLS_INIT_ZERO;

    /* Setup the manifold participant pointers */
    rt_mf->rbA = rbA;
    rt_mf->rbB = rbB;

    /* Get the overlap region */
    get_overlap(rbA, rbB, overlap_min, overlap_max);

    /* Prepare the overlap prim name */
    bu_vls_sprintf(&overlap_name, "%s%s_%s",
		   prefix_overlap,
		   rbA->rb_namep,
		   rbB->rb_namep);

    /* Make the overlap volume RPP */
    make_rpp(sim_params->gedp, overlap_min, overlap_max, bu_vls_addr(&overlap_name));

    /* Add the region to the result of the sim so it will be drawn too */
    add_to_comb(sim_params->gedp, sim_params->sim_comb_name, bu_vls_addr(&overlap_name));

    /* Initialize the rayshot results structure, has to be done for each manifold  */
    init_rayshot_results();


    /* Shoot rays right here as the pair of rigid_body ptrs are known,
     * TODO: ignore volumes already shot
     */
    shoot_x_rays(rt_mf, sim_params, overlap_min, overlap_max);


    /* shoot_y_rays(); */


    /* shoot_z_rays(); */


    /* Create the contact pairs and normals */
    create_contact_pairs(rt_mf, overlap_min, overlap_max);


    return GED_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */