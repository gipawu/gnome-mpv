/*
 * Copyright (c) 2015-2016 gnome-mpv
 *
 * This file is part of GNOME MPV.
 *
 * GNOME MPV is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GNOME MPV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNOME MPV.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPRIS_H
#define MPRIS_H

#include <gio/gio.h>
#include <glib.h>

#include "gmpv_application.h"

typedef struct gmpv_mpris gmpv_mpris;
typedef struct gmpv_mpris_prop gmpv_mpris_prop;

struct gmpv_mpris
{
	GmpvApplication* gmpv_ctx;
	guint name_id;
	guint base_reg_id;
	guint player_reg_id;
	gulong shutdown_sig_id;
	gulong *base_sig_id_list;
	gulong *player_sig_id_list;
	gdouble pending_seek;
	GHashTable *base_prop_table;
	GHashTable *player_prop_table;
	GDBusConnection *session_bus_conn;
};

struct gmpv_mpris_prop
{
	gchar *name;
	GVariant *value;
};

void gmpv_mpris_emit_prop_changed(	gmpv_mpris *inst,
					const gchar *iface_name,
					const gmpv_mpris_prop *prop_list );
GVariant *gmpv_mpris_build_g_variant_string_array(const gchar** list);
void gmpv_mpris_init(GmpvApplication *gmpv_ctx);

#endif
