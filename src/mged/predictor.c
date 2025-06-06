/*                     P R E D I C T O R . C
 * BRL-CAD
 *
 * Copyright (c) 1992-2025 United States Government as represented by
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
/** @file mged/predictor.c
 *
 * Put a predictor frame into view, as an aid to velocity-based
 * navigation through an MGED model.
 *
 * Inspired by the paper "Manipulating the Future: Predictor Based
 * Feedback for Velocity Control in Virtual Environment Navigation" by
 * Dale Chapman and Colin Ware, <cware@unb.ca>, in ACM SIGGRAPH
 * Computer Graphics Special Issue on 1992 Symposium on Interactive 3D
 * Graphics.
 *
 */

#include "common.h"

#include <string.h>
#include <math.h>

#include "vmath.h"
#include "bn.h"
#include "ged.h"

#include "./mged.h"
#include "./cmd.h"
#include "./mged_dm.h"

static void
init_trail(struct trail *tp)
{
    tp->t_cur_index = 0;
    tp->t_nused = 0;
}


/*
 * Add a new point to the end of the trail.
 */
static void
push_trail(struct trail *tp, fastf_t *pt)
{
    VMOVE(tp->t_pt[tp->t_cur_index], pt);
    if (tp->t_cur_index >= tp->t_nused) tp->t_nused++;
    tp->t_cur_index++;
    if (tp->t_cur_index >= MAX_TRAIL) tp->t_cur_index = 0;
}


/*
 * Draw from the most recently added points in two trails, as polygons.
 * Proceeds backwards.
 * t1 should be below (lower screen Y) t2.
 */
static void
poly_trail(struct mged_state *s, struct bu_list *vhead, struct trail *t1, struct trail *t2)
{
    int i1, i2;
    int todo = t1->t_nused;
    fastf_t *s1, *s2;
    vect_t right, up;
    vect_t norm;

    if (t2->t_nused < todo) todo = t2->t_nused;

    BU_LIST_INIT(vhead);
    if (t1->t_nused <= 0 || t1->t_nused <= 0) return;

    if ((i1 = t1->t_cur_index-1) < 0) i1 = t1->t_nused-1;
    if ((i2 = t2->t_cur_index-1) < 0) i2 = t2->t_nused-1;

    /* Get starting points, next to frame. */
    s1 = t1->t_pt[i1];
    s2 = t2->t_pt[i2];
    if ((--i1) < 0) i1 = t1->t_nused-1;
    if ((--i2) < 0) i2 = t2->t_nused-1;
    todo--;

    for (; todo > 0; todo--) {
	/* Go from s1 to s2 to t2->t_pt[i2] to t1->t_pt[i1] */
	VSUB2(up, s1, s2);
	VSUB2(right, t1->t_pt[i1], s1);
	VCROSS(norm, right, up);

	BV_ADD_VLIST(s->vlfree, vhead, norm, BV_VLIST_POLY_START);
	BV_ADD_VLIST(s->vlfree, vhead, s1, BV_VLIST_POLY_MOVE);
	BV_ADD_VLIST(s->vlfree, vhead, s2, BV_VLIST_POLY_DRAW);
	BV_ADD_VLIST(s->vlfree, vhead, t2->t_pt[i2], BV_VLIST_POLY_DRAW);
	BV_ADD_VLIST(s->vlfree, vhead, t1->t_pt[i1], BV_VLIST_POLY_DRAW);
	BV_ADD_VLIST(s->vlfree, vhead, s1, BV_VLIST_POLY_END);

	s1 = t1->t_pt[i1];
	s2 = t2->t_pt[i2];

	if ((--i1) < 0) i1 = t1->t_nused-1;
	if ((--i2) < 0) i2 = t2->t_nused-1;
    }
}


void
predictor_init(struct mged_state *s)
{
    int i;

    for (i = 0; i < NUM_TRAILS; ++i)
	init_trail(&s->mged_curr_dm->dm_trails[i]);
}


void
predictor_kill(struct mged_state *s)
{
    BV_FREE_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist);
    predictor_init(s);
}


#define TF_BORD 0.01
#define TF_X 0.14
#define TF_Y 0.07
#define TF_Z (1.0-0.15)	/* To prevent Z clipping of TF_X */

#define TF_VL(_m, _v) \
	{ vect_t edgevect_m; \
	MAT4X3VEC(edgevect_m, predictorXv2m, _v); \
	VADD2(_m, framecenter_m, edgevect_m); }

/*
 * Draw the frame itself as four polygons:
 * ABFE, HGCD, EILH, and JFGK.
 * The streamers will attach at edges AE, BF, GC, and HD.
 *
 *		D --------------- C
 *		|                 |
 *		H -L-----------K- G
 *		|  |           |  |
 *		|  |           |  |
 *		|  |           |  |
 *		E -I-----------J- F
 *		|                 |
 *		A --------------- B
 */
void
predictor_frame(struct mged_state *s)
{
    int i;
    int nframes;
    mat_t predictor;
    mat_t predictorXv2m;
    point_t m;		/* model coords */
    point_t mA, mB, mC, mD, mE, mF, mG, mH, mI, mJ, mK, mL;
    struct bu_list trail;
    point_t framecenter_m;
    point_t center_m;
    vect_t delta_v;
    vect_t right, up;
    vect_t norm;

    if (!view_state->vs_gvp)
	return;

    if (view_state->vs_gvp->k.vs_rateflag_rotate == 0 &&
	view_state->vs_gvp->k.vs_rateflag_tran == 0 &&
	view_state->vs_gvp->k.vs_rateflag_scale == 0) {
	predictor_kill(s);
	return;
    }

    BV_FREE_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist);

    /* Advance into the future */
    nframes = (int)(mged_variables->mv_predictor_advance / frametime);
    if (nframes < 1) nframes = 1;

    /* Build view2model matrix for the future time */
    MAT_IDN(predictor);
    for (i=0; i < nframes; i++) {
	bn_mat_mul2(view_state->vs_ModelDelta, predictor);
    }
    bn_mat_mul(predictorXv2m, predictor, view_state->vs_gvp->gv_view2model);
    MAT_DELTAS_GET_NEG(center_m, view_state->vs_gvp->gv_center);

    MAT4X3PNT(framecenter_m, predictor, center_m);

    /*
     * Draw the frame around the point framecenter_v.
     */

    /* Centering dot */
    VSETALL(delta_v, 0.0);
    TF_VL(m, delta_v);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, m, BV_VLIST_LINE_MOVE);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, m, BV_VLIST_LINE_DRAW);

    /* The exterior rectangle */
    VSET(delta_v, -TF_X, -TF_Y, 0.0);
    TF_VL(mA, delta_v);

    VSET(delta_v,  TF_X, -TF_Y, 0.0);
    TF_VL(mB, delta_v);

    VSET(delta_v,  TF_X,  TF_Y, 0.0);
    TF_VL(mC, delta_v);

    VSET(delta_v, -TF_X,  TF_Y, 0.0);
    TF_VL(mD, delta_v);

    /* The EFGH rectangle */
    VSET(delta_v, -TF_X, -TF_Y+TF_BORD, 0.0);
    TF_VL(mE, delta_v);

    VSET(delta_v,  TF_X, -TF_Y+TF_BORD, 0.0);
    TF_VL(mF, delta_v);

    VSET(delta_v,  TF_X,  TF_Y-TF_BORD, 0.0);
    TF_VL(mG, delta_v);

    VSET(delta_v, -TF_X,  TF_Y-TF_BORD, 0.0);
    TF_VL(mH, delta_v);

    /* The IJKL rectangle */
    VSET(delta_v, -TF_X+TF_BORD, -TF_Y+TF_BORD, 0.0);
    TF_VL(mI, delta_v);

    VSET(delta_v,  TF_X-TF_BORD, -TF_Y+TF_BORD, 0.0);
    TF_VL(mJ, delta_v);

    VSET(delta_v,  TF_X-TF_BORD,  TF_Y-TF_BORD, 0.0);
    TF_VL(mK, delta_v);

    VSET(delta_v, -TF_X+TF_BORD,  TF_Y-TF_BORD, 0.0);
    TF_VL(mL, delta_v);

    VSUB2(right, mB, mA);
    VSUB2(up, mD, mA);
    VCROSS(norm, right, up);
    VUNITIZE(norm);

    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, norm, BV_VLIST_POLY_START);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mA, BV_VLIST_POLY_MOVE);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mB, BV_VLIST_POLY_DRAW);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mF, BV_VLIST_POLY_DRAW);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mE, BV_VLIST_POLY_DRAW);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mA, BV_VLIST_POLY_END);

    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, norm, BV_VLIST_POLY_START);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mE, BV_VLIST_POLY_MOVE);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mI, BV_VLIST_POLY_DRAW);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mL, BV_VLIST_POLY_DRAW);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mH, BV_VLIST_POLY_DRAW);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mE, BV_VLIST_POLY_END);

    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, norm, BV_VLIST_POLY_START);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mH, BV_VLIST_POLY_MOVE);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mG, BV_VLIST_POLY_DRAW);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mC, BV_VLIST_POLY_DRAW);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mD, BV_VLIST_POLY_DRAW);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mH, BV_VLIST_POLY_END);

    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, norm, BV_VLIST_POLY_START);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mJ, BV_VLIST_POLY_MOVE);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mF, BV_VLIST_POLY_DRAW);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mG, BV_VLIST_POLY_DRAW);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mK, BV_VLIST_POLY_DRAW);
    BV_ADD_VLIST(s->vlfree, &s->mged_curr_dm->dm_p_vlist, mJ, BV_VLIST_POLY_END);

    push_trail(&s->mged_curr_dm->dm_trails[0], mA);
    push_trail(&s->mged_curr_dm->dm_trails[1], mB);
    push_trail(&s->mged_curr_dm->dm_trails[2], mC);
    push_trail(&s->mged_curr_dm->dm_trails[3], mD);

    push_trail(&s->mged_curr_dm->dm_trails[4], mE);
    push_trail(&s->mged_curr_dm->dm_trails[5], mF);
    push_trail(&s->mged_curr_dm->dm_trails[6], mG);
    push_trail(&s->mged_curr_dm->dm_trails[7], mH);

    /* Draw the trails */
    poly_trail(s, &trail, &s->mged_curr_dm->dm_trails[0], &s->mged_curr_dm->dm_trails[4]);
    BU_LIST_APPEND_LIST(&s->mged_curr_dm->dm_p_vlist, &trail);
    poly_trail(s, &trail, &s->mged_curr_dm->dm_trails[1], &s->mged_curr_dm->dm_trails[5]);
    BU_LIST_APPEND_LIST(&s->mged_curr_dm->dm_p_vlist, &trail);
    poly_trail(s, &trail, &s->mged_curr_dm->dm_trails[6], &s->mged_curr_dm->dm_trails[2]);
    BU_LIST_APPEND_LIST(&s->mged_curr_dm->dm_p_vlist, &trail);
    poly_trail(s, &trail, &s->mged_curr_dm->dm_trails[7], &s->mged_curr_dm->dm_trails[3]);
    BU_LIST_APPEND_LIST(&s->mged_curr_dm->dm_p_vlist, &trail);

    /* Done */
    MAT_IDN(view_state->vs_ModelDelta);
}


/*
 * Called from set.c when the predictor variables are modified.
 */
void
predictor_hook(const struct bu_structparse *UNUSED(sp), const char *UNUSED(c1), void *UNUSED(v1), const char *UNUSED(c2), void *data)
{
    struct mged_state *s = (struct mged_state *)data;
    MGED_CK_STATE(s);

    if (mged_variables->mv_predictor > 0)
	predictor_init(s);
    else
	predictor_kill(s);

    DMP_dirty = 1;
    dm_set_dirty(DMP, 1);
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
