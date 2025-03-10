/*                      N M G _ F C U T . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2025 United States Government as represented by
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
/** @addtogroup nmg */
/** @{ */
/** @file primitives/nmg/nmg_fcut.c
 *
 * After two faces have been intersected, cut or join loops crossed
 * by the line of intersection.  (Formerly nmg_comb.c)
 *
 * The main external routine here is nmg_face_cutjoin().
 *
 * The line of intersection ("ray") will divide the face into two sets
 * of loops.
 * No one loop may cross the ray after this routine is finished.
 * (Current optimization may remove this property).
 *
 * Intersection points of significance to the other face but not yet
 * part of the current face's geometry are denoted by a vu on the ray
 * list, which points to a loop of a single vertex.  These points
 * need to be incorporated into the final face.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "bu/malloc.h"
#include "bu/sort.h"
#include "bg/plane.h"
#include "bv/plot3.h"
#include "nmg.h"

#define PLOT_BOTH_FACES 1

#define VAVERAGE(a, b, c) { \
	(a)[X] = ((b)[X] + (c)[X]) * 0.5;\
	(a)[Y] = ((b)[Y] + (c)[Y]) * 0.5;\
	(a)[Z] = ((b)[Z] + (c)[Z]) * 0.5;\
    }

/* States of the state machine */
#define NMG_STATE_ERROR		0
#define NMG_STATE_OUT		1
#define NMG_STATE_ON_L		2
#define NMG_STATE_ON_R		3
#define NMG_STATE_ON_B		4
#define NMG_STATE_ON_N		5
#define NMG_STATE_IN		6
#if 0
static const char *nmg_state_names[] = {
    "*ERROR*",
    "out",
    "on_L",
    "on_R",
    "on_both",		/* "hole" crack */
    "on_neither",		/* "non-hole" crack */
    "in",
    "TOOBIG"
};


#define NMG_E_ASSESSMENT_LEFT		0
#define NMG_E_ASSESSMENT_RIGHT		1
#define NMG_E_ASSESSMENT_ON_FORW	2
#define NMG_E_ASSESSMENT_ON_REV		3
#define NMG_E_ASSESSMENT_ERROR		4	/* risky */

#define NMG_V_ASSESSMENT_LONE		16
#define NMG_V_ASSESSMENT_ERROR		17
#define NMG_V_COMB(_p, _n)	(((_p)<<2)|(_n))

/* Extract previous and next assessments from combined version */
#define NMG_V_ASSESSMENT_PREV(_a)	(((_a)>>2)&3)
#define NMG_V_ASSESSMENT_NEXT(_a)	((_a)&3)

static const char *nmg_v_assessment_names[32] = {
    "Left, Left",
    "Left, Right",
    "Left, On_Forw",
    "Left, On_Rev",
    "Right, Left",
    "Right, Right",
    "Right, On_Forw",
    "Right, On_Rev",
    "On_Forw, Left",
    "On_Forw, Right",
    "On_Forw, On_Forw",
    "On_Forw, On_Rev",
    "On_Rev, Left",
    "On_Rev, Right",
    "On_Rev, On_Forw",
    "On_Rev, On_Rev",
    "LONE_V",		/* 16 */
    "V_ERROR",		/* 17 */
    "?18",
    "?19",
    "?20",
    "?21",
    "?22",
    "?23",
    "?24",
    "?25",
    "?26",
    "?27",
    "?28",
    "?29",
    "?30",
    "?31"
};
#define NMG_LEFT_LEFT NMG_V_COMB(NMG_E_ASSESSMENT_LEFT, NMG_E_ASSESSMENT_LEFT)
#define NMG_LEFT_RIGHT NMG_V_COMB(NMG_E_ASSESSMENT_LEFT, NMG_E_ASSESSMENT_RIGHT)
#define NMG_LEFT_ON_FORW NMG_V_COMB(NMG_E_ASSESSMENT_LEFT, NMG_E_ASSESSMENT_ON_FORW)
#define NMG_LEFT_ON_REV NMG_V_COMB(NMG_E_ASSESSMENT_LEFT, NMG_E_ASSESSMENT_ON_REV)
#define NMG_RIGHT_LEFT NMG_V_COMB(NMG_E_ASSESSMENT_RIGHT, NMG_E_ASSESSMENT_LEFT)
#define NMG_RIGHT_RIGHT NMG_V_COMB(NMG_E_ASSESSMENT_RIGHT, NMG_E_ASSESSMENT_RIGHT)
#define NMG_RIGHT_ON_FORW NMG_V_COMB(NMG_E_ASSESSMENT_RIGHT, NMG_E_ASSESSMENT_ON_FORW)
#define NMG_RIGHT_ON_REV NMG_V_COMB(NMG_E_ASSESSMENT_RIGHT, NMG_E_ASSESSMENT_ON_REV)
#define NMG_ON_FORW_LEFT NMG_V_COMB(NMG_E_ASSESSMENT_ON_FORW, NMG_E_ASSESSMENT_LEFT)
#define NMG_ON_FORW_RIGHT NMG_V_COMB(NMG_E_ASSESSMENT_ON_FORW, NMG_E_ASSESSMENT_RIGHT)
#define NMG_ON_FORW_ON_FORW NMG_V_COMB(NMG_E_ASSESSMENT_ON_FORW, NMG_E_ASSESSMENT_ON_FORW)
#define NMG_ON_FORW_ON_REV NMG_V_COMB(NMG_E_ASSESSMENT_ON_FORW, NMG_E_ASSESSMENT_ON_REV)
#define NMG_ON_REV_LEFT NMG_V_COMB(NMG_E_ASSESSMENT_ON_REV, NMG_E_ASSESSMENT_LEFT)
#define NMG_ON_REV_RIGHT NMG_V_COMB(NMG_E_ASSESSMENT_ON_REV, NMG_E_ASSESSMENT_RIGHT)
#define NMG_ON_REV_ON_FORW NMG_V_COMB(NMG_E_ASSESSMENT_ON_REV, NMG_E_ASSESSMENT_ON_FORW)
#define NMG_ON_REV_ON_REV NMG_V_COMB(NMG_E_ASSESSMENT_ON_REV, NMG_E_ASSESSMENT_ON_REV)
#define NMG_LONE NMG_V_ASSESSMENT_LONE

static const char *nmg_e_assessment_names[6] = {
    "LEFT",
    "RIGHT",
    "ON_FORW",
    "ON_REV",
    "E_ERROR",
    "E_5?"
};


/*
 * Action entries for the state transition tables
 */
#define NMG_ACTION_ERROR		0
#define NMG_ACTION_NONE			1
#define NMG_ACTION_NONE_OPTIM		2
#define NMG_ACTION_VFY_EXT		3
#define NMG_ACTION_VFY_MULTI		4
#define NMG_ACTION_LONE_V_ESPLIT	5
#define NMG_ACTION_LONE_V_JAUNT		6
#define NMG_ACTION_CUTJOIN		7
static const char *action_names[] = {
    "*ERROR*",
    "NONE",
    "NONE_OPTIM",
    "VFY_EXT",
    "VFY_MULTI",
    "LONE_V_ESPLIT",
    "LONE_V_JAUNT",
    "CUTJOIN",
    "*TOOBIG*"
};
#endif

/* The "ray" here is the intersection line between two faces */
struct nmg_ray_state {
    uint32_t magic;
    struct vertexuse **vu;		/* ptr to vu array */
    int nvu;		/* len of vu[] */
    point_t pt;		/* The ray */
    vect_t dir;
    struct edge_g_lseg *eg_p;		/* Edge geom of the ray */
    struct shell *sA;
    struct shell *sB;
    struct faceuse *fu1;
    struct faceuse *fu2;
    vect_t left;		/* points left of ray, on face */
    int state;		/* current (old) state */
    int last_action;	/* last action taken */
    vect_t ang_x_dir;	/* x axis for angle measure */
    vect_t ang_y_dir;	/* y axis for angle measure */
    const struct bn_tol *tol;
};
#define NMG_RAYSTATE_MAGIC 0x54322345
#define NMG_CK_RAYSTATE(_p) NMG_CKMAG(_p, NMG_RAYSTATE_MAGIC, "nmg_ray_state")

struct loop_cuts {
    struct loopuse *lu;
    struct vertexuse *vu1;
    struct vertexuse *vu2;
};

#if 0
static int
nmg_face_state_transition(struct nmg_ray_state *rs,
			  int pos,
			  int multi,
			  int other_rs_state,
			  struct bu_list *vlfree);
#endif

/**
 * Sort list of hit points (vertexuse's) in fu1 (bu_ptbl 'b') on plane
 * of fu2 (defined by pt+dir), by increasing distance, vertex ptr, and
 * vu ptr.  Eliminate duplications of vu at same distance.  (Actually,
 * a given vu should show up at exactly 1 distance!)  The line of
 * intersection is pt + t * dir.
 *
 * For now, a bubble-sort is used, because the list should not have more
 * than a few hundred entries on it.
 */
static void
ptbl_vsort(struct bu_ptbl *b, fastf_t *pt, fastf_t *dir, fastf_t *mag, fastf_t dist_tol)
/* table of vertexuses on intercept line */
/* unused? */
/* unused? */


{
    register struct vertexuse **vu;
    register size_t i, j;

    vu = (struct vertexuse **)b->buffer;

    if (nmg_debug) {
	/* Ensure that distance from points to ray is reasonable */
	for (i = 0; i < b->end; ++i) {
	    fastf_t dist;

	    NMG_CK_VERTEXUSE(vu[i]);
	    dist = bg_dist_line3_pnt3(pt, dir, vu[i]->v_p->vg_p->coord);
	    if (dist > dist_tol) {
		bu_log("WARNING ptbl_vsort() vu=%p point off line by %e %g*tol, tol=%e\n",
		       (void *)vu[i], dist,
		       dist/dist_tol, dist_tol);
		if (nmg_debug&NMG_DEBUG_VU_SORT) {
		    VPRINT("  vu", vu[i]->v_p->vg_p->coord);
		    VPRINT("  pt", pt);
		    VPRINT(" dir", dir);
		}
	    }
	    if (dist > 100*dist_tol) {
		bu_log("ERROR ptbl_vsort() vu=%p point off line by %g > 100*dist_tol\n",
		       (void *)vu[i], dist);
		bu_bomb("ptbl_vsort()\n");
	    }
	}
    }

    /* check vertexuses and compute distance from start of line */
    for (i = 0; i < b->end; ++i) {
	vect_t vect;
	NMG_CK_VERTEXUSE(vu[i]);

	if (mag[i] - MAX_FASTF > -SMALL_FASTF) {
	    VSUB2(vect, vu[i]->v_p->vg_p->coord, pt);
	    mag[i] = VDOT(vect, dir);
	}

	/* Find previous vu's at "same" distance, within dist_tol */
	for (j = 0; j < i; j++) {
	    register fastf_t tmag;

	    tmag = mag[i] - mag[j];
	    if (tmag < -dist_tol) continue;
	    if (tmag > dist_tol) continue;
	    /* Nearly equal at same vertex */
	    if (!ZERO(mag[i] - mag[j])
		&& vu[i]->v_p == vu[j]->v_p)
	    {
		bu_log("ptbl_vsort: forcing vu=%p & vu=%p mag equal\n", (void *)vu[i], (void *)vu[j]);
		mag[j] = mag[i]; /* force equal */
	    }
	}
    }

    for (i = 0; i < b->end - 1; ++i) {
	for (j = i+1; j < b->end; ++j) {

	    if (mag[i] < mag[j]) continue;
	    if (ZERO(mag[i] - mag[j])) {
		if (vu[i]->v_p < vu[j]->v_p) continue;
		if (vu[i]->v_p == vu[j]->v_p) {
		    if (vu[i] < vu[j]) continue;
		    if (vu[i] == vu[j]) {
			ssize_t last = b->end - 1;
			/* vu duplication, eliminate! */
			bu_log("ptbl_vsort: vu duplication eliminated\n");
			if ((ssize_t)j >= last) {
			    /* j is last element */
			    b->end--;
			    break;
			}
			/* rewrite j with last element */
			vu[j] = vu[last];
			mag[j] = mag[last];
			b->end--;
			/* Repeat this index */
			j--;
			continue;
		    }
		    /* vu[i] > vu[j], fall through */
		}
		/* vu[i]->v_p > vu[j]->v_p, fall through */
	    }
	    /* mag[i] > mag[j] */

	    /* exchange [i] and [j] */
	    {
		register struct vertexuse *tvu;
		tvu = vu[i];
		vu[i] = vu[j];
		vu[j] = tvu;
	    }

	    {
		register fastf_t tmag;
		tmag = mag[i];
		mag[i] = mag[j];
		mag[j] = tmag;
	    }
	}
    }
}


/**
 * As an automatic check for the intersector failing to find
 * all intersections, check all the vertices on the intersection line.
 * For each one, find all the other uses in this faceuse, and
 * if they are not also listed on the line, they were overlooked.
 *
 * This does not catch _all_ possible mistakes, but does catch some.
 */
int
nmg_ck_vu_ptbl(struct bu_ptbl *p, struct faceuse *fu)
{
    struct vertex *v;
    struct vertexuse *vu;
    struct vertexuse *tvu;
    struct faceuse *tfu;
    size_t i;
    int ret = 0;

    BU_CK_PTBL(p);
    NMG_CK_FACEUSE(fu);

top:
    for (i = 0; i < BU_PTBL_LEN(p); i++) {
	vu = (struct vertexuse *)BU_PTBL_GET(p, i);
	NMG_CK_VERTEXUSE(vu);
	v = vu->v_p;
	NMG_CK_VERTEX(v);
	tfu = nmg_find_fu_of_vu(vu);
	if (tfu != fu) {
	    bu_log("ERROR: vu=%p v=%p up_fu=%p != arg_fu=%p\n",
		   (void *)vu, (void *)v, (void *)tfu, (void *)fu);
	    bu_bomb("nmg_ck_vu_ptbl() intersect list is confused about which face it belongs to.\n");
	}
	for (BU_LIST_FOR(tvu, vertexuse, &v->vu_hd)) {
	    NMG_CK_VERTEXUSE(tvu);
	    if (tvu == vu) continue;
	    if ((tfu = nmg_find_fu_of_vu(tvu)) == (struct faceuse *)NULL)
		continue;
	    if (tfu != fu) continue;
	    /* tvu is in fu.  Is tvu on the line? */
	    if (bu_ptbl_locate(p, (long *)&tvu->l.magic) >= 0) continue;
	    /* No, not on list */
	    bu_log("ERROR: vu=%p v=%p %s=%p is on isect line, tvu=%p %s=%p isn't.\n",
		   (void *)vu, (void *)v,
		   bu_identify_magic(*vu->up.magic_p),
		   (void *)vu->up.magic_p,
		   (void *)tvu,
		   bu_identify_magic(*tvu->up.magic_p),
		   (void *)tvu->up.magic_p);
	    /* XXX bomb here? */
	    nmg_pr_ptbl("intersect line vu table", p, 1);
	    bu_bomb("nmg_ck_vu_ptbl() missing vertexuse\n");
/* XXXXXXXXXXXXXXXXXXXXXXXX Horrible temporizing measure */
	    /* Another strategy:  add it in! */
	    (void)bu_ptbl_ins(p, (long *)&tvu->l.magic);
	    ret++;
	    goto top;
	}
    }
    if (ret)bu_log("nmg_ck_vu_ptbl() ret=%d\n", ret);
    return ret;
}

#if 0

/**
 * Given a vertexuse from a loop which lies in a plane,
 * compute the vector 'vec' from the previous vertex to this one.
 * Using two perpendicular vectors (x_dir and y_dir) which both lie
 * in the plane of the loop, return the angle (in radians) of 'vec'
 * from x_dir, going CCW around the perpendicular x_dir CROSS y_dir.
 *
 * x_dir is -ray_dir
 * y_dir points right.
 *
 * Returns -
 * vec == x_dir returns 0,
 * vec == y_dir returns pi/2,
 * vec == -x_dir returns pi,
 * vec == -y_dir returns 3*pi/2.
 * 0.0 if unable to compute 'vec'
 */
double
nmg_vu_angle_measure(struct vertexuse *vu, fastf_t *x_dir, fastf_t *y_dir, int assessment, int in)


/* 1 = inbound edge, 0 = outbound edge */
{
    struct loopuse *lu;
    struct edgeuse *this_eu;
    struct edgeuse *prev_eu;
    vect_t vec;
    fastf_t ang;
    int this_ass;

    NMG_CK_VERTEXUSE(vu);
    if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
	return 0;		/* Unable to compute 'vec' */
    }

    /*
     * For consistency, if entry edge is ON the ray,
     * force the angles to be exact, don't compute them.
     */
    if (in)
	this_ass = NMG_V_ASSESSMENT_PREV(assessment);
    else
	this_ass = NMG_V_ASSESSMENT_NEXT(assessment);
    if (this_ass == NMG_E_ASSESSMENT_ON_FORW) {
	if (in) ang = 0.0;	/* zero angle */
	else ang = M_PI;	/* 180 degrees */
	if (nmg_debug&NMG_DEBUG_VU_SORT)
	    bu_log("nmg_vu_angle_measure:  NMG_E_ASSESSMENT_ON_FORW, ang=%g\n", ang);
	return ang;
    }
    if (this_ass == NMG_E_ASSESSMENT_ON_REV) {
	if (in) ang = M_PI;	/* 180 degrees */
	else ang = 0.0;	/* zero angle */
	if (nmg_debug&NMG_DEBUG_VU_SORT)
	    bu_log("nmg_vu_angle_measure:  NMG_E_ASSESSMENT_ON_REV, ang=%g\n", ang);
	return ang;
    }

    /*
     * Compute the angle
     */
    lu = nmg_find_lu_of_vu(vu);
    NMG_CK_LOOPUSE(lu);
    this_eu = nmg_find_eu_with_vu_in_lu(lu, vu);
    prev_eu = this_eu;
    do {
	prev_eu = in ? BU_LIST_PPREV_CIRC(edgeuse, prev_eu) :
	    BU_LIST_PNEXT_CIRC(edgeuse, prev_eu);
	if (prev_eu == this_eu) {
	    if (nmg_debug&NMG_DEBUG_VU_SORT)
		bu_log("nmg_vu_angle_measure: prev eu is this eu, ang=0\n");
	    return 0;	/* Unable to compute 'vec' */
	}
	/* Skip any edges that stay on this vertex */
    } while (prev_eu->vu_p->v_p == this_eu->vu_p->v_p);

    /* in==1 Get vec for inbound edge, but pointing away from vert. */
    /* in==0 "prev" is really next, so this is departing vec */
    VSUB2(vec, prev_eu->vu_p->v_p->vg_p->coord, vu->v_p->vg_p->coord);

    ang = bg_angle_measure(vec, x_dir, y_dir);
    if (nmg_debug&NMG_DEBUG_VU_SORT)
	bu_log("nmg_vu_angle_measure:  measured angle=%e\n", ang*RAD2DEG);

    /*
     * Since the entry edge is not on the ray, ensure the
     * angles are not exactly 0 or pi.
     */
#define RADIAN_TWEEK 1.0e-14	/* low bits of double prec., re: 6.28... */
    if (ZERO(ang)) {
	if (this_ass == NMG_E_ASSESSMENT_RIGHT) {
	    ang = RADIAN_TWEEK;
	} else {
	    /* Assuming NMG_E_ASSESSMENT_LEFT */
	    ang = M_2PI - RADIAN_TWEEK;
	}
    } else if (ZERO(ang - M_PI)) {
	if (this_ass == NMG_E_ASSESSMENT_RIGHT) {
	    ang = M_PI - RADIAN_TWEEK;
	} else {
	    ang = M_PI + RADIAN_TWEEK;
	}
    }

    /*
     * Also, ensure computed angle and topological assessment agree
     * about which side of the ray this edge is on.
     */
    if (ang > M_PI) {
	if (this_ass != NMG_E_ASSESSMENT_LEFT) {
	    bu_log("*** ERROR topology/geometry conflict, ang=%e, ass=%s\n",
		   ang*RAD2DEG,
		   nmg_e_assessment_names[this_ass]);
	}
    } else if (ang < M_PI) {
	if (this_ass != NMG_E_ASSESSMENT_RIGHT) {
	    bu_log("*** ERROR topology/geometry conflict, ang=%e, ass=%s\n",
		   ang*RAD2DEG,
		   nmg_e_assessment_names[this_ass]);
	}
    }
    if (nmg_debug&NMG_DEBUG_VU_SORT)
	bu_log("  final ang=%g (%e), vec=(%g, %g, %g)\n", ang*RAD2DEG, ang*RAD2DEG, V3ARGS(vec));
    return ang;
}


static int
nmg_is_v_on_rs_list(const struct nmg_ray_state *rs, const struct vertex *v)
{
    register int i;

    NMG_CK_VERTEX(v);
    for (i=rs->nvu-1; i >= 0; i--) {
	if (!rs->vu[i]) continue;
	if (rs->vu[i]->v_p == v) return i;
    }
    return -1;
}


/**
 * The current vertex (eu->vu_p) is on the line of intersection.
 * Assess the indicated edge, to see if it lies on the line of
 * intersection, or departs towards the left or right.
 *
 * There is no need to look more than one edge forward or backward.
 * Even if there are edges which loop around to the same vertex
 * (with a different vertexuse), that (0-length) edge is ON the ray.
 */
static int
nmg_assess_eu(struct edgeuse *eu, int forw, struct nmg_ray_state *rs, int pos)
{
    struct vertex *v;
    struct vertex *otherv = (struct vertex *)0;
    struct edgeuse *othereu;
    vect_t heading;
    int ret;
    register int i;

    VSETALL(heading, 0);

    NMG_CK_EDGEUSE(eu);
    NMG_CK_RAYSTATE(rs);
    BN_CK_TOL(rs->tol);
    v = eu->vu_p->v_p;
    NMG_CK_VERTEX(v);
    othereu = eu;
    if (forw) {
	othereu = BU_LIST_PNEXT_CIRC(edgeuse, othereu);
    } else {
	othereu = BU_LIST_PPREV_CIRC(edgeuse, othereu);
    }
    if (othereu == eu) {
	/* Back to where search started */
	if (nmg_debug) nmg_pr_eu(eu, NULL);
	bu_bomb("nmg_assess_eu() no edges leave the vertex!\n");
    }
    otherv = othereu->vu_p->v_p;
    if (otherv == v) {
	/* Edge stays on this vertex -- can't tell if forw or rev! */
	if (nmg_debug) nmg_pr_eu(eu, NULL);
	bu_bomb("nmg_assess_eu() edge runs from&to same vertex!\n");
    }

    /* If the other vertex is mentioned anywhere on the ray's vu list,
     * then the edge is "on" the ray.
     * Match against vertex (rather than vertexuse) because cut/join
     * operations may have changed the particular vertexuse pointer.
     */
    if ((i = nmg_is_v_on_rs_list(rs, otherv)) > -1) {
	struct vertex *farv;
	struct edgeuse *fareu;
	int start, end;

	fareu = othereu;
    again:
	/* Edge's far end is ON the ray.  Which way does it go? */
	if (nmg_debug&NMG_DEBUG_FCUT)
	    bu_log("eu ON ray: vu[%d]=%p, other:vu[%d]=%p\n",
		   pos, (void *)rs->vu[pos], i, (void *)otherv);

	/*
	 * As an attempt at fixing the ON/ON vertexuse problem,
	 * look further forw/back along the edgeuse list,
	 * as long as it shares the same edge geometry.
	 * If not on list, use *that* vertex to assess left/right.
	 */
	if (forw) {
	    fareu = BU_LIST_PNEXT_CIRC(edgeuse, fareu);
	} else {
	    fareu = BU_LIST_PPREV_CIRC(edgeuse, fareu);
	}
	if (fareu == eu) goto really_on;	/* All eu's are ON! */
	if (fareu->g.lseg_p != eu->g.lseg_p) goto really_on;
	farv = fareu->vu_p->v_p;
	if (nmg_debug&NMG_DEBUG_FCUT)
	    bu_log("nmg_assess_eu() farv = %p, on_index=%d\n", (void *)farv, nmg_is_v_on_rs_list(rs, farv));
	if (nmg_is_v_on_rs_list(rs, farv) > -1) {
	    /* farv is ON list, try going further back */
	    goto again;
	}
	/* farv is not ON list, assess _it_ */
	/* XXX Need to remove othervu from the list! */
	otherv = farv;
	if (nmg_debug&NMG_DEBUG_FCUT)
	    bu_log("nmg_assess_eu() assessing farv\n");
	goto left_right;

	/* Compute edge vector, for purposes of orienting answer */
    really_on:
	/*
	 * There are 2 ways of determining the assessment:
	 * edge direction DOT ray direction, above, and
	 * comparing the subscripts of the two vertexuses
	 * (which translates to comparing dists along ray).
	 */
	if (i > pos) {
	    if (forw)
		ret = NMG_E_ASSESSMENT_ON_FORW;
	    else
		ret = NMG_E_ASSESSMENT_ON_REV;
	    start = pos+1;
	    end = i;
	} else {
	    /* i < pos  (They can't be equal) */
	    if (forw)
		ret = NMG_E_ASSESSMENT_ON_REV;
	    else
		ret = NMG_E_ASSESSMENT_ON_FORW;
	    start = i+1;
	    end = pos;
	}

	/*
	 * Verify that any other vertexuses on the intersect line
	 * along the ON edge are uses of one of the two edge
	 * endpoints.  Otherwise, something awful has happened.
	 */
	for (i=start; i<end; i++) {
	    int j;
	    if (!rs->vu[i]) continue;
	    if (rs->vu[i]->v_p == v ||
		rs->vu[i]->v_p == otherv)
		continue;
	    if (nmg_debug&NMG_DEBUG_FCUT) {
		bu_log("In edge interval (%d, %d), ON vertexuse [%d] = %p appears?\n",
		       start-1, end, i, (void *)rs->vu[i]);
		for (j=start-1; j<=end; j++) {
		    if (!rs->vu[i]) continue;
		    bu_log(" %d ", j);
		    nmg_pr_vu_briefly(rs->vu[j], (char *)0);
		}
		bu_log("nmg_assess_eu():  ON vertexuse in middle of edge?\n");
	    }
	    /* Leave it for nmg_onon_fix() to fix */
	    ret = NMG_E_ASSESSMENT_ERROR;
	    return -i;	/* Special flag to nmg_onon_fix() */
	}
	goto out;
    }

    /*
     * Since other vertex does not lie anywhere on line of intersection,
     * the edge must lie to one side or the other of the ray.
     * Check vector from v to otherv against "left" vector.
     */
left_right:
    VSUB2(heading, otherv->vg_p->coord, v->vg_p->coord);
    if (MAGSQ(heading) <= SMALL_FASTF) bu_bomb("nmg_assess_eu() null heading 2\n");
    if (VDOT(heading, rs->left) < 0) {
	ret = NMG_E_ASSESSMENT_RIGHT;
    } else {
	ret = NMG_E_ASSESSMENT_LEFT;
    }
out:
    if (nmg_debug&NMG_DEBUG_FCUT) {
	bu_log("nmg_assess_eu(%p, fw=%d, pos=%d) v=%p otherv=%p: %s\n",
	       (void *)eu, forw, pos, (void *)v, (void *)otherv,
	       nmg_e_assessment_names[ret]);
	bu_log(" v(%g, %g, %g) other(%g, %g, %g)\n",
	       V3ARGS(v->vg_p->coord), V3ARGS(otherv->vg_p->coord));
	bu_log(" rs->left(%g, %g, %g) heading(%g, %g, %g)\n",
	       V3ARGS(rs->left), V3ARGS(heading));
    }
    return ret;
}


static int
nmg_assess_vu(struct nmg_ray_state *rs, int pos)
{
    struct vertexuse *vu;
    struct loopuse *lu;
    struct edgeuse *this_eu;
    int next_ass;
    int prev_ass;
    int ass;

    NMG_CK_RAYSTATE(rs);
    vu = rs->vu[pos];
    NMG_CK_VERTEXUSE(vu);
    if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
	return NMG_V_ASSESSMENT_LONE;
    }
    if ((lu = nmg_find_lu_of_vu(vu)) == (struct loopuse *)0)
	bu_bomb("nmg_assess_vu: no lu\n");
    this_eu = nmg_find_eu_with_vu_in_lu(lu, vu);
    /* Couldn't this have been this_eu = vu->up.eu_p ? */
    if (this_eu != vu->up.eu_p)
	bu_log("nmg_assess_vu() eu mismatch? %p %p\n", (void *)this_eu, (void *)vu->up.eu_p);
    prev_ass = nmg_assess_eu(this_eu, 0, rs, pos);
    next_ass = nmg_assess_eu(this_eu, 1, rs, pos);
    if (prev_ass < 0 || next_ass < 0) {
	ass = NMG_V_ASSESSMENT_ERROR;
    } else {
	ass = NMG_V_COMB(prev_ass, next_ass);
    }

    /*
     * If the vu assessment is
     * NMG_ON_REV_ON_FORW or NMG_ON_FORW_ON_REV,
     * ensure that other end of both eu's is same vertex.
     * If not, it's an intersector error, and will confuse our caller.
     */
    if (ass==NMG_ON_REV_ON_FORW || ass==NMG_ON_FORW_ON_REV) {
	struct edgeuse *prev;
	struct edgeuse *next;
	prev = BU_LIST_PPREV_CIRC(edgeuse, this_eu);
	next = BU_LIST_PNEXT_CIRC(edgeuse, this_eu);
	if (prev->vu_p->v_p != next->vu_p->v_p) {
	    bu_log("nmg_assess_vu() %s, v=%p, prev_v=%p, next_v=%p\n",
		   nmg_v_assessment_names[ass],
		   (void *)this_eu->vu_p->v_p,
		   (void *)prev->vu_p->v_p, (void *)next->vu_p->v_p);
	    bu_log("nmg_assess_vu() ON/ON edgeuse ends on different vertices.\n");
	    VPRINT("vu  ", this_eu->vu_p->v_p->vg_p->coord);
	    VPRINT("prev", prev->vu_p->v_p->vg_p->coord);
	    VPRINT("next", next->vu_p->v_p->vg_p->coord);

	    /* See how far off the line they are */
	    bu_log("vu dist=%e, next dist=%e, tol=%e\n",
		   bg_dist_line3_pnt3(rs->pt, rs->dir, this_eu->vu_p->v_p->vg_p->coord),
		   bg_dist_line3_pnt3(rs->pt, rs->dir, prev->vu_p->v_p->vg_p->coord),
		   rs->tol->dist);
	    bu_bomb("nmg_assess_vu() ON/ON edgeuse ends on different vertices.\n");
	}
    }
    if (nmg_debug&NMG_DEBUG_FCUT) {
	bu_log("nmg_assess_vu() vu[%d]=%p, v=%p: %s\n",
	       pos, (void *)vu, (void *)vu->v_p, nmg_v_assessment_names[ass]);
    }
    return ass;
}


struct nmg_vu_stuff {
    struct vertexuse *vu;
    int loop_index;
    struct nmg_loop_stuff *lsp;
    fastf_t in_vu_angle;
    fastf_t out_vu_angle;
    fastf_t min_vu_dot;
    fastf_t lo_ang;		/* small if RIGHT, large if LEFT */
    fastf_t hi_ang;
    int seq;		/* seq # after lsp->min_vu */
    int wedge_class;	/* WEDGE_LEFT, etc. */
};
struct nmg_loop_stuff {
    struct loopuse *lu;
    fastf_t min_dot;
    struct vertexuse *min_vu;
    int n_vu_in_loop;	/* # ray vu's in this loop */
};


#define WEDGE_LEFT 0
#define WEDGE_CROSS 1
#define WEDGE_RIGHT 2
#define WEDGE_ON 3
#define WEDGECLASS2STR(_cl) nmg_wedgeclass_string[(_cl)]
static const char *nmg_wedgeclass_string[] = {
    "LEFT",
    "CROSS",
    "RIGHT",
    "ON",
    "???"
};


static void
nmg_pr_vu_stuff(const struct nmg_vu_stuff *vs)
{
    bu_log("nmg_pr_vu_stuff(%p) vu=%p, loop_index=%d, lsp=%p\n",
	   (void *)vs, (void *)vs->vu, vs->loop_index, (void *)vs->lsp);
    bu_log(" in_vu_angle=%g, out_vu_angle=%g, min_vu_dot=%g\n",
	   vs->in_vu_angle, vs->out_vu_angle, vs->min_vu_dot);
    bu_log(" lo_ang=%g, hi_ang=%g, seq=%d, wedge_class=%s\n",
	   vs->lo_ang, vs->hi_ang, vs->seq,
	   WEDGECLASS2STR(vs->wedge_class));
}

int
nmg_wedge_class(int ass, double a, double b)
/* assessment of two edges forming wedge */


{
    double ha, hb;
    register int ret;

    ha = a - 180;
    hb = b - 180;

    if (ass == NMG_V_COMB(NMG_E_ASSESSMENT_ON_FORW, NMG_E_ASSESSMENT_ON_FORW)) {
	ret = WEDGE_ON;
	goto out;
    }
    if (ass == NMG_V_COMB(NMG_E_ASSESSMENT_ON_REV, NMG_E_ASSESSMENT_ON_REV)) {
	ret = WEDGE_ON;
	goto out;
    }

    if (NEAR_ZERO(ha, .01)) {
	/* A is on the ray, within tol */
	if (NEAR_ZERO(hb, .01)) {
	    /* B is on the ray, within tol */
	    /* This is a 0-angle wedge entering & leaving.
	     * This is not WEDGE_ON
	     * Call it WEDGE_CROSS.
	     */
	    if (nmg_debug&NMG_DEBUG_VU_SORT)
		bu_log("nmg_wedge_class() 0-angle wedge\n");
	    ret = WEDGE_CROSS;
	    goto out;
	}
	if (hb < 0) {
	    ret = WEDGE_RIGHT;
	    goto out;
	}
	ret = WEDGE_LEFT;
	goto out;
    }
    if (ha < 0) {
	/* A is to the right */
	if (hb <= 0) {
	    ret = WEDGE_RIGHT;
	    goto out;
	}
	ret = WEDGE_CROSS;
	goto out;
    }
    /* ha is > 0, A is to the left */
    if (NEAR_ZERO(hb, .01)) {
	/* A is left, B is ON_FORW (180) */
	ret = WEDGE_LEFT;
	goto out;
    }
    if (hb >= 0) {
	/* A is left, B is LEFT */
	ret = WEDGE_LEFT;
	goto out;
    }
    /* A is left, B is RIGHT */
    ret = WEDGE_CROSS;
out:
    if (nmg_debug&NMG_DEBUG_VU_SORT) {
	bu_log("nmg_wedge_class(%g, %g) = %s\n",
	       a, b, WEDGECLASS2STR(ret));
    }
    return ret;
}


static const char *nmg_wedge2_string[] = {
    "WEDGE2_OVERLAP",
    "WEDGE2_NO_OVERLAP",
    "WEDGE2_AB_IN_CD",
    "WEDGE2_CD_IN_AB",
    "WEDGE2_IDENTICAL",
    "WEDGE2_TOUCH_AT_BC",
    "WEDGE2_TOUCH_AT_DA",
    "WEDGE2_???"
};
#define WEDGE2_TO_STRING(_class2)	(nmg_wedge2_string[(_class2)+2])

#define WEDGE2_OVERLAP		-2
#define WEDGE2_NO_OVERLAP	-1
#define WEDGE2_AB_IN_CD		0
#define WEDGE2_CD_IN_AB		1
#define WEDGE2_IDENTICAL	2
#define WEDGE2_TOUCH_AT_BC	3
#define WEDGE2_TOUCH_AT_DA	4

/**
 * Returns -
 * WEDGE2_OVERLAP AB partially overlaps CD (error)
 * WEDGE2_NO_OVERLAP AB does not overlap CD
 * WEDGE2_AB_IN_CD AB is inside CD
 * WEDGE2_CD_IN_AB CD is inside AB
 * WEDGE2_IDENTICAL AB == CD
 * WEDGE2_TOUCH_AT_BC AB touches CD at BC, but does not overlap
 * WEDGE2_TOUCH_AT_DA CD touches AB at DA, but does not overlap
 */
static int
nmg_compare_2_wedges(double a, double b, double c, double d)
{
    double t;
    int a_in_cd = 0;
    int b_in_cd = 0;
    int c_in_ab = 0;
    int d_in_ab = 0;
    int a_eq_b = 0;
    int a_eq_c = 0;
    int a_eq_d = 0;
    int b_eq_c = 0;
    int b_eq_d = 0;
    int c_eq_d = 0;
    int ret;

#define WEDGE_ANG_TOL 0.001	/* XXX Angular tolerance, in degrees */
#define ANG_SMASH(_a) {\
	if (_a <= WEDGE_ANG_TOL)  _a = 0; \
	else if (NEAR_EQUAL(_a, 180, WEDGE_ANG_TOL))  _a = 180; \
	else if (_a >= 360 - WEDGE_ANG_TOL)  _a = 360; }

    ANG_SMASH(a);
    ANG_SMASH(b);
    ANG_SMASH(c);
    ANG_SMASH(d);

    /* Ensure A < B */
    if (a > b) {
	t = a;
	a = b;
	b = t;
    }
    /* Ensure that C < D */
    if (c > d) {
	t = c;
	c = d;
	d = t;
    }

    if (NEAR_EQUAL(a, b, WEDGE_ANG_TOL)) a_eq_b = 1;
    if (NEAR_EQUAL(a, c, WEDGE_ANG_TOL)) a_eq_c = 1;
    if (NEAR_EQUAL(a, d, WEDGE_ANG_TOL)) a_eq_d = 1;
    if (NEAR_EQUAL(b, c, WEDGE_ANG_TOL)) b_eq_c = 1;
    if (NEAR_EQUAL(b, d, WEDGE_ANG_TOL)) b_eq_d = 1;
    if (NEAR_EQUAL(c, d, WEDGE_ANG_TOL)) c_eq_d = 1;

    /*
     * Test for TOUCHing wedges must come before INside test,
     * so that zero-angle wedges that touch a non-zero angle wedge,
     * will be properly recognized.  e.g. AB=(0, 0) CD=(0, 180).
     */
    if (b_eq_c) {
	/* Wedges touch along B-C junction */
	if (a_eq_d)
	    ret = WEDGE2_IDENTICAL;	/* two zero-angle wedges */
	else
	    ret = WEDGE2_TOUCH_AT_BC;
	goto out;
    }

    if (a_eq_d) {
	/* We know c <= d, d==a, a <= b */
	if (a_eq_b) {
	    ret = WEDGE2_AB_IN_CD;
	} else if (c_eq_d) {
	    ret = WEDGE2_CD_IN_AB;
	} else {
	    ret = WEDGE2_TOUCH_AT_DA;
	}
	goto out;
    }

    if (a_eq_c) {
	if (b_eq_d) {
	    ret = WEDGE2_IDENTICAL;
	    goto out;
	}
	/* We already know that A <= B, from sort above */
	if (b < d) ret = WEDGE2_AB_IN_CD;
	else ret = WEDGE2_CD_IN_AB;
	goto out;
    }

    if (b_eq_d) {
	/* a != c, because of previous IF statement */
	if (a < c) ret = WEDGE2_CD_IN_AB;
	else ret = WEDGE2_AB_IN_CD;
	goto out;
    }

    /* See if c < a, b < d */
    if (c <= a && a <= d) a_in_cd = 1;
    if (c <= b && b <= d) b_in_cd = 1;
    /* See if a < c, d < b */
    if (a < c && c < b) c_in_ab = 1;
    if (a < d && d < b) d_in_ab = 1;

    if (a_in_cd && b_in_cd) {
	if (c_in_ab || d_in_ab) {
	    ret = WEDGE2_OVERLAP;	/* ERROR */
	    goto out;
	}
	ret = WEDGE2_AB_IN_CD;
	goto out;
    }
    if (c_in_ab && d_in_ab) {
	if (a_in_cd || b_in_cd) {
	    ret = WEDGE2_OVERLAP;	/* ERROR */
	    goto out;
	}
	ret = WEDGE2_CD_IN_AB;
	goto out;
    }
    if (a_in_cd + b_in_cd + c_in_ab + d_in_ab <= 0) {
	ret = WEDGE2_NO_OVERLAP;
	goto out;
    }
    ret = WEDGE2_OVERLAP;			/* ERROR */
out:
    if (nmg_debug&NMG_DEBUG_VU_SORT) {
	bu_log(" a_in_cd=%d, b_in_cd=%d, c_in_ab=%d, d_in_ab=%d\n",
	       a_in_cd, b_in_cd, c_in_ab, d_in_ab);
	bu_log("nmg_compare_2_wedges(%g, %g, %g, %g) = %d %s\n",
	       a, b, c, d, ret, WEDGE2_TO_STRING(ret));
    }
    if (ret <= WEDGE2_OVERLAP) {
	bu_log("nmg_compare_2_wedges(%g, %g, %g, %g) ERROR!\n", a, b, c, d);
	bu_bomb("nmg_compare_2_wedges() ERROR\n");
    }
    return ret;
}


/**
 * Find the VU which is inside (or on) the given wedge,
 * fitting as tightly to the given wedge as possible,
 * and with the lowest value of lo_ang possible.
 * XXX how to do tie-breaking for the two coincident ones where
 * XXX two loops come together.
 *
 * lo_ang < hi_ang on RIGHT side of intersection line
 * lo_ang > hi_ang on LEFT side of intersection line
 *
 * There are three wedges involved here:
 * the original one, from lo to hi,
 * the current best "candidate" so far,
 * and "this", the current one being considered.
 */
static int
nmg_find_vu_in_wedge(struct nmg_vu_stuff *vs, int start, int end, double lo_ang, double hi_ang, int wclass, int *skip_array)

/* vu index of coincident range */


{
    register int i;
    double cand_lo;
    double cand_hi;
    int candidate;

    if (nmg_debug&NMG_DEBUG_VU_SORT)
	bu_log("nmg_find_vu_in_wedge(start=%d, end=%d, lo=%g, hi=%g) START\n",
	       start, end, lo_ang, hi_ang);

    candidate = -1;
    cand_lo = lo_ang;
    cand_hi = hi_ang;

    /* Consider all the candidates */
    for (i=start; i < end; i++) {
	int this_wrt_orig;
	int this_wrt_cand;

	NMG_CK_VERTEXUSE(vs[i].vu);
	if (skip_array[i]) {
	    if (nmg_debug&NMG_DEBUG_VU_SORT)
		bu_log("Skipping index %d\n", i);
	    continue;
	}

	/* Ignore wedges crossing, or on other side of line */
	if (vs[i].wedge_class != wclass && vs[i].wedge_class != WEDGE_ON) {
	    if (nmg_debug&NMG_DEBUG_VU_SORT) {
		bu_log("Seeking wedge_class=%s, [%d] has wedge_class %s\n",
		       WEDGECLASS2STR(wclass), i, WEDGECLASS2STR(vs[i].wedge_class));
	    }
	    continue;
	}

	if (vs[i].wedge_class == WEDGE_ON) {
	    /* Ensure that comparisons will work */
	    if (wclass == WEDGE_RIGHT) {
		vs[i].lo_ang = 0;
		vs[i].hi_ang = 180;
	    } else {
		vs[i].lo_ang = 360;
		vs[i].hi_ang = 180;
	    }
	}

	this_wrt_orig = nmg_compare_2_wedges(
	    vs[i].lo_ang, vs[i].hi_ang,
	    lo_ang, hi_ang);
	switch (this_wrt_orig) {
	    case WEDGE2_AB_IN_CD:
		break;
	    case WEDGE2_IDENTICAL:
		candidate = i;
		goto out;
	    default:
		continue;	/* not inside wedge */
	}

	if (candidate < 0) {
	    /* This wedge AB is inside original wedge.
	     * If candidate is -1, use AB as candidate.
	     */
	    if (nmg_debug&NMG_DEBUG_VU_SORT)
		bu_log("Initial candidate %d selected\n", i);
	    candidate = i;
	    cand_lo = vs[i].lo_ang;
	    cand_hi = vs[i].hi_ang;
	    continue;
	}

	this_wrt_cand = nmg_compare_2_wedges(
	    vs[i].lo_ang, vs[i].hi_ang,
	    cand_lo, cand_hi);
	switch (this_wrt_cand) {
	    case WEDGE2_CD_IN_AB:
		/* This wedge AB contains candidate wedge CD, therefore
		 * this wedge is closer to original wedge */
		if (nmg_debug&NMG_DEBUG_VU_SORT)
		    bu_log("This candidate %d is closer\n", i);
		candidate = i;
		cand_lo = vs[i].lo_ang;
		cand_hi = vs[i].hi_ang;
		break;
	    case WEDGE2_NO_OVERLAP:
		/* No overlap, but both are inside.  Take lower angle */
		if (vs[i].lo_ang < cand_lo) {
		    if (nmg_debug&NMG_DEBUG_VU_SORT)
			bu_log("Taking lower angle %d\n", i);
		    candidate = i;
		    cand_lo = vs[i].lo_ang;
		    cand_hi = vs[i].hi_ang;
		}
		break;
	    default:
		if (nmg_debug&NMG_DEBUG_VU_SORT)
		    bu_log("Continuing with search\n");
		continue;
	}
    }
out:
    if (nmg_debug&NMG_DEBUG_VU_SORT)
	bu_log("nmg_find_vu_in_wedge(start=%d, end=%d, lo=%g, hi=%g) END candidate=%d\n",
	       start, end, lo_ang, hi_ang,
	       candidate);
    return candidate;	/* is -1 if none found */
}


/**
 * Determine if the 'wedge' vu, which is either a LEFT or RIGHT wedge,
 * should be processed before or after the 'cross' vu, which is a
 * CROSS wedge.
 *
 * Returns -
 * 1 if wedge should be processed before cross
 * 0 if cross should be processed before wedge
 */
static int
nmg_is_wedge_before_cross(const struct nmg_vu_stuff *wedge, const struct nmg_vu_stuff *cross)
{
    int class2;
    int ret = -1;

    if (cross->wedge_class != WEDGE_CROSS || wedge->wedge_class == WEDGE_CROSS)
	bu_bomb("nmg_is_wedge_before_cross() bad input\n");

    /* LEFT/RIGHT Wedge is (AB), CROSS wedge is (CD) */
    class2 = nmg_compare_2_wedges(wedge->lo_ang, wedge->hi_ang,
				  cross->lo_ang, cross->hi_ang);

    switch (class2) {
	default:
	    bu_log("nmg_is_wedge_before_cross() class2=%s\n",
		   WEDGE2_TO_STRING(class2));
	    bu_bomb("nmg_is_wedge_before_cross(): bad wedge comparison\n");
	case WEDGE2_NO_OVERLAP:
	    /* wedge is not inside cross, cross is not inside wedge. */
	    /* Do wedge first */
	    ret = 1;
	    break;
	case WEDGE2_TOUCH_AT_BC:
	    /* Should only happen when wedge is WEDGE_RIGHT */
	    if (wedge->wedge_class != WEDGE_RIGHT)
		bu_bomb("WEDGE not RIGHT with WEDGE2_TOUCH_AT_BC?\n");
	    ret = 1;
	    break;
	case WEDGE2_TOUCH_AT_DA:
	    /* Should only happen when wedge is WEDGE_LEFT */
	    if (wedge->wedge_class != WEDGE_LEFT)
		bu_bomb("WEDGE not LEFT with WEDGE2_TOUCH_AT_DA?\n");
	    ret = 1;
	    break;
	case WEDGE2_AB_IN_CD:
	    /* 'wedge' is inside the 'cross' wedge, do it second. */
	    ret = 0;
	    break;
    }
    if (nmg_debug&NMG_DEBUG_VU_SORT) {
	bu_log("nmg_is_wedge_before_cross() class2=%s, ret=%d\n",
	       WEDGE2_TO_STRING(class2), ret);
    }
    return ret;
}


/**
 * Support routine for nmg_face_coincident_vu_sort(), via bu_sort().
 *
 * It is important to note that an edge on the LEFT side of the ray
 * will have a "lo" angle which is numerically LARGER than the "hi" angle.
 * However, all are measured in the usual units:
 * 0 = ON_REV, 90 = RIGHT, 180 = ON_FORW, 270 = LEFT.
 *
 * Returns -
 * -1 when A < B
 * 0 when A == B
 * +1 when A > B
 */
#define A_LT_B {ret = -1; goto out;}
#define AB_EQUAL {ret = 0; goto out;}
#define A_GT_B {ret = 1; goto out;}
static int
nmg_face_vu_compare(const void *aa, const void *bb, void *UNUSED(arg))
{
    register const struct nmg_vu_stuff *a = (const struct nmg_vu_stuff *)aa;
    register const struct nmg_vu_stuff *b = (const struct nmg_vu_stuff *)bb;
    register double diff;
    int lo_equal = 0;
    int hi_equal = 0;
    register int ret = 0;

    lo_equal = NEAR_EQUAL(a->lo_ang, b->lo_ang, WEDGE_ANG_TOL);
    hi_equal = NEAR_EQUAL(a->hi_ang, b->hi_ang, WEDGE_ANG_TOL);
    /* If both have the same assessment & angles match, => tie */
    if (a->wedge_class == b->wedge_class && lo_equal && hi_equal) {
	/* Break the tie */
    tie_break:
	if (a->loop_index == b->loop_index) {
	    /* Within a loop, sort by vu sequence number */
	    if (a->seq < b->seq) A_LT_B;
	    if (a->seq == b->seq) AB_EQUAL;
	    A_GT_B;
	}
	/* Select smallest inbound angle */
	diff = a->in_vu_angle - b->in_vu_angle;
	if (NEAR_ZERO(diff, WEDGE_ANG_TOL)) {
	    /* Gak, this really means trouble! */
	    bu_log("nmg_face_vu_compare(): two loops (single vertex) have same in_vu_angle%g?\n",
		   a->in_vu_angle);
	    nmg_pr_vu_stuff(a);
	    nmg_pr_vu_stuff(b);
	    bu_bomb("nmg_face_vu_compare():  can't decide\n");
	    AB_EQUAL;
	}
	if (diff < 0) A_LT_B;
	A_GT_B;
    }
    switch (a->wedge_class) {
	case WEDGE_ON:
	    switch (b->wedge_class) {
		case WEDGE_ON:
		    goto tie_break;
		default:
		    nmg_pr_vu_stuff(a);
		    nmg_pr_vu_stuff(b);
		    bu_bomb("nmg_face_vu_compare() WEDGE_ON 1?\n");
	    }
	    break;

	case WEDGE_LEFT:
	    switch (b->wedge_class) {
		case WEDGE_LEFT:
		    if (lo_equal) {
			/* hi_equal case handled above */
			if (a->hi_ang < b->hi_ang) A_LT_B;
			A_GT_B;
		    }
		    if (a->lo_ang > b->lo_ang) A_LT_B;
		    A_GT_B;
		case WEDGE_CROSS:
		    /* A is LEFT, B is CROSS */
		    if (nmg_is_wedge_before_cross(a, b)) A_LT_B;
		    A_GT_B;
		case WEDGE_RIGHT:
		    diff = 360 - a->lo_ang;/* CW version of left angle */
		    if (b->lo_ang <= diff) A_GT_B;
		    A_LT_B;
		case WEDGE_ON:
		    nmg_pr_vu_stuff(a);
		    nmg_pr_vu_stuff(b);
		    bu_bomb("nmg_face_vu_compare() WEDGE_ON 2?\n");
	    }
	    bu_log("nmg_face_vu_compare: Unhandled wedge class: %d\n", b->wedge_class);
	    break;

	case WEDGE_CROSS:
	    switch (b->wedge_class) {
		case WEDGE_LEFT:
		    /* A (AB) is CROSS, (CD) is LEFT */
		    if (nmg_is_wedge_before_cross(b, a)) A_GT_B;
		    A_LT_B;
		case WEDGE_CROSS:
		    if (lo_equal) {
			if (a->hi_ang > b->hi_ang) A_LT_B;
			A_GT_B;
		    }
		    if (a->lo_ang < b->lo_ang) A_LT_B;
		    A_GT_B;
		case WEDGE_RIGHT:
		    /* A is CROSS, B is RIGHT */
		    if (nmg_is_wedge_before_cross(b, a)) A_GT_B;
		    A_LT_B;
		case WEDGE_ON:
		    nmg_pr_vu_stuff(a);
		    nmg_pr_vu_stuff(b);
		    bu_bomb("nmg_face_vu_compare() WEDGE_ON 3?\n");
	    }
	    bu_log("nmg_face_vu_compare: Unhandled wedge class: %d\n", b->wedge_class);
	    break;

	case WEDGE_RIGHT:
	    switch (b->wedge_class) {
		case WEDGE_LEFT:
		    diff = 360 - b->lo_ang;/* CW version of left angle */
		    if (a->lo_ang <= diff) A_LT_B;
		    A_GT_B;
		case WEDGE_CROSS:
		    /* A is RIGHT, B is CROSS */
		    if (nmg_is_wedge_before_cross(a, b)) A_LT_B;
		    A_GT_B;
		case WEDGE_RIGHT:
		    if (lo_equal) {
			if (a->hi_ang < b->hi_ang) A_GT_B;
			A_LT_B;
		    }
		    if (a->lo_ang < b->lo_ang) A_LT_B;
		    A_GT_B;
		case WEDGE_ON:
		    nmg_pr_vu_stuff(a);
		    nmg_pr_vu_stuff(b);
		    bu_bomb("nmg_face_vu_compare() WEDGE_ON 4?\n");
	    }
	    bu_log("nmg_face_vu_compare: Unhandled wedge class: %d\n", b->wedge_class);
	    break;
    }
out:
    if (nmg_debug&NMG_DEBUG_VU_SORT) {
	bu_log("nmg_face_vu_compare(vu=%p, vu=%p) %s %s, %s\n",
	       (void *)a->vu, (void *)b->vu,
	       WEDGECLASS2STR(a->wedge_class),
	       WEDGECLASS2STR(b->wedge_class),
	       ret == (-1) ? "A<B" : (ret == 0 ? "A==B" : "A>B"));
    }
    return ret;
}


/**
 * For the purpose of computing the dot product of the edges around
 * this vertexuse and the ray direction vector, the edge vectors should
 * both be pointing inwards to the vertex,
 * rather than in the edge direction, so that it is possible to sort
 * the vertexuse's into sequence by "angle" along the ray direction,
 * starting with the vertexuse that the ray first encounters.
 */
static void
nmg_face_vu_dot(struct nmg_vu_stuff *vsp, struct loopuse *lu, const struct nmg_ray_state *rs, int ass)
{
    struct edgeuse *this_eu;
    struct edgeuse *othereu;
    vect_t vec;
    fastf_t dot;
    struct vertexuse *vu;
    int this_vertex;

    NMG_CK_RAYSTATE(rs);
    vu = vsp->vu;
    NMG_CK_VERTEXUSE(vu);
    NMG_CK_LOOPUSE(lu);
    this_eu = nmg_find_eu_with_vu_in_lu(lu, vu);

    /* First, consider the edge inbound into this vertex */
    this_vertex = NMG_V_ASSESSMENT_PREV(ass);
    if (this_vertex == NMG_E_ASSESSMENT_ON_REV) {
	vsp->min_vu_dot = -1;		/* straight back */
    } else if (this_vertex == NMG_E_ASSESSMENT_ON_FORW) {
	vsp->min_vu_dot = 1;		/* straight forw */
    } else {
	othereu = BU_LIST_PPREV_CIRC(edgeuse, this_eu);
	if (vu->v_p != othereu->vu_p->v_p) {
	    /* Vector from othereu to this_eu */
	    VSUB2(vec, vu->v_p->vg_p->coord,
		  othereu->vu_p->v_p->vg_p->coord);
	    VUNITIZE(vec);
	    vsp->min_vu_dot = VDOT(vec, rs->dir);
	} else {
	    vsp->min_vu_dot = 99;		/* larger than +1 */
	}
    }

    /* Second, consider the edge outbound from this vertex (forw) */
    this_vertex = NMG_V_ASSESSMENT_NEXT(ass);
    if (this_vertex == NMG_E_ASSESSMENT_ON_REV) {
	dot = -1;		/* straight back */
	if (dot < vsp->min_vu_dot) vsp->min_vu_dot = dot;
    } else if (this_vertex == NMG_E_ASSESSMENT_ON_FORW) {
	dot = 1;		/* straight forw */
	if (dot < vsp->min_vu_dot) vsp->min_vu_dot = dot;
    } else {
	othereu = BU_LIST_PNEXT_CIRC(edgeuse, this_eu);
	if (vu->v_p != othereu->vu_p->v_p) {
	    /* Vector from othereu to this_eu */
	    VSUB2(vec, vu->v_p->vg_p->coord,
		  othereu->vu_p->v_p->vg_p->coord);
	    VUNITIZE(vec);
	    dot = VDOT(vec, rs->dir);
	    if (dot < vsp->min_vu_dot) {
		vsp->min_vu_dot = dot;
	    }
	}
    }
}


/**
 * If one loop gets cut, then unwind the whole call stack, and reassess
 * where things stand.  (The caller needs to re-call in that case).
 *
 * The goal here is to get rid of nested wedges.
 * To do that, join loops wherever possible, cut them sparingly.
 *
 * Returns -
 * 0 Nothing needed changing, OK to proceed with vertex sorting.
 * 1 Loops were cut or joined, need to reclassify everything
 * at this vertexuse.
 */
static int
nmg_special_wedge_processing(struct nmg_vu_stuff *vs, int start, int end, double lo_ang, double hi_ang, int wclass, int *exclude, struct bu_list *vlfree, const struct bn_tol *tol)

/* vu index of coincident range */


{
    register int i;
    int outer_wedge;
    int inner_wedge;
    int class2;
    struct loopuse *outer_lu;
    struct loopuse *inner_lu;
    int not_these[128];

    BN_CK_TOL(tol);

    if (nmg_debug&NMG_DEBUG_VU_SORT) {
	char buf[128];
	FILE *fp;
	struct model *m;
	long *b;
	struct bv_vlblock *vbp;
	static int num = 0;

	bu_log("nmg_special_wedge_processing(start=%d, end=%d, lo=%g, hi=%g, wclass=%s)\n",
	       start, end, lo_ang, hi_ang,
	       WEDGECLASS2STR(wclass));
	VPRINT("\tvertex", vs[start].vu->v_p->vg_p->coord);

	/* Plot all the loops that touch here. */
	m = nmg_find_model((uint32_t *)vs[start].vu);
	b = (long *)bu_calloc(m->maxindex, sizeof(long), "nmg_special_wedge_processing flag[]");
	vbp = bv_vlblock_init(vlfree, 32);
	for (i=start; i < end; i++) {
	    struct loopuse *lu;
	    lu = nmg_find_lu_of_vu(vs[i].vu);
	    bu_log("\tvu[%d]=%p, lu=%p\n", i, (void *)vs[i].vu, (void *)lu);
	    nmg_vlblock_lu(vbp, lu, b, 255, 0, 0, 0, vlfree);
	}
	for (i=start; i < end; i++) {
	    struct loopuse *lu;
	    lu = nmg_find_lu_of_vu(vs[i].vu);
	    nmg_pr_lu_briefly(lu, 0);
	}
	sprintf(buf, "wedge%d.plot3", num++);
	fp = fopen(buf, "wb");
	bv_plot_vlblock(fp, vbp);
	fclose(fp);
	bu_log("wrote %s\n", buf);
	bu_free((char *)b, "nmg_special_wedge_processing flag[]");
	bv_vlblock_free(vbp);
    }

    if (end-start >= 128) bu_bomb("nmg_special_wedge_processing: array overflow\n");
    if (!exclude) {
	memset((char *)not_these, 0, sizeof(not_these));
	exclude = not_these;
    }

again:
    /* May be many "outer" wedges to iterate over this side of line */
    outer_wedge = nmg_find_vu_in_wedge(vs, start, end,
				       lo_ang, hi_ang, wclass, exclude);
    if (outer_wedge <= -1) return 0;	/* No wedges to process */

    exclude[outer_wedge] = 1;	/* Don't return this wedge again */

    /* There is at least one wedge on this side of the line */
    outer_lu = nmg_find_lu_of_vu(vs[outer_wedge].vu);
    NMG_CK_LOOPUSE(outer_lu);

again_inner:
    inner_wedge = nmg_find_vu_in_wedge(vs, start, end,
				       vs[outer_wedge].lo_ang, vs[outer_wedge].hi_ang,
				       wclass, exclude);
    if (inner_wedge <= -1) {
	/*
	 * See if there is another outer wedge that starts where
	 * outer_wedge left off.
	 * exclude[outer_wedge] is already set.
	 */
	goto again;
    }
    if (inner_wedge == outer_wedge) bu_bomb("nmg_special_wedge_processing() identical vu selections?\n");

    class2 = nmg_compare_2_wedges(vs[outer_wedge].lo_ang, vs[outer_wedge].hi_ang,
				  vs[inner_wedge].lo_ang, vs[inner_wedge].hi_ang);
    if (nmg_debug&NMG_DEBUG_VU_SORT)
	bu_log("+++nmg_special_wedge_processing() outer=%d, inner=%d, class2=%s\n", outer_wedge, inner_wedge, WEDGE2_TO_STRING(class2));

    inner_lu = nmg_find_lu_of_vu(vs[inner_wedge].vu);
    NMG_CK_LOOPUSE(inner_lu);

    if (outer_lu == inner_lu) {
	struct loopuse *new_lu;
	if (class2 == WEDGE2_IDENTICAL &&
	    NEAR_EQUAL(vs[inner_wedge].hi_ang, vs[inner_wedge].lo_ang, WEDGE_ANG_TOL)
	    ) {
	    if (nmg_debug&NMG_DEBUG_VU_SORT)
		bu_log("nmg_special_wedge_processing:  inner and outer wedges from same loop, WEDGE2_IDENTICAL & 0deg spread, already in final form.\n");
	    exclude[inner_wedge] = 1;	/* Don't return this wedge again */
	    /* Don't need to recurse only because this is a crack */
	    goto again_inner;
	}
	if (nmg_debug&NMG_DEBUG_VU_SORT)
	    bu_log("nmg_special_wedge_processing:  inner and outer wedges from same loop, cutting loop\n");
	new_lu = nmg_cut_loop(vs[outer_wedge].vu, vs[inner_wedge].vu, vlfree);
	NMG_CK_LOOPUSE(new_lu);
	NMG_CK_LOOPUSE(inner_lu);
	nmg_loop_a(inner_lu->l_p, tol);
	nmg_loop_a(new_lu->l_p, tol);
	nmg_lu_reorient(inner_lu);
	nmg_lu_reorient(new_lu);
	return 1;		/* cutjoin was done */
    }

    /* XXX Or they could be WEDGE2_IDENTICAL */
    /* XXX If WEDGE2_IDENTICAL, could we join and then simplify? */
    if (nmg_debug&NMG_DEBUG_VU_SORT)
	bu_log("wedge at vu[%d] is inside wedge at vu[%d]\n", inner_wedge, outer_wedge);

    if (outer_lu->orientation == inner_lu->orientation) {
	/*
	 * Two loops meet at this vu.  Joining them will impose
	 * a natural edgeuse ordering onto the vu's.
	 */
	if (nmg_debug&NMG_DEBUG_VU_SORT)
	    bu_log("joining loops\n");
	vs[inner_wedge].vu = nmg_join_2loops(vs[outer_wedge].vu,
					     vs[inner_wedge].vu);
	nmg_loop_a(outer_lu->l_p, tol);
	return 1;		/* cutjoin was done */
    }

    /* Recurse on inner wedge */
    if (nmg_special_wedge_processing(vs, start, end,
				     vs[inner_wedge].lo_ang, vs[inner_wedge].hi_ang, wclass,
				     exclude, vlfree, tol))
	return 1;	/* An inner wedge was cut */

    /*
     * Inner and outer are different loopuses,
     * have different orientations,
     * and have nothing complex inside the wedge of the inner loop.
     * Join inner and outer loops here, to impose a proper vu ordering.
     */
    if (nmg_debug&NMG_DEBUG_VU_SORT)
	bu_log("Inner wedge is simple, join inner and outer loops.\n");

    vs[inner_wedge].vu = nmg_join_2loops(vs[outer_wedge].vu,
					 vs[inner_wedge].vu);
    nmg_loop_a(outer_lu->l_p, tol);
    return 1;		/* cutjoin was done */
}


/**
 * Given co-incident vertexuses (same distance along the ray),
 * sort them into the "proper" order for driving the state machine.
 */
static int
nmg_face_coincident_vu_sort(struct nmg_ray_state *rs, int start, int end, struct bu_list *vlfree)

/* first index */
/* last index + 1 */
{
    int num;
    struct nmg_vu_stuff *vs;
    struct nmg_loop_stuff *ls;
    int nloop;
    unsigned nvu;
    unsigned i;
    struct loopuse *lu;
    int ass;
    int l;
    int retries = 0;

    if (nmg_debug&NMG_DEBUG_VU_SORT)
	bu_log("nmg_face_coincident_vu_sort(, %d, %d) START\n", start, end);

    NMG_CK_RAYSTATE(rs);

    num = end - start;
    vs = (struct nmg_vu_stuff *)bu_malloc(sizeof(struct nmg_vu_stuff)*num,
					  "nmg_vu_stuff");
    ls = (struct nmg_loop_stuff *)bu_malloc(sizeof(struct nmg_loop_stuff)*num,
					    "nmg_loop_stuff");

top:
    if (retries++ > 24) bu_bomb("nmg_face_coincident_vu_sort() infinite loop\n");
    /* Assess each vu, create list of loopuses, find max angles */
    nloop = 0;
    nvu = 0;
    if (start < 0 || end < 0)
	bu_log("%s:%d Internal Error\n", __FILE__, __LINE__);
    for (i = (unsigned)end-1; i >= (unsigned)start; i--) {
	lu = nmg_find_lu_of_vu(rs->vu[i]);
	NMG_CK_LOOPUSE(lu);
	ass = nmg_assess_vu(rs, i);
	if (nmg_debug&NMG_DEBUG_VU_SORT)
	    bu_log("vu[%d]=%p v=%p assessment=%s\n",
		   i, (void *)rs->vu[i], (void *)rs->vu[i]->v_p, nmg_v_assessment_names[ass]);
	/* Ignore lone vertices, unless that is all that there is,
	 * in which case, let just one through.  (return 'start+1');
	 */
	if (*(rs->vu[i]->up.magic_p) == NMG_LOOPUSE_MAGIC) {
	    if (start < 0)
		bu_log("%s:%d Internal Error\n", __FILE__, __LINE__);

	    if (nvu > 0 || i > (unsigned)start) {
		/* Drop this loop of a single vertex in sanitize() */
		lu->orientation =
		    lu->lumate_p->orientation = OT_BOOLPLACE;
		/* "continue" keeps vu from being added to vs[] */
		continue;
	    }
	}

	vs[nvu].vu = rs->vu[i];
	vs[nvu].seq = -1;		/* Not assigned yet */

	/* x_dir is -dir, y_dir is -left */
	vs[nvu].in_vu_angle = nmg_vu_angle_measure(rs->vu[i],
						   rs->ang_x_dir, rs->ang_y_dir, ass, 1) * RAD2DEG;
	vs[nvu].out_vu_angle = nmg_vu_angle_measure(rs->vu[i],
						    rs->ang_x_dir, rs->ang_y_dir, ass, 0) * RAD2DEG;

	/* Special case for LEFT & ON combos */
	if (ass == NMG_V_COMB(NMG_E_ASSESSMENT_ON_FORW, NMG_E_ASSESSMENT_LEFT))
	    vs[nvu].in_vu_angle = 360;
	else if (ass == NMG_V_COMB(NMG_E_ASSESSMENT_LEFT, NMG_E_ASSESSMENT_ON_REV))
	    vs[nvu].out_vu_angle = 360;

	vs[nvu].wedge_class = nmg_wedge_class(ass, vs[nvu].in_vu_angle, vs[nvu].out_vu_angle);
	if (nmg_debug&NMG_DEBUG_VU_SORT) bu_log("nmg_wedge_class = %d %s\n", vs[nvu].wedge_class, WEDGECLASS2STR(vs[nvu].wedge_class));
	/* Sort the angles (Don't forget to sort for CROSS case too) */
	if ((vs[nvu].wedge_class == WEDGE_LEFT && vs[nvu].in_vu_angle > vs[nvu].out_vu_angle) ||
	    (vs[nvu].wedge_class != WEDGE_LEFT && vs[nvu].in_vu_angle < vs[nvu].out_vu_angle)) {
	    vs[nvu].lo_ang = vs[nvu].in_vu_angle;
	    vs[nvu].hi_ang = vs[nvu].out_vu_angle;
	} else {
	    vs[nvu].lo_ang = vs[nvu].out_vu_angle;
	    vs[nvu].hi_ang = vs[nvu].in_vu_angle;
	}

	/* Check entering and departing edgeuse angle w.r.t. ray */
	/* This is already done once in nmg_assess_vu();  reuse? */
	/* Computes vs[nvu].min_vu_dot */
	nmg_face_vu_dot(&vs[nvu], lu, rs, ass);

	/* Search for loopuse table entry */
	for (l = 0; l < nloop; l++) {
	    if (ls[l].lu == lu) goto got_loop;
	}
	/* didn't find loopuse in table, add to table */
	l = nloop++;
	ls[l].lu = lu;
	ls[l].n_vu_in_loop = 0;
	ls[l].min_dot = 99;		/* > +1 */
    got_loop:
	ls[l].n_vu_in_loop++;
	vs[nvu].loop_index = l;
	vs[nvu].lsp = &ls[l];
	if (vs[nvu].min_vu_dot < ls[l].min_dot) {
	    ls[l].min_dot = vs[nvu].min_vu_dot;
	    ls[l].min_vu = vs[nvu].vu;
	}
	nvu++;
    }

    /*
     * For each loop which has more than one vertexuse present on the
     * ray, start at the vu which has the smallest angle off the ray,
     * and walk the edges of the loop, marking off the vu sequence for
     * those vu's on the ray (those vu's found in vs[].vu).
     */
    for (l = 0; l < nloop; l++) {
	register struct edgeuse *eu;
	struct edgeuse *first_eu;
	int seq = 0;

	if (ls[l].n_vu_in_loop <= 1) continue;

	first_eu = nmg_find_eu_with_vu_in_lu(ls[l].lu, ls[l].min_vu);
	eu = first_eu;
	do {
	    register struct vertexuse *vu = eu->vu_p;
	    NMG_CK_VERTEXUSE(vu);
	    for (i = 0; i < nvu; i++) {
		if (vs[i].vu == vu) {
		    vs[i].seq = seq++;
		    break;
		}
	    }
	    eu = BU_LIST_PNEXT_CIRC(edgeuse, eu);
	} while (eu != first_eu);
    }

    /* For loops with >1 crossings here, determine proper VU ordering on that loop */
    /* XXX */

    /* Here is where the special wedge-breaking code goes */
    if (nmg_special_wedge_processing(vs, 0, nvu, 0.0, 180.0, WEDGE_RIGHT, (int *)0, vlfree, rs->tol)) {
	if (nmg_debug&NMG_DEBUG_VU_SORT)
	    bu_log("*** nmg_face_coincident_vu_sort(, %d, %d) restarting after 0--180 wedge\n", start, end);
	goto top;
    }
    /* XXX reclass on/on edges from WEDGE_RIGHT to WEDGE_LEFT here? */
    if (nmg_special_wedge_processing(vs, 0, nvu, 360.0, 180.0, WEDGE_LEFT, (int *)0, vlfree, rs->tol)) {
	if (nmg_debug&NMG_DEBUG_VU_SORT)
	    bu_log("*** nmg_face_coincident_vu_sort(, %d, %d) restarting after 180-360 wedge\n", start, end);
	goto top;
    }

    if (nmg_debug&NMG_DEBUG_VU_SORT) {
	bu_log("Loop table (before sort):\n");
	for (l = 0; l < nloop; l++) {
	    bu_log("  index=%d, lu=%p, min_dot=%g, #vu=%d\n",
		   l, (void *)ls[l].lu, ls[l].min_dot,
		   ls[l].n_vu_in_loop);
	}
    }

    /* Sort the vertexuse table into appropriate order */
    bu_sort((void *)vs, (unsigned)nvu, (unsigned)sizeof(*vs),
	    nmg_face_vu_compare, NULL);

    if (nmg_debug&NMG_DEBUG_VU_SORT) {
	bu_log("Vertexuse table (after sort):\n");
	for (i = 0; i < nvu; i++) {
	    bu_log("  %p, l=%d, in/o=(%g, %g), lo/hi=(%g, %g), %s, sq=%d\n",
		   (void *)vs[i].vu, vs[i].loop_index,
		   vs[i].in_vu_angle, vs[i].out_vu_angle,
		   vs[i].lo_ang, vs[i].hi_ang,
		   WEDGECLASS2STR(vs[i].wedge_class),
		   vs[i].seq);
	}
    }

    /* Copy new vu's back to main array */
    {
	for (i = 0; i < nvu; i++) {
	    rs->vu[start+i] = vs[i].vu;
	}
    }
    if (nmg_debug&NMG_DEBUG_VU_SORT) {
	for (i = 0; i < nvu; i++) {
	    bu_log(" vu[%d]=%p, v=%p\n",
		   start+i, (void *)rs->vu[start+i], (void *)rs->vu[start+i]->v_p);
	}
    }

    bu_free((char *)vs, "nmg_vu_stuff");
    bu_free((char *)ls, "nmg_loop_stuff");

    if (nmg_debug&NMG_DEBUG_VU_SORT)
	bu_log("nmg_face_coincident_vu_sort(, %d, %d) END, ret=%d\n", start, end, start+nvu);

    return start+nvu;
}


/**
 * Eliminate any OT_BOOLPLACE self-loops that remain behind in this face.
 */
void
nmg_sanitize_fu(struct faceuse *fu)
{
    struct loopuse *lu;
    struct loopuse *lunext;

    NMG_CK_FACEUSE(fu);

    lu = BU_LIST_FIRST(loopuse, &fu->lu_hd);
    while (BU_LIST_NOT_HEAD(lu, &fu->lu_hd)) {
	NMG_CK_LOOPUSE(lu);
	lunext = BU_LIST_PNEXT(loopuse, lu);
	if (lu->orientation == OT_BOOLPLACE) {
	    /* XXX What to do with return code? */
	    if (nmg_klu(lu)) bu_bomb("nmg_sanitize_fu() nmg_klu() emptied face?\n");
	}
	lu = lunext;
    }
}
#endif

/**
 * Set up nmg_ray_state structure.
 * "left" is a vector that lies in the plane of the face
 * which contains the loops being operated on.
 * It points in the direction "left" of the ray, such that the first
 * OT_SAME loop in the first OT_SAME fact that it crosses will cross
 * the ray in a left-to-right manner consistent with the CCW loop rule.
 * There are some special conditions placed on the "dir" argument to
 * make this happen;  see the comments in nmg_inter.c for details, or
 * Mike's notes "The 'Left' Vector Choice" dated 27-Aug-93, page 1.
 */
static void
nmg_face_rs_init(struct nmg_ray_state *rs, struct bu_ptbl *b, struct faceuse *fu1, struct faceuse *fu2, fastf_t *pt, fastf_t *dir, struct edge_g_lseg *eg, const struct bn_tol *tol)

/* table of vertexuses in fu1 on intercept line */
/* face being worked */
/* for plane equation */


/* may be null.  Geom of isect line. */

{
    plane_t n1;

    BN_CK_TOL(tol);
    BU_CK_PTBL(b);
    NMG_CK_FACEUSE(fu1);
    NMG_CK_FACEUSE(fu2);
    if (eg) NMG_CK_EDGE_G_LSEG(eg);

    memset((char *)rs, 0, sizeof(rs));

    rs->magic = NMG_RAYSTATE_MAGIC;
    rs->tol = tol;
    rs->vu = (struct vertexuse **)b->buffer;
    rs->nvu = b->end;
    rs->eg_p = eg;
    rs->sA = fu1->s_p;
    rs->sB = fu2->s_p;
    rs->fu1 = fu1;
    rs->fu2 = fu2;
    VMOVE(rs->pt, pt);
    VMOVE(rs->dir, dir);
    NMG_GET_FU_PLANE(n1, fu1);
    VCROSS(rs->left, n1, dir);
    VUNITIZE(rs->left);
    switch (fu1->orientation) {
	case OT_SAME:
	    break;
	case OT_OPPOSITE:
	    VREVERSE(rs->left, rs->left);
	    break;
	default:
	    bu_bomb("nmg_face_rs_init: bad orientation\n");
    }
    if (nmg_debug&NMG_DEBUG_FCUT) {
	struct loopuse *lu;
	struct edgeuse *eu;
	struct vertexuse *vu;
	size_t i;

	bu_log("\tfu->orientation=%s\n", nmg_orientation(fu1->orientation));
	HPRINT("\tfg N", n1);
	VPRINT("\t  pt", pt);
	VPRINT("\t dir", dir);
	VPRINT("\tleft", rs->left);
	bu_log("\tvertexuses in fu that are on lintersect line:\n");
	for (i = 0; i < BU_PTBL_LEN(b); i++) {
	    vu = (struct vertexuse *)BU_PTBL_GET(b, i);
	    nmg_pr_vu_briefly(vu, "\t  ");
	}
	bu_log("\tLoopuse in fu (%p):\n", (void *)fu1);
	for (BU_LIST_FOR(lu, loopuse, &fu1->lu_hd)) {
	    bu_log("\tLOOPUSE %p:\n", (void *)lu);
	    if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
		vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
		nmg_pr_vu_briefly(vu, "\tVertex Loop: ");
	    } else {
		for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
		    struct edgeuse *eu_next;
		    vect_t eu_dir;
		    fastf_t eu_len;
		    double inv_len;

		    nmg_pr_eu_briefly(eu, "\t\t");
		    eu_next = BU_LIST_PNEXT_CIRC(edgeuse, eu);
		    VSUB2(eu_dir, eu_next->vu_p->v_p->vg_p->coord, eu->vu_p->v_p->vg_p->coord);
		    eu_len = MAGNITUDE(eu_dir);
		    if (eu_len < VDIVIDE_TOL)
			inv_len = 0.0;
		    else
			inv_len = 1.0/eu_len;
		    for (i = 0; i < 3; i++)
			eu_dir[i] = eu_dir[i] * inv_len;
		    bu_log("\t\t\teu_dir = (%g, %g, %g), length = %g\n", V3ARGS(eu_dir), eu_len);
		}
	    }
	}
    }
    rs->state = NMG_STATE_OUT;

    /* For measuring angle CCW around plane from -dir */
    VREVERSE(rs->ang_x_dir, dir);
    VREVERSE(rs->ang_y_dir, rs->left);
}
#if 0

/**
 * Force the geometry structure for a given edge to be that of
 * the intersection line between the two faces.
 *
 * Note that sometimes a vertex can appear to lie on more than one
 * line.  It is important to refer to geometry here, to make sure
 * that the edgeuse is not mistakenly fused to the wrong edge geometry.
 *
 * XXX This has the byproduct that not all edgeuses "on" the line
 * XXX of intersection will share rs->eg_p.
 *
 * See the comments in nmg_radial_join_eu() for the rationale.
 */
static void
nmg_edge_geom_isect_line(struct edgeuse *eu, struct nmg_ray_state *rs, const char *reason)
{
    register struct edge_g_lseg *eg;

    NMG_CK_EDGEUSE(eu);
    NMG_CK_RAYSTATE(rs);
    if (eu->g.lseg_p) NMG_CK_EDGE_G_LSEG(eu->g.lseg_p);
    if (rs->eg_p) NMG_CK_EDGE_G_LSEG(rs->eg_p);

    if (nmg_debug&NMG_DEBUG_FCUT) {
	bu_log("nmg_edge_geom_isect_line(eu=%p, %s)\n eu->g=%p, rs->eg=%p at START\n",
	       (void *)eu, reason,
	       (void *)eu->g.magic_p, (void *)rs->eg_p);
    }
    if (!eu->g.magic_p) {
	/* This edgeuse has No edge geometry so far.
	 * It may be a new edge formed by a CUTJOIN operation.
	 */
	if (!rs->eg_p) {
	    nmg_edge_g(eu);
	    eg = eu->g.lseg_p;
	    NMG_CK_EDGE_G_LSEG(eg);
	    VMOVE(eg->e_pt, rs->pt);
	    VMOVE(eg->e_dir, rs->dir);
	    rs->eg_p = eg;
	} else {
	    NMG_CK_EDGE_G_LSEG(rs->eg_p);
	    nmg_use_edge_g(eu, (uint32_t *)rs->eg_p);
	}
	goto out;
    }
    /* Edge has edge geometry */
    if (eu->g.lseg_p == rs->eg_p) return;	/* nothing changes */
    if (!rs->eg_p) {
	/* This is first edge_g found on isect line, remember it */
	eg = eu->g.lseg_p;
	NMG_CK_EDGE_G_LSEG(eg);
	rs->eg_p = eg;
	goto out;
    }

    /*
     * Edge has an edge geometry struct, different from that of isect line.
     * Force all uses of this edge geom to take on isect line's geometry.
     * Everywhere eu->g.lseg_p is seen, replace with rs->eg_p.
     *
     * XXX This is DUBIOUS, as the angle might be very different.
     */
    nmg_jeg(rs->eg_p, eu->g.lseg_p);
out:
    NMG_CK_EDGE_G_LSEG(rs->eg_p);
    if (nmg_debug&NMG_DEBUG_FCUT) {
	bu_log("nmg_edge_geom_isect_line(eu=%p) g=%p, rs->eg=%p at END\n",
	       (void *)eu, (void *)eu->g.magic_p, (void *)rs->eg_p);
    }
}
#endif

static struct bu_ptbl *
find_loop_to_cut(int *index1, int *index2, size_t prior_start, size_t prior_end, size_t next_start, size_t next_end, fastf_t *mid_pt, struct nmg_ray_state *rs, struct bu_list *vlfree)
{
    struct loopuse *lu1, *lu2;
    struct vertexuse *vu1 = (struct vertexuse *)NULL;
    struct vertexuse *vu2 = (struct vertexuse *)NULL;
    struct loopuse *match_lu=(struct loopuse *)NULL;
    struct loopuse *prior_lu, *next_lu;
    struct bu_ptbl *cuts=(struct bu_ptbl *)NULL;
    struct loop_cuts *lcut;
    int count = 0;
    size_t i, j, k;
    int done = 0;

    if (nmg_debug&NMG_DEBUG_FCUT)
	bu_log("find_loop_to_cut: prior_start=%zu, prior_end=%zu, next_start=%zu, next_end=%zu, rs=%p\n",
	       prior_start, prior_end, next_start, next_end, (void *)rs);

    NMG_CK_RAYSTATE(rs);

    /* check if any coincident VU's can give us a loop to cut */
    while (!done) {
	done = 1;
	count++;
	if (count > 100) {
	    bu_log("find_loop_to_cut: infinite loop\n");
	    bu_log("prior_start = %zu, prior_end = %zu, next_start = %zu next_end = %zu\n",
		   prior_start, prior_end, next_start, next_end);
	    bu_log("mid_point = (%f %f %f)\n", V3ARGS(mid_pt));
	    for (i = 0; i < (size_t)rs->nvu; i++)
		bu_log("\t%zu %p\n", i, (void *)rs->vu[i]);
	    bu_bomb("find_loop_to_cut: infinite loop");
	}
	for (i=prior_start; i < prior_end; i++) {
	    int class_pt;

	    prior_lu = nmg_find_lu_of_vu(rs->vu[i]);
	    class_pt = NMG_CLASS_Unknown;
	    for (j=next_start; j < next_end; j++) {
		next_lu = nmg_find_lu_of_vu(rs->vu[j]);
		if (prior_lu == next_lu) {
		    int class_lu;

		    if (!match_lu) {
			match_lu = next_lu;
			NMG_ALLOC(cuts, struct bu_ptbl);
			bu_ptbl_init(cuts, 64, " cuts");
			NMG_ALLOC(lcut, struct loop_cuts);
			lcut->lu = match_lu;
			bu_ptbl_ins(cuts, (long *)lcut);
			continue;
		    }

		    if (match_lu == next_lu)
			continue;

		    if (class_pt == NMG_CLASS_Unknown) {
			class_pt = nmg_class_pnt_lu_except(mid_pt,
							  match_lu, (struct edge *)NULL, vlfree, rs->tol);
			if (match_lu->orientation == OT_OPPOSITE) {
			    if (class_pt == NMG_CLASS_AinB)
				class_pt = NMG_CLASS_AoutB;
			    else if (class_pt == NMG_CLASS_AoutB)
				class_pt = NMG_CLASS_AinB;
			}
		    }
		    class_lu = nmg_classify_lu_lu(next_lu, match_lu, vlfree, rs->tol);

		    if (class_lu == class_pt ||
			class_lu == NMG_CLASS_AonBshared) {
			int found = 0;

			match_lu = next_lu;
			for (k = 0; k < BU_PTBL_LEN(cuts); k++) {
			    lcut = (struct loop_cuts *)BU_PTBL_GET(cuts, k);
			    if (lcut->lu == match_lu) {
				found = 1;
				break;
			    }
			}
			if (!found) {
			    done = 0;
			    NMG_ALLOC(lcut, struct loop_cuts);
			    lcut->lu = match_lu;
			    bu_ptbl_ins(cuts, (long *)lcut);
			}
		    }
		}
	    }
	}
    }

    if (cuts) {
	for (k = 0; k < BU_PTBL_LEN(cuts); k++) {
	    lcut = (struct loop_cuts *)BU_PTBL_GET(cuts, k);
	    match_lu = lcut->lu;

	    if (nmg_debug&NMG_DEBUG_FCUT)
		bu_log("\tfind_loop_to_cut: matching lu's = %p\n", (void *)match_lu);
	    lu1 = match_lu;
	    for (i=prior_start; i < prior_end; i++) {
		if (nmg_find_lu_of_vu(rs->vu[i]) == lu1) {
		    *index1 = i;
		    vu1 = rs->vu[i];
		    lcut->vu1 = vu1;
		    break;
		}
	    }
	    lu2 = match_lu;
	    for (i=next_start; i < next_end; i++) {
		if (nmg_find_lu_of_vu(rs->vu[i]) == lu2) {
		    *index2 = i;
		    vu2 = rs->vu[i];
		    lcut->vu2 = vu2;
		    break;
		}
	    }
	}
    } else {
	if (nmg_debug&NMG_DEBUG_FCUT)
	    bu_log("\tfind_loop_to_cut returning 0\n");
	return (struct bu_ptbl *)NULL;
    }

    for (k = 0; k < BU_PTBL_LEN(cuts); k++) {
	lcut = (struct loop_cuts *)BU_PTBL_GET(cuts, k);
	lu1 = lcut->lu;
	lu2 = lu1;

	/* Check if there is more than one VU from lu2 */
	count = 0;
	for (i=next_start; i<next_end; i++) {
	    if (nmg_find_lu_of_vu(rs->vu[i]) == lu2)
		count++;
	}

	if (count > 1) {
	    struct vertexuse *vu_best;
	    fastf_t vu_angle;
	    vect_t x_dir, y_dir;
	    vect_t norm;

	    if (nmg_debug&NMG_DEBUG_FCUT)
		bu_log("\tfind_loop_to_cut: %d VU's from lu %p\n", count, (void *)lu2);

	    /* need to select correct VU */
	    vu_angle = (-M_PI);
	    vu_best = (struct vertexuse *)NULL;

	    VSUB2(x_dir, vu2->v_p->vg_p->coord, vu1->v_p->vg_p->coord);
	    VUNITIZE(x_dir);
	    NMG_GET_FU_NORMAL(norm, rs->fu1);

	    VCROSS(y_dir, norm, x_dir);
	    VUNITIZE(y_dir);

	    if (nmg_debug&NMG_DEBUG_FCUT)
		bu_log("\tx_dir=(%g %g %g), y_dir=(%g %g %g)\n",
		       V3ARGS(x_dir), V3ARGS(y_dir));

	    for (i=next_start; i<next_end; i++) {
		struct edgeuse *eu;
		fastf_t angle;
		vect_t eu_dir;


		if (nmg_find_lu_of_vu(rs->vu[i]) != lu2)
		    continue;

		if (*(rs->vu[i]->up.magic_p) != NMG_EDGEUSE_MAGIC) {
		    bu_log("nmg_fcut_face: VU (%p) is not from an EU\n", (void *)rs->vu[i]);
		    bu_bomb("nmg_fcut_face: VU is not from an EU");
		}

		/* calculate angle this EU will make with edgeuse
		 * that will be created by the cut/join
		 */
		eu = rs->vu[i]->up.eu_p;
		VSUB2(eu_dir, eu->eumate_p->vu_p->v_p->vg_p->coord, eu->vu_p->v_p->vg_p->coord);
		angle = atan2(VDOT(y_dir, eu_dir), VDOT(x_dir, eu_dir));

		if (nmg_debug&NMG_DEBUG_FCUT)
		    bu_log("\tangle for eu %p (vu=%p, #%zu) is %g\n",
			   (void *)eu, (void *)rs->vu[i], i, angle);

		/* select max angle */
		if (angle > vu_angle) {
		    if (nmg_debug&NMG_DEBUG_FCUT)
			bu_log("\t\tabove is the new best VU\n");
		    vu_angle = angle;
		    vu_best = rs->vu[i];
		    *index2 = i;
		}
	    }

	    if (vu_best) {
		vu2 = vu_best;
		lcut->vu2 = vu2;
	    }

	    if (nmg_debug&NMG_DEBUG_FCUT)
		bu_log("\tfind_loop_to_cut: selecting VU2 %p\n", (void *)vu2);
	}

	/* Check for duplicate cuts (cutting two different loops across same two vertices) */
	if (BU_PTBL_LEN(cuts) > 1) {
	    for (i = 0; i < BU_PTBL_LEN(cuts); i++) {
		struct loop_cuts *lcut1, *lcut2;
		int class1, class2;

		lcut1 = (struct loop_cuts *)BU_PTBL_GET(cuts, i);
		for (j=i+1; j<BU_PTBL_LEN(cuts); j++) {
		    lcut2 = (struct loop_cuts *)BU_PTBL_GET(cuts, j);

		    if (lcut1->vu1->v_p != lcut2->vu1->v_p ||
			lcut1->vu2->v_p != lcut2->vu2->v_p)
			continue;

		    /* lcut1 and lcut2 are the same cut, choose one loop to cut */
		    if (lcut1->lu->orientation == OT_OPPOSITE &&
			lcut2->lu->orientation == OT_OPPOSITE)
		    {
			bu_log("find_loop_to_cut: Two OT_OPPOSITE loops to be cut?? %p and %p\n",
			       (void *)lcut1->lu, (void *)lcut2->lu);
			bu_bomb("find_loop_to_cut: Two OT_OPPOSITE loops to be cut??\n");
		    }
		    if (lcut1->lu->orientation == OT_OPPOSITE) {
			/* don't cut an OT_OPPOSITE loop */
			bu_ptbl_rm(cuts, (long *)lcut1);
			break;
		    }
		    if (lcut2->lu->orientation == OT_OPPOSITE) {
			/* don't cut an OT_OPPOSITE loop */
			bu_ptbl_rm(cuts, (long *)lcut2);
			break;
		    }
		    class1 = nmg_class_pnt_lu_except(mid_pt, lcut1->lu,
						    (struct edge *)NULL, vlfree, rs->tol);
		    class2 = nmg_class_pnt_lu_except(mid_pt, lcut2->lu,
						    (struct edge *)NULL, vlfree, rs->tol);

		    if (class1 == NMG_CLASS_AoutB && class2 == NMG_CLASS_AoutB) {
			bu_log("find_loop_to_cut: mid point is outside both loops??? %p and %p pt=(%g %g %g)\n",
			       (void *)lcut1->lu, (void *)lcut2->lu, V3ARGS(mid_pt));
			bu_bomb("find_loop_to_cut: mid point is outside both loops???\n");
		    }

		    if (class1 == NMG_CLASS_AoutB) {
			/* Don't cut this loop (cut is outside loop) */
			bu_ptbl_rm(cuts, (long *)lcut1);
			break;
		    }
		    if (class2 == NMG_CLASS_AoutB) {
			/* Don't cut this loop (cut is outside loop) */
			bu_ptbl_rm(cuts, (long *)lcut2);
			break;
		    }
		}
	    }
	}

	/* Check if there is more than one VU from lu1 */
	count = 0;
	for (i = prior_start; i < prior_end; i++) {
	    if (nmg_find_lu_of_vu(rs->vu[i]) == lu1)
		count++;
	}

	if (count > 1) {
	    struct vertexuse *vu_best;

	    fastf_t vu_angle;
	    vect_t x_dir, y_dir;
	    vect_t norm;

	    if (nmg_debug&NMG_DEBUG_FCUT)
		bu_log("\tfind_loop_to_cut: %d VU's from lu %p\n", count, (void *)lu1);

	    /* need to select correct VU */
	    vu_angle = (-M_PI);
	    vu_best = (struct vertexuse *)NULL;

	    VSUB2(x_dir, vu1->v_p->vg_p->coord, vu2->v_p->vg_p->coord);
	    VUNITIZE(x_dir);
	    NMG_GET_FU_NORMAL(norm, rs->fu1);

	    VCROSS(y_dir, norm, x_dir);
	    VUNITIZE(y_dir);

	    if (nmg_debug&NMG_DEBUG_FCUT)
		bu_log("\tx_dir=(%g %g %g), y_dir=(%g %g %g)\n",
		       V3ARGS(x_dir), V3ARGS(y_dir));

	    for (i = prior_start; i < prior_end; i++) {
		struct edgeuse *eu;
		fastf_t angle;
		vect_t eu_dir;

		if (nmg_find_lu_of_vu(rs->vu[i]) != lu1)
		    continue;

		if (*(rs->vu[i]->up.magic_p) != NMG_EDGEUSE_MAGIC) {
		    bu_log("nmg_fcut_face: VU (%p) is not from an EU\n", (void *)rs->vu[i]);
		    bu_bomb("nmg_fcut_face: VU is not from an EU");
		}

		/* calculate angle this EU will make with edgeuse
		 * that will be created by the cut/join
		 */
		eu = rs->vu[i]->up.eu_p;
		VSUB2(eu_dir, eu->eumate_p->vu_p->v_p->vg_p->coord, eu->vu_p->v_p->vg_p->coord);
		angle = atan2(VDOT(y_dir, eu_dir), VDOT(x_dir, eu_dir));

		if (nmg_debug&NMG_DEBUG_FCUT)
		    bu_log("\tangle for eu %p (vu=%p, #%zu) is %g\n",
			   (void *)eu, (void *)rs->vu[i], i, angle);

		/* select max angle */
		if (angle > vu_angle) {
		    if (nmg_debug&NMG_DEBUG_FCUT)
			bu_log("\t\tabove is the new best VU\n");
		    vu_angle = angle;

		    vu_best = rs->vu[i];
		    *index1 = i;
		}
	    }

	    if (vu_best) {
		vu1 = vu_best;
		lcut->vu1 = vu1;
	    }

	    if (nmg_debug&NMG_DEBUG_FCUT)
		bu_log("\tfind_loop_to_cut: selecting VU1 %p\n", (void *)vu1);
	}
    }

    if (nmg_debug&NMG_DEBUG_FCUT)
	bu_log("\tfind_loop_to_cut: returning %zu cuts (index1=%d, index2=%d)\n",
	       BU_PTBL_LEN(cuts), *index1, *index2);

    return cuts;
}


static fastf_t
nmg_eu_angle(struct edgeuse *eu, struct vertex *vp)
{
    struct faceuse *fu;
    struct vertex_g *vg1, *vg2;
    vect_t norm;
    vect_t x_dir;
    vect_t y_dir;
    vect_t eu_dir;
    fastf_t angle;

    NMG_CK_EDGEUSE(eu);
    NMG_CK_VERTEX(vp);

    fu = nmg_find_fu_of_eu(eu);
    NMG_CK_FACEUSE(fu);
    NMG_GET_FU_NORMAL(norm, fu);

    vg1 = eu->vu_p->v_p->vg_p;
    vg2 = eu->eumate_p->vu_p->v_p->vg_p;

    VSUB2(x_dir, vg1->coord, vp->vg_p->coord);
    VUNITIZE(x_dir);
    VCROSS(y_dir, norm, x_dir);
    VUNITIZE(y_dir);

    VSUB2(eu_dir, vg2->coord, vg1->coord);
    angle = atan2(VDOT(eu_dir, y_dir), VDOT(eu_dir, x_dir));

    return angle;
}


static int
find_best_vu(int start, int end, struct vertex *other_vp, struct nmg_ray_state *rs, struct bu_list *vlfree)
{
    struct edgeuse *eu;
    struct vertexuse *best_vu;
    struct loopuse *best_lu;
    fastf_t best_angle;
    int best_index;
    int other_is_in_best = -42;
    int nmg_class;
    int i;

    if (nmg_debug&NMG_DEBUG_FCUT)
	bu_log("find_best_vu: start=%d, end=%d, other_vp=%p, rs=%p\n",
	       start, end, (void *)other_vp, (void *)rs);

    NMG_CK_VERTEX(other_vp);
    NMG_CK_RAYSTATE(rs);

    if (start == end-1) {
	if (nmg_debug&NMG_DEBUG_FCUT)
	    bu_log("\tfind_best_vu returning %d\n", start);

	return start;
    }

    best_vu = rs->vu[start];
    best_lu = nmg_find_lu_of_vu(best_vu);
    best_index = start;
    eu = best_vu->up.eu_p;
    best_angle = nmg_eu_angle(eu, other_vp);

    if (BU_LIST_FIRST_MAGIC(&best_lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
	other_is_in_best = 0;
    } else if (nmg_loop_is_a_crack(best_lu)) {
	other_is_in_best = 0;
    } else {
	nmg_class = nmg_class_pnt_lu_except(other_vp->vg_p->coord, best_lu, (struct edge *)NULL, vlfree, rs->tol);

	if ((nmg_class == NMG_CLASS_AinB && best_lu->orientation == OT_SAME) ||
	    (nmg_class == NMG_CLASS_AoutB && best_lu->orientation == OT_OPPOSITE))
	    other_is_in_best = 1;
	else if (nmg_class == NMG_CLASS_AonBshared) {
	    bu_log("find_best_vu: There is a loop to cut, lu=%p\n", (void *)best_lu);
	    bu_bomb("find_best_vu: There is a loop to cut");
	} else
	    other_is_in_best = 0;
    }

    if (nmg_debug&NMG_DEBUG_FCUT)
	bu_log("\tfind_best_vu: first choice is index=%d, vu=%p, lu=%p, other_is_in_best=%d\n",
	       best_index, (void *)best_vu, (void *)best_lu, other_is_in_best);

    for (i=start+1; i<end; i++) {
	struct loopuse *lu;

	lu = nmg_find_lu_of_vu(rs->vu[i]);
	if (lu != best_lu) {
	    nmg_class = nmg_classify_lu_lu(lu, best_lu, vlfree, rs->tol);
	    if (nmg_debug&NMG_DEBUG_FCUT) {
		bu_log("lu %p is %s\n", (void *)lu, nmg_orientation(lu->orientation));
		bu_log("best_lu %p is %s\n", (void *)best_lu, nmg_orientation(best_lu->orientation));
		bu_log("lu %p is %s w.r.t lu %p\n",
		       (void *)lu, nmg_class_name(nmg_class), (void *)best_lu);
	    }

	    if (other_is_in_best) {
		if (nmg_class == NMG_CLASS_AinB || (nmg_class == NMG_CLASS_AonBshared && lu->orientation == OT_SAME)) {

		    best_vu = rs->vu[i];
		    best_lu = lu;
		    best_index = i;

		    if (nmg_debug&NMG_DEBUG_FCUT)
			bu_log("\tfind_best_vu: better choice (inside) - index=%d, vu=%p, lu=%p, other_is_in_best=%d\n",
			       best_index, (void *)best_vu, (void *)best_lu, other_is_in_best);
		    /* other_is_in_best can't change */
		}
	    } else {
		if (nmg_class == NMG_CLASS_AoutB || (nmg_class == NMG_CLASS_AonBshared && lu->orientation == OT_OPPOSITE)) {
		    best_vu = rs->vu[i];
		    best_lu = lu;
		    best_index = i;

		    /* other_is_in_best may change */
		    if (BU_LIST_FIRST_MAGIC(&best_lu->down_hd) == NMG_VERTEXUSE_MAGIC) {
			other_is_in_best = 0;
		    } else if (nmg_loop_is_a_crack(best_lu)) {
			other_is_in_best = 0;
		    } else {
			nmg_class = nmg_class_pnt_lu_except(other_vp->vg_p->coord,
							   best_lu, (struct edge *)NULL, vlfree, rs->tol);

			if ((nmg_class == NMG_CLASS_AinB && best_lu->orientation == OT_SAME) ||
			    (nmg_class == NMG_CLASS_AoutB && best_lu->orientation == OT_OPPOSITE))
			    other_is_in_best = 1;
			else if (nmg_class == NMG_CLASS_AonBshared) {
			    bu_log("find_best_vu: There is a loop to cut, lu=%p\n",
				   (void *)best_lu);
			    bu_bomb("find_best_vu: There is a loop to cut");
			} else
			    other_is_in_best = 0;
		    }
		    if (nmg_debug&NMG_DEBUG_FCUT)
			bu_log("\tfind_best_vu: better choice (outside) - index=%d, vu=%p, lu=%p, other_is_in_best=%d\n",
			       best_index, (void *)best_vu, (void *)best_lu, other_is_in_best);
		}
	    }
	} else {
	    fastf_t angle;
	    /* need to choose based on eu directions */

	    eu = rs->vu[i]->up.eu_p;
	    NMG_CK_EDGEUSE(eu);
	    angle = nmg_eu_angle(eu, other_vp);

	    if (nmg_debug&NMG_DEBUG_FCUT)
		bu_log("best_angle = %f, eu=%p, eu_angle=%f\n",
		       best_angle, (void *)eu, angle);
	    if (angle > best_angle) {
		best_angle = angle;
		best_vu = rs->vu[i];
		best_lu = nmg_find_lu_of_vu(best_vu);
		best_index = i;
	    }
	}
    }

    return best_index;
}

static void
#if PLOT_BOTH_FACES
nmg_fcut_face(struct nmg_ray_state *rs, struct bu_list *vlfree)
#else
nmg_fcut_face(struct nmg_ray_state *rs, struct bu_list *UNUSED(vlfree))
#endif
{
    register int cur;
    struct vertexuse *vu1, *vu2;
    struct loopuse *new_lu;
    struct edgeuse *new_eu1;
    struct edgeuse *old_eu;
    struct vertex *prev_v;
    struct bu_ptbl *cuts;
    size_t prior_start;
    size_t cut_no;

    NMG_CK_RAYSTATE(rs);
    BN_CK_TOL(rs->tol);

    if (nmg_debug&NMG_DEBUG_FCUT)
	bu_log("nmg_face_combine()\n");

    if (rs->eg_p) NMG_CK_EDGE_G_LSEG(rs->eg_p);

#if PLOT_BOTH_FACES
    nmg_2face_plot(rs->fu1, rs->fu2, vlfree);
#else
    nmg_face_plot(rs->fu1);
    nmg_face_plot(rs->fu2);
#endif

    if (nmg_debug&NMG_DEBUG_FCUT) {
	bu_log("rs->fu1 = %p\n", (void *)rs->fu1);
	bu_log("rs->fu2 = %p\n", (void *)rs->fu2);
	nmg_pr_fu_briefly(rs->fu1, "");
	bu_log("%d vertices on intersect line\n", rs->nvu);
	for (cur = 0; cur < rs->nvu; cur++)
	    bu_log("\tvu=%p, v=%p, lu=%p\n",
		   (void *)rs->vu[cur], (void *)rs->vu[cur]->v_p, (void *)nmg_find_lu_of_vu(rs->vu[cur]));
    }


    if (rs->nvu < 2)
	return;

    prev_v = (struct vertex *)NULL;
    prior_start = 0;
    while (1) {
	struct edgeuse *eu_tmp;
	struct loopuse *lu1 = (struct loopuse *)NULL;
	struct loopuse *lu2 = (struct loopuse *)NULL;
	point_t mid_pt;
	int nmg_class;
	int orient1 = 0;
	int orient2 = 0;
	size_t prior_end;
	size_t next_start, next_end;
	size_t i;
	int index1 = 0, index2 = 0;

	while (rs->vu[prior_start]->v_p == prev_v)
	    prior_start++;

	vu1 = rs->vu[prior_start];
	prior_end = prior_start;

	prev_v = vu1->v_p;

	while (++prior_end < (size_t)rs->nvu && rs->vu[prior_end]->v_p == vu1->v_p)
	    ;

	next_start = prior_end;

	if (next_start >= (size_t) rs->nvu)
	    break;		/* all done */

	vu2 = rs->vu[next_start];
	next_end = next_start;
	while (++next_end < (size_t)rs->nvu && rs->vu[next_end]->v_p == vu2->v_p);

	if (nmg_debug&NMG_DEBUG_FCUT) {
	    bu_log("rs->fu1 = %p\n", (void *)rs->fu1);
	    bu_log("rs->fu2 = %p\n", (void *)rs->fu2);
	    bu_log("prior_start=%zu. prior_end=%zu, next_start=%zu, next_end=%zu\n",
		   prior_start, prior_end, next_start, next_end);
	    bu_log("%d vertices on intersect line\n", rs->nvu);
	    for (cur = 0; cur < rs->nvu; cur++)
		bu_log("\tvu=%p, v=%p, lu=%p\n",
		       (void *)rs->vu[cur], (void *)rs->vu[cur]->v_p, (void *)nmg_find_lu_of_vu(rs->vu[cur]));
	}

	/* look for an EU in this face that connects vu1 and vu2 */
	if (nmg_find_eu_in_face(vu1->v_p, vu2->v_p, rs->fu1,
				(struct edgeuse *)NULL, 0))
	{
	    if (nmg_debug&NMG_DEBUG_FCUT)
		bu_log("Already an edge here\n");
	    continue;
	}

	if (nmg_find_eu_in_face(vu1->v_p, vu2->v_p, rs->fu1->fumate_p,
				(struct edgeuse *)NULL, 0))
	{
	    if (nmg_debug&NMG_DEBUG_FCUT)
		bu_log("Already an edge here\n");
	    continue;
	}

	/* we know that fu1 does not contain an edge connecting vu1 and vu2,
	 * Now check if the midpoint is inside or outside fu1.
	 * Note that ON fu1 is not an option at this point
	 */
	VAVERAGE(mid_pt, vu1->v_p->vg_p->coord, vu2->v_p->vg_p->coord)
	    nmg_class = nmg_class_pnt_fu_except(mid_pt, rs->fu1, NULL,
					       NULL,
					       NULL,
					       NULL, 0, 1, vlfree, rs->tol);

	if (nmg_debug&NMG_DEBUG_FCUT) {
	    bu_log("vu1=%p (%g %g %g), vu2=%p (%g %g %g)\n",
		   (void *)vu1, V3ARGS(vu1->v_p->vg_p->coord),
		   (void *)vu2, V3ARGS(vu2->v_p->vg_p->coord));
	    bu_log("\tmid_pt = (%g %g %g)\n", V3ARGS(mid_pt));
	    bu_log("class for mid point is %s\n", nmg_class_name(nmg_class));
	}

	if (nmg_class == NMG_CLASS_AoutB)
	    continue;

	/* Check if mid-point is in fu2. If fu2 is disjoint loops, this point
	 * may be outside fu2, and we don't want to cut fu1 here.
	 */
	nmg_class = nmg_class_pnt_fu_except(mid_pt, rs->fu2, NULL,
					   NULL,
					   NULL,
					   NULL, 0, 0, vlfree, rs->tol);

	if (nmg_class == NMG_CLASS_AoutB)
	    continue;

	/* See if there is an edge joining the 2 vertices already, this
	 * will be used for radial join later */
	old_eu = nmg_findeu(vu1->v_p, vu2->v_p, (struct shell *)NULL,
			    (struct edgeuse *)NULL, 0);


	cuts = find_loop_to_cut(&index1, &index2, prior_start, prior_end,
				next_start, next_end, mid_pt, rs, vlfree);
	if (cuts == (struct bu_ptbl *)NULL) {
	    index1 = find_best_vu(prior_start, prior_end, vu2->v_p, rs, vlfree);
	    index2 = find_best_vu(next_start, next_end, vu1->v_p, rs, vlfree);
	    vu1 = rs->vu[index1];
	    vu2 = rs->vu[index2];
	    lu1 = nmg_find_lu_of_vu(vu1);
	    lu2 = nmg_find_lu_of_vu(vu2);
	    orient1 = lu1->orientation;
	    orient2 = lu2->orientation;
	}

	if (cuts) {
	    for (cut_no = 0; cut_no < BU_PTBL_LEN(cuts); cut_no++) {
		struct loop_cuts *lcut;

		if (nmg_debug&NMG_DEBUG_FCUT)
		    bu_log("\tcut loop (#%zu of %zu)\n", cut_no, BU_PTBL_LEN(cuts));

		lcut = (struct loop_cuts *)BU_PTBL_GET(cuts, cut_no);

		vu1 = lcut->vu1;
		vu2 = lcut->vu2;
		lu1 = lcut->lu;

		new_lu = nmg_cut_loop(vu1, vu2, vlfree);
		new_eu1 = BU_LIST_LAST(edgeuse, &new_lu->down_hd);

		NMG_CK_EDGEUSE(new_eu1);

		/* make intersection edges real */
		new_eu1->e_p->is_real = 1;

		nmg_loop_a(lu1->l_p, rs->tol);
		nmg_loop_a(new_lu->l_p, rs->tol);

		nmg_lu_reorient(lu1);
		nmg_lu_reorient(new_lu);

		if (nmg_debug&NMG_DEBUG_FCUT) {
		    bu_log("\t\t new_eu = %p\n", (void *)new_eu1);
		    nmg_pr_fu_briefly(rs->fu1, "");
		}
		if (old_eu) nmg_radial_join_eu(old_eu, new_eu1, rs->tol);

		/* A new VU has been added to vu2->v_p, add it to the rs->vu[]
		 * array by overwriting the last use of vu1->v_p
		 */

		eu_tmp = vu1->up.eu_p;
		eu_tmp = BU_LIST_PPREV_CIRC(edgeuse, &eu_tmp->l);
		if (eu_tmp->vu_p->v_p != vu2->v_p) {
		    bu_log("nmg_fcut_face: eu (%p) has wrong vertex (%p) should be (%p)\n",
			   (void *)eu_tmp, (void *)eu_tmp->vu_p->v_p, (void *)vu2->v_p);
		    bu_bomb("nmg_fcut_face: eu has wrong vertex");
		}

		rs->vu[prior_end-1-cut_no] = eu_tmp->vu_p;
	    }
	    bu_ptbl_free(cuts);
	    bu_free((char *)cuts, "cuts");

	    continue;
	}

	if (BU_LIST_FIRST_MAGIC(&lu1->down_hd) == NMG_VERTEXUSE_MAGIC &&
	    BU_LIST_FIRST_MAGIC(&lu2->down_hd) == NMG_VERTEXUSE_MAGIC)

	{
	    struct vertexuse *new_vu2;

	    new_vu2 = nmg_join_2singvu_loops(vu1, vu2);
	    new_eu1 = new_vu2->up.eu_p;
	    NMG_CK_EDGEUSE(new_eu1);

	    /* make intersection edges real */
	    new_eu1->e_p->is_real = 1;

	    nmg_loop_a(lu1->l_p, rs->tol);
	    lu1->orientation = OT_SAME;
	    lu1->lumate_p->orientation = OT_SAME;

	    if (nmg_debug&NMG_DEBUG_FCUT) {
		bu_log("\tjoin 2 singvu loops\n");
		nmg_pr_fu_briefly(rs->fu1, "");
	    }
	    if (old_eu) nmg_radial_join_eu(old_eu, new_eu1, rs->tol);

	    /* vu2 has been killed, replace it in rs->vu[] */

	    for (i=next_start; i<next_end; i++) {
		if (rs->vu[i] == vu2) {
		    rs->vu[i] = new_vu2;
		    break;
		}
	    }

	    continue;
	}

	if (BU_LIST_FIRST_MAGIC(&lu1->down_hd) == NMG_VERTEXUSE_MAGIC &&
	    BU_LIST_FIRST_MAGIC(&lu2->down_hd) == NMG_EDGEUSE_MAGIC)
	{
	    struct vertexuse *new_vu1;

	    new_vu1 = nmg_join_singvu_loop(vu2, vu1);
	    new_eu1 = new_vu1->up.eu_p;
	    NMG_CK_EDGEUSE(new_eu1);

	    /* make intersection edges real */
	    new_eu1->e_p->is_real = 1;

	    nmg_loop_a(lu2->l_p, rs->tol);
	    lu2->orientation = orient2;
	    lu2->lumate_p->orientation = orient2;

	    if (nmg_debug&NMG_DEBUG_FCUT) {
		bu_log("\tjoin loops vu1 (%p) is sing vu loop\n", (void *)vu1);
		nmg_pr_fu_briefly(rs->fu1, "");
	    }
	    if (old_eu) nmg_radial_join_eu(old_eu, new_eu1, rs->tol);

	    /* A new VU has been added to vu2->v_p, add it to the rs->vu[]
	     * array by overwriting the last use of vu1->v_p
	     */

	    eu_tmp = new_vu1->up.eu_p;
	    eu_tmp = BU_LIST_PPREV_CIRC(edgeuse, &eu_tmp->l);
	    if (eu_tmp->vu_p->v_p != vu2->v_p) {
		bu_log("nmg_fcut_face: eu (%p) has wrong vertex (%p) should be (%p)\n",
		       (void *)eu_tmp, (void *)eu_tmp->vu_p->v_p, (void *)vu2->v_p);
		bu_bomb("nmg_fcut_face: eu has wrong vertex");
	    }

	    rs->vu[prior_end-1] = eu_tmp->vu_p;
	    continue;
	}

	if (BU_LIST_FIRST_MAGIC(&lu1->down_hd) == NMG_EDGEUSE_MAGIC &&
	    BU_LIST_FIRST_MAGIC(&lu2->down_hd) == NMG_VERTEXUSE_MAGIC)
	{
	    struct vertexuse *new_vu2;

	    new_vu2 = nmg_join_singvu_loop(vu1, vu2);
	    new_eu1 = new_vu2->up.eu_p;
	    NMG_CK_EDGEUSE(new_eu1);

	    /* make intersection edges real */
	    new_eu1->e_p->is_real = 1;

	    nmg_loop_a(lu1->l_p, rs->tol);
	    lu1->orientation = orient1;
	    lu1->lumate_p->orientation = orient1;

	    if (nmg_debug&NMG_DEBUG_FCUT) {
		bu_log("\tjoin loops vu2 (%p) is sing vu loop\n", (void *)vu2);
		nmg_pr_fu_briefly(rs->fu1, "");
	    }

	    if (old_eu) nmg_radial_join_eu(old_eu, new_eu1, rs->tol);

	    /* vu2 has been killed, replace it in rs->vu[] */

	    for (i=next_start; i<next_end; i++) {
		if (rs->vu[i] == vu2) {
		    rs->vu[i] = new_vu2;
		    break;
		}
	    }

	    continue;
	}

	if (lu1 != lu2) {
	    struct vertexuse *new_vu2;

	    new_vu2 = nmg_join_2loops(vu1, vu2);
	    new_eu1 = new_vu2->up.eu_p;
	    NMG_CK_EDGEUSE(new_eu1);

	    /* make intersection edges real */
	    new_eu1->e_p->is_real = 1;

	    nmg_loop_a(lu1->l_p, rs->tol);

	    if (nmg_debug&NMG_DEBUG_FCUT) {
		bu_log("\t join 2 loops\n");
		bu_log("\t\tvu2 (%p) replaced with new_vu2 %p\n", (void *)vu2, (void *)new_vu2);
		nmg_pr_fu_briefly(rs->fu1, "");
	    }

	    if (old_eu) nmg_radial_join_eu(old_eu, new_eu1, rs->tol);

	    /* a new use of vu2->v_p has been created add it to the list by
	     * overwriting a use of the previous vertex
	     */
	    rs->vu[prior_end - 1] = new_vu2;

	    continue;
	}

	bu_log("ERROR in face cutter: something should have been cut!!\n");
	bu_log("vu1=%p, vu2=%p\n", (void *)vu1, (void *)vu2);
	nmg_pr_fu_briefly(rs->fu1, "");
	bu_bomb("ERROR: face cutter didn't cut anything");
    }

    if (nmg_debug&NMG_DEBUG_FCUT)
	nmg_pr_fu_briefly(rs->fu1, "");
}

#if 0

void
nmg_unlist_v(struct bu_ptbl *b, fastf_t *mag, struct vertex *v)
{
    register size_t i;
    struct vertexuse *vu;

    BU_CK_PTBL(b);
    NMG_CK_VERTEX(v);
    for (i = 0; i < BU_PTBL_LEN(b); i++) {
	vu = (struct vertexuse *)BU_PTBL_GET(b, i);
	if (!vu) continue;
	if (vu->v_p == v) {
	    BU_PTBL_GET(b, i) = (long *)0;
	    mag[i] = MAX_FASTF;
	}
    }
}


/**
 * An attempt to fix the condition:
 * nmg_assess_eu():  ON vertexuse in middle of edge?
 *
 * Note that the vertexuse being zapped may have a lower subscript.
 *
 * Must be called after vu list has been sorted.
 */
static int
nmg_onon_fix(struct nmg_ray_state *rs, struct bu_ptbl *b, struct bu_ptbl *ob, fastf_t *mag, fastf_t *omag)


/* other rs's vu list */
/* list of distances from intersect ray start point */
/* list of distances from intersect ray start point */
{
    size_t i;
    int zapped;
    struct vertexuse *vu;
    struct vertex *v;
    int zot;

    NMG_CK_RAYSTATE(rs);
    BU_CK_PTBL(b);
    BU_CK_PTBL(ob);

    zapped = 0;
    for (i = 0; i < BU_PTBL_LEN(b); i++) {
    again:
	vu = (struct vertexuse *)BU_PTBL_GET(b, i);
	if (!vu) continue;
	NMG_CK_VERTEXUSE(vu);
	if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC) continue;
	v = vu->v_p;
	NMG_CK_VERTEX(v);
	if ((zot = nmg_assess_eu(vu->up.eu_p, 0, rs, i)) < 0) {
	    if (nmg_debug&NMG_DEBUG_FCUT) {
		bu_log("nmg_onon_fix(): vu[%d] zapped (rev)\n", -zot);
	    }
	doit:
	    vu = (struct vertexuse *)BU_PTBL_GET(b, -zot);
	    NMG_CK_VERTEXUSE(vu);
	    v = vu->v_p;
	    /* Need to null out all uses of this vertex from both lists */
	    nmg_unlist_v(b, mag, v);
	    nmg_unlist_v(ob, omag, v);
	    zapped++;
	    goto again;
	}

	if ((zot = nmg_assess_eu(vu->up.eu_p, 1, rs, i)) < 0) {
	    if (nmg_debug&NMG_DEBUG_FCUT) {
		bu_log("nmg_onon_fix(): vu[%d] zapped (forw)\n", -zot);
	    }
	    goto doit;
	}
    }
    if (zapped) {
	int removed;

	/* If any vertexuses got zapped, their pointer was set to NULL.
	 * They can all be removed from the list in one easy operation.
	 */
	bu_log("nmg_onon_fix(): removing %d dead vertexuses\n", zapped);
	/* remove entries from distance list first */
	removed = 0;
	for (i= BU_PTBL_LEN(b)+1; i > 0; i--) {
	    int count=0;
	    int j;
	    size_t k;

	    j = i-1;
	    while (j >= 0 && !BU_PTBL_GET(b, j))
		--j;
	    count = i-1-j;
	    removed += count;
	    for (k = j + 1; k < BU_PTBL_LEN(b)-removed; k++)
		mag[k] = mag[k+count];
	}
	bu_ptbl_rm(b, 0);
	removed = 0;
	for (i = BU_PTBL_LEN(ob)+1; i > 0; i--) {
	    int count=0;
	    int j;
	    size_t k;

	    j = i-1;
	    while (j>=0 && !BU_PTBL_GET(ob, j)) --j;
	    count = i-1-j;
	    removed += count;
	    for (k = j + 1; k < BU_PTBL_LEN(ob)-removed; k++)
		omag[k] = omag[k+count];
	}
	bu_ptbl_rm(ob, 0);
	return 1;
    }

    return 0;
}
#endif

/**
 * The main face cut handler.
 * Called from nmg_inter.c by nmg_isect_2faces().
 *
 * A wrapper for nmg_face_combine, for now.
 *
 * The two vertexuse lists may be of different lengths, because
 * one may have multiple uses of a vertex, while the other has only
 * a single use of that same vertex.
 */
struct edge_g_lseg *
nmg_face_cutjoin(struct bu_ptbl *b1, struct bu_ptbl *b2, fastf_t *mag1, fastf_t *mag2, struct faceuse *fu1, struct faceuse *fu2, point_t pt, vect_t dir, struct edge_g_lseg *eg, struct bu_list *vlfree, const struct bn_tol *tol)
/* table of vertexuses in fu1 on intercept line */
/* table of vertexuses in fu2 on intercept line */
/* table of distances to vertexuses from is->pt */
/* table of distances to vertexuses from is->pt */
/* face being worked */
/* for plane equation */


/* may be null.  geometry of isect line */

{
    struct vertexuse **vu1, **vu2;
    size_t i;
    struct nmg_ray_state rs1;
    struct nmg_ray_state rs2;

    if (nmg_debug&NMG_DEBUG_FCUT) {
	bu_log("\nnmg_face_cutjoin(fu1=%p, fu2=%p) eg=%p START\n", (void *)fu1, (void *)fu2, (void *)eg);
    }

    BN_CK_TOL(tol);
    NMG_CK_FACEUSE(fu1);
    NMG_CK_FACEUSE(fu2);

    /* Perhaps this should only happen when debugging is on? */
    if (nmg_debug&NMG_DEBUG_FCUT && (b1->end <= 0 || b2->end <= 0)) {
	bu_log("nmg_face_cutjoin(fu1=%p, fu2=%p): WARNING empty list %zu %zu\n",
	       (void *)fu1, (void *)fu2, b1->end, b2->end);
    }

top:
    /*
     * Sort hit points by increasing distance, vertex ptr, vu ptr,
     * and eliminate any duplicate vu's.
     */
    ptbl_vsort(b1, pt, dir, mag1, tol->dist);
    ptbl_vsort(b2, pt, dir, mag2, tol->dist);

    vu1 = (struct vertexuse **)b1->buffer;
    vu2 = (struct vertexuse **)b2->buffer;

    /* Print list of intersections */
    if (nmg_debug&NMG_DEBUG_FCUT) {
	bu_log("Ray vu intersection list:\n");
	for (i = 0; i < b1->end; i++) {
	    bu_log(" %zu %e ", i, mag1[i]);
	    nmg_pr_vu_briefly(vu1[i], (char *)0);
	}
	for (i = 0; i < b2->end; i++) {
	    bu_log(" %zu %e ", i, mag2[i]);
	    nmg_pr_vu_briefly(vu2[i], (char *)0);
	}
    }

    /* Check to make sure that intersector didn't miss anything */
    if (nmg_ck_vu_ptbl(b1, fu1) || nmg_ck_vu_ptbl(b2, fu2)) goto top;


    /* this block of code checks if the two lists of intersection vertexuses
     * contain vertexuses from the appropriate faceuse */
    if (nmg_debug&NMG_DEBUG_FCUT) {
	int found1=(-1), found2=(-1); /* -1 => not set, 0 => no vertexuses from faceuse found */
	int tmp_found;
	struct faceuse *fu;
	struct vertexuse *vu;

	/* list b1 should contain vertexuses from faceuse fu1 */
	for (i = 0; i < BU_PTBL_LEN(b1); i++) {
	    tmp_found = 0;
	    vu = (struct vertexuse *)BU_PTBL_GET(b1, i);
	    fu = nmg_find_fu_of_vu(vu);
	    if (fu == fu1)
		tmp_found = 1;
	    if (found1 == (-1))
		found1 = tmp_found;
	    else if (tmp_found != found1) {
		/* some found, some not found!!!!! */
		bu_log ("nmg_face_cutjoin: Intersection list is screwy for face 1\n");
		break;
	    }
	}

	/* and list b2 should contain vertexuses from faceuse fu2 */
	for (i = 0; i < BU_PTBL_LEN(b2); i++) {
	    tmp_found = 0;
	    vu = (struct vertexuse *)BU_PTBL_GET(b2, i);
	    fu = nmg_find_fu_of_vu(vu);
	    if (fu == fu2)
		tmp_found = 1;
	    if (found2 == (-1))
		found2 = tmp_found;
	    else if (tmp_found != found2) {
		/* some found, some not found!!!!! */
		bu_log ("nmg_face_cutjoin: Intersection list is screwy for face 2\n");
		break;
	    }
	}
	if (!found1)
	    bu_log("nmg_face_cutjoin: intersection list for face 1 doesn't contain vertexuses from face 1!!!\n");
	if (!found2)
	    bu_log("nmg_face_cutjoin: intersection list for face 2 doesn't contain vertexuses from face 2!!!\n");
    }

    nmg_face_rs_init(&rs1, b1, fu1, fu2, pt, dir, eg, tol);
    nmg_face_rs_init(&rs2, b2, fu2, fu1, pt, dir, eg, tol);
    nmg_fcut_face(&rs1, vlfree);
    nmg_fcut_face(&rs2, vlfree);

    /* Can't do simplifications here,
     * because the caller's linked lists & pointers might get disrupted.
     */

    /* Merging uses of common edges is OK, though, and quite necessary. */
    i = nmg_mesh_two_faces(fu1, fu2, tol);
    if (i) {
	if (nmg_debug&NMG_DEBUG_FCUT)
	    bu_log("nmg_face_cutjoin() meshed %zu edges\n", i);
    }
    if (nmg_debug&NMG_DEBUG_FCUT) {
	bu_log("nmg_face_cutjoin(fu1=%p, fu2=%p) eg=%p END\n", (void *)fu1, (void *)fu2, (void *)rs1.eg_p);
    }
    if (eg && !rs1.eg_p)
	bu_bomb("nmg_face_cutjoin() lost edge_g_lseg?\n");
    if (eg && eg != rs1.eg_p)
	bu_log("nmg_face_cutjoin() changed from eg=%p to rs1.eg_p=%p\n", (void *)eg, (void *)rs1.eg_p);
    return rs1.eg_p;
}

void
nmg_fcut_face_2d(struct bu_ptbl *vu_list, fastf_t *UNUSED(mag), struct faceuse *fu1, struct faceuse *fu2, struct bu_list *vlfree, struct bn_tol *tol)
{
    struct nmg_ray_state rs;
    point_t pt = VINIT_ZERO;
    vect_t dir;
    struct edge_g_lseg *eg;

    NMG_CK_FACEUSE(fu1);
    NMG_CK_FACEUSE(fu2);
    BN_CK_TOL(tol);
    BU_CK_PTBL(vu_list);

    VSET(dir, 1.0, 0.0 , 0.0);
    eg = (struct edge_g_lseg *)NULL;

    nmg_face_rs_init(&rs, vu_list, fu1, fu2, pt,  dir, eg, tol);

    nmg_fcut_face(&rs, vlfree);
}

#if 0

/*
 * State machine transition tables
 * Indexed by MNG_V_ASSESSMENT values.
 */

struct state_transitions {
    int assessment;
    int new_state;
    int action;
};


static const struct state_transitions nmg_state_is_out[17] = {
    { NMG_LEFT_LEFT,	NMG_STATE_OUT,		NMG_ACTION_NONE },
    { NMG_LEFT_RIGHT,	NMG_STATE_IN,		NMG_ACTION_VFY_EXT },
    { NMG_LEFT_ON_FORW,	NMG_STATE_ON_L,		NMG_ACTION_VFY_EXT },
    { NMG_LEFT_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_RIGHT_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_RIGHT_RIGHT,	NMG_STATE_OUT,		NMG_ACTION_NONE },
    { NMG_RIGHT_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_RIGHT_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_FORW_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_FORW_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_FORW_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_FORW_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_REV_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_REV_RIGHT,	NMG_STATE_ON_R,		NMG_ACTION_VFY_EXT },
    { NMG_ON_REV_ON_FORW,	NMG_STATE_ON_N,		NMG_ACTION_NONE },
    { NMG_ON_REV_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_LONE,		NMG_STATE_OUT,		NMG_ACTION_NONE }
};


static const struct state_transitions nmg_state_is_on_L[17] = {
    { NMG_LEFT_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_LEFT_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_LEFT_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_LEFT_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_RIGHT_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_RIGHT_RIGHT,	NMG_STATE_ON_L,		NMG_ACTION_NONE },
    { NMG_RIGHT_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_RIGHT_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_FORW_LEFT,	NMG_STATE_OUT,		NMG_ACTION_NONE },
    { NMG_ON_FORW_RIGHT,	NMG_STATE_IN,		NMG_ACTION_NONE },
    { NMG_ON_FORW_ON_FORW,	NMG_STATE_ON_L,		NMG_ACTION_NONE },
    { NMG_ON_FORW_ON_REV,	NMG_STATE_IN,		NMG_ACTION_NONE },
    { NMG_ON_REV_LEFT,	NMG_STATE_ON_N,		NMG_ACTION_VFY_MULTI },
    { NMG_ON_REV_RIGHT,	NMG_STATE_ON_B,		NMG_ACTION_VFY_EXT },
    { NMG_ON_REV_ON_FORW,	NMG_STATE_ON_L,		NMG_ACTION_VFY_MULTI },
    { NMG_ON_REV_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_LONE,		NMG_STATE_ON_L,		NMG_ACTION_LONE_V_ESPLIT }
};


static const struct state_transitions nmg_state_is_on_R[17] = {
    { NMG_LEFT_LEFT,	NMG_STATE_ON_R,		NMG_ACTION_NONE },
    { NMG_LEFT_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_LEFT_ON_FORW,	NMG_STATE_ON_B,		NMG_ACTION_NONE },
    { NMG_LEFT_ON_REV,	NMG_STATE_IN,		NMG_ACTION_NONE },
    { NMG_RIGHT_LEFT,	NMG_STATE_ON_N,		NMG_ACTION_VFY_MULTI },
    { NMG_RIGHT_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_RIGHT_ON_FORW,	NMG_STATE_ON_N,		NMG_ACTION_VFY_MULTI },
    { NMG_RIGHT_ON_REV,	NMG_STATE_OUT,		NMG_ACTION_NONE },
    { NMG_ON_FORW_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_FORW_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_FORW_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_FORW_ON_REV,	NMG_STATE_IN,		NMG_ACTION_NONE },
    { NMG_ON_REV_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_REV_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_REV_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_REV_ON_REV,	NMG_STATE_ON_R,		NMG_ACTION_NONE },
    { NMG_LONE,		NMG_STATE_ON_R,		NMG_ACTION_LONE_V_ESPLIT }
};


static const struct state_transitions nmg_state_is_on_B[17] = {
    { NMG_LEFT_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_LEFT_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_LEFT_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_LEFT_ON_REV,	NMG_STATE_IN,		NMG_ACTION_VFY_MULTI },
    { NMG_RIGHT_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_RIGHT_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_RIGHT_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_RIGHT_ON_REV,	NMG_STATE_ON_L,		NMG_ACTION_NONE },
    { NMG_ON_FORW_LEFT,	NMG_STATE_ON_R,		NMG_ACTION_NONE },
    { NMG_ON_FORW_RIGHT,	NMG_STATE_IN,		NMG_ACTION_VFY_MULTI },
    { NMG_ON_FORW_ON_FORW,	NMG_STATE_ON_B,		NMG_ACTION_VFY_MULTI },
    { NMG_ON_FORW_ON_REV,	NMG_STATE_IN,		NMG_ACTION_NONE },
    { NMG_ON_REV_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_REV_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_REV_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_REV_ON_REV,	NMG_STATE_ON_B,		NMG_ACTION_VFY_MULTI },
    { NMG_LONE,		NMG_STATE_ON_B,		NMG_ACTION_LONE_V_ESPLIT }
};


static const struct state_transitions nmg_state_is_on_N[17] = {
    { NMG_LEFT_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_LEFT_RIGHT,	NMG_STATE_OUT,		NMG_ACTION_VFY_MULTI },
    { NMG_LEFT_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_LEFT_ON_REV,	NMG_STATE_ON_L,		NMG_ACTION_VFY_MULTI },
    { NMG_RIGHT_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_RIGHT_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_RIGHT_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_RIGHT_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_FORW_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_FORW_RIGHT,	NMG_STATE_ON_R,		NMG_ACTION_VFY_MULTI },
    { NMG_ON_FORW_ON_FORW,	NMG_STATE_ON_N,		NMG_ACTION_VFY_MULTI },
    { NMG_ON_FORW_ON_REV,	NMG_STATE_OUT,		NMG_ACTION_NONE },
    { NMG_ON_REV_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_REV_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_REV_ON_FORW,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_REV_ON_REV,	NMG_STATE_ON_N,		NMG_ACTION_VFY_MULTI },
    { NMG_LONE,		NMG_STATE_ON_N,		NMG_ACTION_LONE_V_ESPLIT }
};


static const struct state_transitions nmg_state_is_in[17] = {
    { NMG_LEFT_LEFT,	NMG_STATE_IN,		NMG_ACTION_CUTJOIN },
    { NMG_LEFT_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_LEFT_ON_FORW,	NMG_STATE_ON_R,		NMG_ACTION_CUTJOIN },
    { NMG_LEFT_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_RIGHT_LEFT,	NMG_STATE_OUT,		NMG_ACTION_CUTJOIN },
    { NMG_RIGHT_RIGHT,	NMG_STATE_IN,		NMG_ACTION_CUTJOIN },
    { NMG_RIGHT_ON_FORW,	NMG_STATE_ON_L,		NMG_ACTION_CUTJOIN },
    { NMG_RIGHT_ON_REV,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_FORW_LEFT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_FORW_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_FORW_ON_FORW,	NMG_STATE_IN,		NMG_ACTION_NONE },
    { NMG_ON_FORW_ON_REV,	NMG_STATE_OUT,		NMG_ACTION_NONE },
    { NMG_ON_REV_LEFT,	NMG_STATE_ON_R,		NMG_ACTION_CUTJOIN },
    { NMG_ON_REV_RIGHT,	NMG_STATE_ERROR,	NMG_ACTION_ERROR },
    { NMG_ON_REV_ON_FORW,	NMG_STATE_ON_B,		NMG_ACTION_CUTJOIN },
    { NMG_ON_REV_ON_REV,	NMG_STATE_IN,		NMG_ACTION_NONE },
    { NMG_LONE,		NMG_STATE_IN,		NMG_ACTION_LONE_V_JAUNT }
};


/**
 * Given current (old) state, assess the current vertexuse, and
 * pull the appropriate action and new state from the tables.
 * Then perform the indicated action.
 *
 * The real work happens in the nmg_assess_vu() and in the tables.
 *
 * Explicit Returns -
 * Nothing useful yet.
 *
 * Implicit Returns -
 * Modifications to the NMG shell being operated on.
 * Updated state etc. in nmg_ray_state structure.
 */
static int
nmg_face_state_transition(struct nmg_ray_state *rs, int pos, int multi, int other_rs_state, struct bu_list *vlfree)
{
    int assessment;
    int old_state;
    int new_state;
    const struct state_transitions *stp;
    struct vertexuse *vu;
    struct vertexuse *prev_vu = (struct vertexuse *)NULL;
    struct loopuse *lu;
    struct loopuse *prev_lu;
    struct faceuse *fu;
    struct edgeuse *eu;
    struct edgeuse *first_new_eu;
    struct edgeuse *second_new_eu;
    struct edgeuse *old_eu;
    int e_assessment;
    int action;
    int e_pos;

    NMG_CK_RAYSTATE(rs);
    vu = rs->vu[pos];
    NMG_CK_VERTEXUSE(vu);
    BN_CK_TOL(rs->tol);
    if (rs->eg_p) NMG_CK_EDGE_G_LSEG(rs->eg_p);

    if (nmg_debug&NMG_DEBUG_FCUT) {
	bu_log("nmg_face_state_transition(vu %p, pos=%d) START\n",
	       (void *)vu, pos);
	bu_log("Plotting this loopuse, before action:\n");
	nmg_pr_lu_briefly(nmg_find_lu_of_vu(vu), (char *)0);
	nmg_plot_lu_ray(nmg_find_lu_of_vu(vu),
			rs->vu[0], rs->vu[rs->nvu-1], rs->left, vlfree);
    }

    if (nmg_debug & NMG_DEBUG_VERIFY) {
	nmg_vfu(&rs->fu1->s_p->fu_hd, rs->fu1->s_p);
	nmg_vfu(&rs->fu2->s_p->fu_hd, rs->fu2->s_p);
	nmg_fu_touchingloops(rs->fu1);
	nmg_fu_touchingloops(rs->fu2);
    }

    assessment = nmg_assess_vu(rs, pos);
    old_state = rs->state;
    switch (old_state) {
	default:
	case NMG_STATE_ERROR:
	    bu_bomb("nmg_face_state_transition: was in ERROR state\n");
	case NMG_STATE_OUT:
	    stp = &nmg_state_is_out[assessment];
	    break;
	case NMG_STATE_ON_L:
	    stp = &nmg_state_is_on_L[assessment];
	    break;
	case NMG_STATE_ON_R:
	    stp = &nmg_state_is_on_R[assessment];
	    break;
	case NMG_STATE_ON_B:
	    stp = &nmg_state_is_on_B[assessment];
	    break;
	case NMG_STATE_ON_N:
	    stp = &nmg_state_is_on_N[assessment];
	    break;
	case NMG_STATE_IN:
	    stp = &nmg_state_is_in[assessment];
	    break;
    }

    if (assessment != stp->assessment) {
	bu_log("assessment=%d, stp->assessment=%d, error\n", assessment, stp->assessment);
	bu_bomb("nmg_face_state_transition() bad table\n");
    }
    action = stp->action;
    new_state = stp->new_state;
    rs->last_action = action;

    /*
     * Major optimization here.
     * If the state machine for the other face is still in OUT state,
     * then take no actions in this face,
     * because any cutting or joining done here will have no effect
     * on the final result of the boolean, it's just extra work.
     * This can reduce the amount of unnecessary topology by 75% or more.
     */
    if (other_rs_state == NMG_STATE_OUT && action != NMG_ACTION_ERROR &&
	action != NMG_ACTION_NONE) {
	action = NMG_ACTION_NONE_OPTIM;
    }

    if (nmg_debug&NMG_DEBUG_FCUT) {
	bu_log("nmg_face_state_transition(vu %p, pos=%d)\n\told=%s, assessed=%s, new=%s, action=%s\n",
	       (void *)vu, pos,
	       nmg_state_names[old_state], nmg_v_assessment_names[assessment],
	       nmg_state_names[new_state], action_names[action]);
    }

    /*
     * Force edge geometry that lies on the intersection line
     * to use the edge_g_lseg structure of the intersection line (ray).
     */
    if (NMG_V_ASSESSMENT_PREV(assessment) == NMG_E_ASSESSMENT_ON_REV) {
	eu = nmg_find_eu_of_vu(vu);
	eu = BU_LIST_PPREV_CIRC(edgeuse, eu);
	NMG_CK_EDGEUSE(eu);
	if (!rs->eg_p || eu->g.lseg_p != rs->eg_p) {
	    if (rs->eg_p) {
		NMG_CK_EDGE_G_LSEG(rs->eg_p);
	    }
	    nmg_edge_geom_isect_line(eu, rs, "force ON_REV to line");
	}
    }
    if (NMG_V_ASSESSMENT_NEXT(assessment) == NMG_E_ASSESSMENT_ON_FORW) {
	eu = nmg_find_eu_of_vu(vu);
	NMG_CK_EDGEUSE(eu);
	if (!rs->eg_p || eu->g.lseg_p != rs->eg_p) {
	    if (rs->eg_p) {
		NMG_CK_EDGE_G_LSEG(rs->eg_p);
	    }
	    nmg_edge_geom_isect_line(eu, rs, "force ON_FORW to line");
	}
    }

    switch (action) {
	default:
	case NMG_ACTION_ERROR:
	bomb:
	    {
		struct bu_vls str = BU_VLS_INIT_ZERO;

		bu_log("nmg_face_state_transition: got action=ERROR\n");

		bu_vls_printf(&str, "nmg_face_state_transition(vu %p, pos=%d)\n\told=%s, assessed=%s, new=%s, action=%s\n",
			      (void *)vu, pos,
			      nmg_state_names[old_state], nmg_v_assessment_names[assessment],
			      nmg_state_names[new_state], action_names[action]);
		if (nmg_debug) {
		    /* First, print this faceuse */
		    lu = nmg_find_lu_of_vu(vu);
		    NMG_CK_LOOPUSE(lu);
		    /* Print the faceuse for later analysis */
		    bu_log("Loop with the offending vertex\n");
		    nmg_pr_lu_briefly(lu, (char *)0);
		}
		/* Explode */
		bu_bomb(bu_vls_addr(&str));
	    }
	case NMG_ACTION_NONE:
	case NMG_ACTION_NONE_OPTIM:
	    if (*(vu->up.magic_p) != NMG_LOOPUSE_MAGIC) break;
	    lu = vu->up.lu_p;
	    if (old_state != NMG_STATE_OUT && lu->orientation == OT_BOOLPLACE) {
		/* If something connects to the surface of our face,
		 * hang on to this vertexuse.
		 */
		lu->orientation = lu->lumate_p->orientation = OT_UNSPEC;
	    }
	    break;
	case NMG_ACTION_VFY_EXT:
	    /* Verify loop containing this vertex has external orientation */
	    lu = nmg_find_lu_of_vu(vu);
	    NMG_CK_LOOPUSE(lu);
	    switch (lu->orientation) {
		case OT_SAME:
		    break;
		default:
		    bu_log("nmg_face_state_transition: VFY_EXT got orientation=%s\n",
			   nmg_orientation(lu->orientation));
		    break;
	    }
	    break;
	case NMG_ACTION_VFY_MULTI:
	    /* Ensure that there are multiple vertexuse's at this
	     * vertex along the ray.
	     * If not, the table entry is illegal.
	     */
	    if (multi) break;
	    bu_log("nmg_face_state_transition: VFY_MULTI had only 1 vertex\n");
	    goto bomb;
	case NMG_ACTION_LONE_V_ESPLIT:
	    /*
	     * Split edge to include vertex from this lone vert loop.
	     * This only happens in an "ON" state, so split the edge that
	     * starts (or ends) with the previously seen vertex.
	     * Note that the forward going edge may point the wrong way,
	     * i.e., not lie on the ray at all.
	     * Also note that the previous member(s) of vu[] may be
	     * lone vert loops that were not processed due to optimization,
	     * so it may be necessary to look back a ways to find
	     * the vertexuse which started this ON edge.
	     */
	    lu = nmg_find_lu_of_vu(vu);
	    NMG_CK_LOOPUSE(lu);
	    for (e_pos = pos-1; e_pos >= 0; e_pos--) {
		prev_vu = rs->vu[e_pos];
		NMG_CK_VERTEXUSE(prev_vu);
		/* lu is lone vert loop; l_p is distinct from prev_lu->l_p */
		if (*prev_vu->up.magic_p == NMG_EDGEUSE_MAGIC)
		    break;
		/* Not an edgeuse, prob. a loopuse, continue backwards */
	    }
	    if (e_pos < 0) bu_bomb("nmg_face_state_transition: LONE_V_ESPLIT can't find start of edge!\n");
	    eu = prev_vu->up.eu_p;
	    NMG_CK_EDGEUSE(eu);
	    e_assessment = nmg_assess_eu(eu, 1, rs, e_pos);	/* forw */
	    if (e_assessment == NMG_E_ASSESSMENT_ON_FORW) {
		/* "eu" (forw) is the right edge to split */
	    } else {
		e_assessment = nmg_assess_eu(eu, 0, rs, e_pos); /*rev*/
		if (e_assessment == NMG_E_ASSESSMENT_ON_REV) {
		    /* (reverse) "eu" is the right one */
		    eu = BU_LIST_PPREV_CIRC(edgeuse, eu);
		} else {
		    /* What went wrong? */
		    bu_log("LONE_V_ESPLIT:  rev e_assessment = %s\n", nmg_e_assessment_names[e_assessment]);
		    bu_bomb("nmg_face_state_transition: LONE_V_ESPLIT could not find ON edge to split\n");
		}
	    }
	    /* Break edge, update vu table with new value */
	    if (vu->v_p == eu->vu_p->v_p) {
		/* Edge already starts at same vertex */
		rs->vu[pos] = eu->vu_p;
	    } else if (vu->v_p == eu->eumate_p->vu_p->v_p) {
		/* Edge already ends at same vertex */
		rs->vu[pos] = BU_LIST_PNEXT_CIRC(edgeuse, eu)->vu_p;
	    } else {
		/* Break edge, force onto isect line */
		nmg_edge_geom_isect_line(eu, rs, "LONE_V_ESPLIT, before edge split");
		rs->vu[pos] = nmg_ebreaker(vu->v_p, eu, rs->tol)->vu_p;
	    }
	    /* Kill lone vertex loop (and vertexuse) */
	    nmg_klu(lu);
	    if (nmg_debug&NMG_DEBUG_FCUT) {
		bu_log("After LONE_V_ESPLIT, the final loop:\n");
		lu = nmg_find_lu_of_vu(rs->vu[pos]);
		NMG_CK_LOOPUSE(lu);
		nmg_pr_lu(lu, "   ");
		nmg_plot_lu_ray(lu, rs->vu[0], rs->vu[rs->nvu-1], rs->left, vlfree);
	    }
	    break;
	case NMG_ACTION_LONE_V_JAUNT:
	    /*
	     * Take current loop on a jaunt from current (prev_vu) edge
	     * up to the vertex (vu) of this lone vertex loop,
	     * and back again.
	     * This only happens in "IN" state.
	     */
	    prev_vu = rs->vu[pos-1];
	    NMG_CK_VERTEXUSE(prev_vu);

	    if (*prev_vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		/* Both prev and current are lone vertex loops */
		rs->vu[pos] = nmg_join_2singvu_loops(prev_vu, vu);
		/* Set orientation */
		lu = nmg_find_lu_of_vu(rs->vu[pos]);
		NMG_CK_LOOPUSE(lu);
		/* If state is IN, this is a "crack" loop */
		nmg_set_lu_orientation(lu, old_state==NMG_STATE_IN);
	    } else {
		rs->vu[pos] = nmg_join_singvu_loop(prev_vu, vu);
	    }

	    /* We know edge geom is null, make it be the isect line */
	    first_new_eu = BU_LIST_PPREV_CIRC(edgeuse, rs->vu[pos]->up.eu_p);
	    nmg_edge_geom_isect_line(first_new_eu, rs, "LONE_V_JAUNT, new edge");

	    /* Fusing to this new edge will happen in nmg_face_cutjoin() */

	    /* Recompute loop geometry.  Bounding box may have expanded */
	    nmg_loop_a(nmg_find_lu_of_vu(rs->vu[pos])->l_p, rs->tol);

	    if (nmg_debug&NMG_DEBUG_FCUT) {
		bu_log("After LONE_V_JAUNT, the final loop:\n");
		nmg_pr_lu_briefly(nmg_find_lu_of_vu(rs->vu[pos]), (char *)0);
		nmg_plot_lu_ray(nmg_find_lu_of_vu(rs->vu[pos]),
				rs->vu[0], rs->vu[rs->nvu-1], rs->left, vlfree);
	    }
	    break;
	case NMG_ACTION_CUTJOIN:
	    /*
	     * Cut loop into two, or join two into one.
	     * The operation happens between the previous vu
	     * and the current one.
	     * If the two vu's are part of the same loop,
	     * then cut the loop into two, otherwise
	     * join the two loops into one.
	     */
	    lu = nmg_find_lu_of_vu(vu);
	    NMG_CK_LOOPUSE(lu);
	    fu = lu->up.fu_p;
	    NMG_CK_FACEUSE(fu);
	    prev_vu = rs->vu[pos-1];
	    NMG_CK_VERTEXUSE(prev_vu);
	    prev_lu = nmg_find_lu_of_vu(prev_vu);
	    NMG_CK_LOOPUSE(prev_lu);

	    /* See if there is an edge joining the 2 vertices already */
	    old_eu = nmg_findeu(prev_vu->v_p, vu->v_p, (struct shell *)NULL,
				(struct edgeuse *)NULL, 0);

	    if (lu->l_p == prev_lu->l_p) {
		int is_crack;

		/* Same loop, cut into two */
		is_crack = nmg_loop_is_a_crack(lu);
		if (nmg_debug&NMG_DEBUG_FCUT)
		    bu_log("Calling nmg_cut_loop(prev_vu=%p, vu=%p) is_crack=%d, old_eu=%p\n",
			   (void *)prev_vu, (void *)vu, is_crack, (void *)old_eu);
		if (prev_vu->v_p == vu->v_p) {
		    /* The loop touches itself already */
		    lu = nmg_split_lu_at_vu(prev_lu, prev_vu);
		} else {
		    lu = nmg_cut_loop(prev_vu, vu, vlfree);

		    /* New edge has been created between 2 verts, fuse */
		    /* first_new_eu starts at vu, ends at prev_vu */
		    /* second_new_eu starts at prev_vu, ends at vu */
		    first_new_eu = BU_LIST_PPREV_CIRC(edgeuse, &prev_vu->up.eu_p->l);
		    second_new_eu = BU_LIST_PPREV_CIRC(edgeuse, &vu->up.eu_p->l);
		    NMG_CK_EDGEUSE(first_new_eu);
		    NMG_CK_EDGEUSE(second_new_eu);
		    if (first_new_eu->vu_p->v_p != vu->v_p || first_new_eu->eumate_p->vu_p->v_p != prev_vu->v_p) {
			nmg_pr_vu_briefly(prev_vu, 0);
			nmg_pr_vu_briefly(vu, 0);
			nmg_pr_eu_endpoints(first_new_eu, 0);
			nmg_pr_eu_endpoints(second_new_eu, 0);
			if (old_eu) nmg_pr_eu_endpoints(old_eu, 0);
			bu_bomb("nmg_face_state_transition() cut loop bafflement\n");
		    }
		    /* Fuse new edge to intersect line, old edge */
		    nmg_edge_geom_isect_line(first_new_eu, rs, "CUTJOIN, new edge after loop cut");
		    if (old_eu) nmg_radial_join_eu(old_eu, first_new_eu, rs->tol);
		}

		nmg_loop_a(lu->l_p, rs->tol);
		nmg_loop_a(prev_lu->l_p, rs->tol);

		nmg_lu_reorient(lu);
		nmg_lu_reorient(prev_lu);

		if (nmg_debug&NMG_DEBUG_FCUT) {
		    bu_log("After CUT, the final loop:\n");
		    nmg_pr_lu_briefly(nmg_find_lu_of_vu(rs->vu[pos]), (char *)0);
		    nmg_plot_lu_ray(nmg_find_lu_of_vu(rs->vu[pos]),
				    rs->vu[0], rs->vu[rs->nvu-1], rs->left, vlfree);
		}
		break;
	    }
	    /*
	     * prev_vu and vu are in different loops,
	     * join the two loops into one loop.
	     * No edgeuses are deleted at this stage,
	     * so some "snakes" may appear in the process.
	     */
	    if (nmg_debug&NMG_DEBUG_FCUT) {
		bu_log("nmg_face_state_transition() joining 2 loops, prev_vu=%p, vu=%p, old_eu=%p\n",
		       (void *)prev_vu, (void *)vu, (void *)old_eu);
	    }

	    if (*prev_vu->up.magic_p == NMG_LOOPUSE_MAGIC ||
		*vu->up.magic_p == NMG_LOOPUSE_MAGIC) {
		/* One (or both) is a loop of a single vertex */
		/* This is the special boolean vertex marker */

		/* See if there is an existing edge between
		 * the two vertices.
		 */

		if (*prev_vu->up.magic_p == NMG_LOOPUSE_MAGIC &&
		    *vu->up.magic_p != NMG_LOOPUSE_MAGIC) {
		    if (nmg_debug&NMG_DEBUG_FCUT)
			bu_log("\tprev_vu is a vertex loop\n");
		    /* if prev_vu is geometrically on an edge that goes through vu,
		     * then split that edge at prev_vu */
		    rs->vu[pos-1] = nmg_join_singvu_loop(vu, prev_vu);
		} else if (*vu->up.magic_p == NMG_LOOPUSE_MAGIC &&
			   *prev_vu->up.magic_p != NMG_LOOPUSE_MAGIC) {
		    if (nmg_debug&NMG_DEBUG_FCUT)
			bu_log("\tvu is a vertex loop\n");
		    /* if vu is geometrically on an edge that goes through prev_vu,
		     * then split that edge at vu */
		    rs->vu[pos] = nmg_join_singvu_loop(prev_vu, vu);
		} else {
		    /* Both are loops of single vertex */
		    if (nmg_debug&NMG_DEBUG_FCUT)
			bu_log("\tprev_vu and vu are vertex loops\n");
		    vu = rs->vu[pos] = nmg_join_2singvu_loops(prev_vu, vu);
		    /* Set orientation */
		    lu = nmg_find_lu_of_vu(vu);
		    NMG_CK_LOOPUSE(lu);
		    nmg_set_lu_orientation(lu, old_state==NMG_STATE_IN);
		}
	    } else {
		rs->vu[pos] = nmg_join_2loops(prev_vu, vu);
	    }

	    /* XXX If an edge has been built between prev_vu and vu,
	     * force its geometry to lie on the ray.
	     */
	    vu = rs->vu[pos];
	    lu = nmg_find_lu_of_vu(vu);
	    NMG_CK_LOOPUSE(lu);
	    prev_vu = rs->vu[pos-1];
	    eu = prev_vu->up.eu_p;
	    NMG_CK_EDGEUSE(eu);
	    first_new_eu = BU_LIST_PPREV_CIRC(edgeuse, rs->vu[pos]->up.eu_p);
	    NMG_CK_EDGEUSE(first_new_eu);
	    if (eu == first_new_eu) {
		nmg_edge_geom_isect_line(first_new_eu, rs, "CUTJOIN new edge");
		if (old_eu) nmg_radial_join_eu(old_eu, eu, rs->tol);
	    }

	    /* Recompute loop geometry.  Bounding box may have expanded */
	    nmg_loop_a(lu->l_p, rs->tol);

	    if (nmg_debug&NMG_DEBUG_FCUT) {
		bu_log("After JOIN, the final loop:\n");
		nmg_pr_lu_briefly(lu, (char *)0);
		nmg_plot_lu_ray(lu, rs->vu[0], rs->vu[rs->nvu-1], rs->left, vlfree);
	    }
	    break;
    }

    rs->state = new_state;

    if (nmg_debug & NMG_DEBUG_VERIFY) {
	/* Verify both faces are still OK */
	nmg_vfu(&rs->fu1->s_p->fu_hd, rs->fu1->s_p);
	nmg_vfu(&rs->fu2->s_p->fu_hd, rs->fu2->s_p);
	nmg_fu_touchingloops(rs->fu1);
	nmg_fu_touchingloops(rs->fu2);
    }

    if (nmg_debug&NMG_DEBUG_FCUT) {
	bu_log("nmg_face_state_transition(vu %p, pos=%d) END\n",
	       (void *)rs->vu[pos], pos);
    }
    if (rs->eg_p) NMG_CK_EDGE_G_LSEG(rs->eg_p);
    return 0;
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
