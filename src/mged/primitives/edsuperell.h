/*                      E D S U P E R E L L . H
 * BRL-CAD
 *
 * Copyright (c) 1985-2024 United States Government as represented by
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
/** @file mged/edsuperell.h
 */

#ifndef EDSUPERELL_H
#define EDSUPERELL_H

#include "common.h"
#include "vmath.h"
#include "raytrace.h"
#include "mged.h"

#define MENU_SUPERELL_SCALE_A	113
#define MENU_SUPERELL_SCALE_B	114
#define MENU_SUPERELL_SCALE_C	115
#define MENU_SUPERELL_SCALE_ABC	116

struct menu_item *
mged_superell_menu_item(const struct bn_tol *tol);

void
mged_superell_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *tol,
	fastf_t base2local);

void menu_superell_scale_a(struct mged_state *s);
void menu_superell_scale_b(struct mged_state *s);
void menu_superell_scale_c(struct mged_state *s);
void menu_superell_scale_abc(struct mged_state *s);

#endif  /* EDSUPERELL_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
