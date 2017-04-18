/*
 * Copyright (c) 2015-2017 gnome-mpv
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
#include "gmpv_mpris_module.h"

typedef struct gmpv_mpris gmpv_mpris;
typedef struct gmpv_mpris_prop gmpv_mpris_prop;

struct gmpv_mpris
{
	GmpvApplication* gmpv_ctx;
	GmpvMprisModule *base;
	GmpvMprisModule *player;
	GmpvMprisModule *track_list;
	guint name_id;
	guint player_reg_id;
	gulong shutdown_sig_id;
	gulong *player_sig_id_list;
	gdouble pending_seek;
	GHashTable *player_prop_table;
	GDBusConnection *session_bus_conn;
};

GVariant *gmpv_mpris_build_g_variant_string_array(const gchar** list);
void gmpv_mpris_init(GmpvApplication *gmpv_ctx);

#endif
