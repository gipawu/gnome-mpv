/*
 * Copyright (c) 2014-2016 gnome-mpv
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

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <gdk/gdk.h>

#include "gmpv_def.h"
#include "gmpv_menu.h"
#include "gmpv_application.h"
#include "gmpv_playlist_widget.h"
#include "gmpv_main_window.h"
#include "gmpv_header_bar.h"
#include "gmpv_control_box.h"
#include "gmpv_video_area.h"

enum
{
	PROP_0,
	PROP_PLAYLIST,
	N_PROPERTIES
};

struct _GmpvMainWindow
{
	GtkApplicationWindow parent_instance;
	GmpvPlaylist *playlist_store;
	gint width_offset;
	gint height_offset;
	gint resize_target[2];
	gboolean csd;
	gboolean fullscreen;
	gboolean playlist_visible;
	gboolean playlist_first_toggle;
	gboolean pre_fs_playlist_visible;
	gint playlist_width;
	guint timeout_tag;
	GtkWidget *header_bar;
	GtkWidget *main_box;
	GtkWidget *vid_area_paned;
	GtkWidget *vid_area;
	GtkWidget *control_box;
	GtkWidget *playlist;
};

struct _GmpvMainWindowClass
{
	GtkApplicationWindowClass parent_class;
};

static void gmpv_main_window_constructed(GObject *object);
static void gmpv_main_window_set_property(	GObject *object,
						guint property_id,
						const GValue *value,
						GParamSpec *pspec );
static void gmpv_main_window_get_property(	GObject *object,
						guint property_id,
						GValue *value,
						GParamSpec *pspec );
static void resize_video_area_finalize(	GtkWidget *widget,
					GdkRectangle *allocation,
					gpointer data );
static gboolean resize_to_target(gpointer data);
static void set_window_pos(GmpvMainWindow *wnd, const GmpvGeometry *geom);

G_DEFINE_TYPE(GmpvMainWindow, gmpv_main_window, GTK_TYPE_APPLICATION_WINDOW)

static void gmpv_main_window_constructed(GObject *object)
{
	GmpvMainWindow *self = GMPV_MAIN_WINDOW(object);

	self->playlist = gmpv_playlist_widget_new(self->playlist_store);

	gtk_widget_show_all(self->playlist);
	gtk_widget_hide(self->playlist);
	gtk_widget_set_no_show_all(self->playlist, TRUE);

	gtk_widget_show_all(self->control_box);
	gtk_widget_hide(self->control_box);
	gtk_widget_set_no_show_all(self->control_box, TRUE);

	gtk_paned_pack1(	GTK_PANED(self->vid_area_paned),
				self->vid_area,
				TRUE,
				TRUE );
	gtk_paned_pack2(	GTK_PANED(self->vid_area_paned),
				self->playlist,
				FALSE,
				FALSE );

	G_OBJECT_CLASS(gmpv_main_window_parent_class)->constructed(object);
}

static void gmpv_main_window_set_property(	GObject *object,
						guint property_id,
						const GValue *value,
						GParamSpec *pspec )
{
	GmpvMainWindow *self = GMPV_MAIN_WINDOW(object);

	if(property_id == PROP_PLAYLIST)
	{
		self->playlist_store = g_value_get_pointer(value);

	}
	else
	{
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
	}
}

static void gmpv_main_window_get_property(	GObject *object,
						guint property_id,
						GValue *value,
						GParamSpec *pspec )
{
	GmpvMainWindow *self = GMPV_MAIN_WINDOW(object);

	if(property_id == PROP_PLAYLIST)
	{
		g_value_set_pointer(value, self->playlist_store);
	}
	else
	{
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
	}
}

static void resize_video_area_finalize(	GtkWidget *widget,
					GdkRectangle *allocation,
					gpointer data )
{
	GmpvMainWindow *wnd = data;
	GdkScreen *screen = gdk_screen_get_default();
	gint screen_width = gdk_screen_get_width(screen);
	gint screen_height = gdk_screen_get_height(screen);
	gint width = allocation->width;
	gint height = allocation->height;
	gint target_width = wnd->resize_target[0];
	gint target_height = wnd->resize_target[1];

	g_signal_handlers_disconnect_by_func
		(widget, resize_video_area_finalize, data);

	/* Adjust resize offset */
	if((width != target_width || height != target_height)
	&& (target_width < screen_width && target_height < screen_height)
	&& !gtk_window_is_maximized(GTK_WINDOW(wnd))
	&& !wnd->fullscreen)
	{
		wnd->width_offset += target_width-width;
		wnd->height_offset += target_height-height;

		g_idle_add(resize_to_target, wnd);
	}
}

static gboolean resize_to_target(gpointer data)
{
	GmpvMainWindow *wnd = data;
	gint target_width = wnd->resize_target[0];
	gint target_height = wnd->resize_target[1];

	gtk_window_resize(	GTK_WINDOW(wnd),
				target_width+wnd->width_offset,
				target_height+wnd->height_offset );

	/* Prevent graphical glitches that appear when calling
	 * gmpv_main_window_resize_video_area() with the current size as the
	 * target size.
	 */
	gmpv_playlist_widget_queue_draw(GMPV_PLAYLIST_WIDGET(wnd->playlist));

	return FALSE;
}

static void set_window_pos(GmpvMainWindow *wnd, const GmpvGeometry *geom)
{
	GmpvControlBox *box;
	GdkScreen *screen;
	gint screen_dim[2];
	gint window_dim[2];
	gboolean flip[2];
	GValue geom_pos[2];
	gint64 pos[2];

	g_assert(geom);

	box = gmpv_main_window_get_control_box(wnd);
	screen = gdk_screen_get_default();
	screen_dim[0] = gdk_screen_get_width(screen);
	screen_dim[1] = gdk_screen_get_height(screen);
	window_dim[0] = 0;
	window_dim[1] = 0;
	geom_pos[0] = geom->x;
	geom_pos[1] = geom->y;
	flip[0] = geom->flags&GMPV_GEOMETRY_FLIP_X;
	flip[1] = geom->flags&GMPV_GEOMETRY_FLIP_Y;
	pos[0] = 0;
	pos[1] = 0;

	gtk_window_get_size(GTK_WINDOW(wnd), &window_dim[0], &window_dim[1]);

	for(gint i = 0; i < 2; i++)
	{
		GType type = G_VALUE_TYPE(&geom_pos[i]);

		if(type == G_TYPE_INT64)
		{
			gint64 value =	g_value_get_int64(&geom_pos[i]);

			pos[i] =	flip[i]?
					screen_dim[i]-window_dim[i]-value:
					value;
		}
		else if(type == G_TYPE_DOUBLE)
		{
			gdouble value =	g_value_get_double(&geom_pos[i]);

			pos[i] = (gint64)(value*(screen_dim[i]-window_dim[i]));
		}
		else
		{
			g_assert_not_reached();
		}
	}

	/* Adjust the y-position to account for the height of
	 * the control box.
	 */
	if(flip[1] && gtk_widget_get_visible(GTK_WIDGET(box)))
	{
		pos[1] -=	gtk_widget_get_allocated_height
				(GTK_WIDGET(box));
	}

	gtk_window_move(GTK_WINDOW(wnd), (gint)pos[0], (gint)pos[1]);
}

static void gmpv_main_window_class_init(GmpvMainWindowClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GParamSpec *pspec = NULL;

	obj_class->constructed = gmpv_main_window_constructed;
	obj_class->set_property = gmpv_main_window_set_property;
	obj_class->get_property = gmpv_main_window_get_property;

	pspec = g_param_spec_pointer
		(	"playlist",
			"Playlist",
			"Playlist object used to store playlist items",
			G_PARAM_CONSTRUCT_ONLY|G_PARAM_READWRITE );

	g_object_class_install_property(obj_class, PROP_PLAYLIST, pspec);
}

static void gmpv_main_window_init(GmpvMainWindow *wnd)
{
	wnd->csd = FALSE;
	wnd->fullscreen = FALSE;
	wnd->playlist_visible = FALSE;
	wnd->pre_fs_playlist_visible = FALSE;
	wnd->playlist_width = PLAYLIST_DEFAULT_WIDTH;
	wnd->timeout_tag = 0;
	wnd->header_bar = gmpv_header_bar_new();
	wnd->main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	wnd->vid_area_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
	wnd->vid_area = gmpv_video_area_new();
	wnd->control_box = gmpv_control_box_new();

	wnd->playlist_first_toggle = TRUE;
	wnd->width_offset = 0;
	wnd->height_offset = 0;

	gtk_widget_add_events(	wnd->vid_area,
				GDK_ENTER_NOTIFY_MASK
				|GDK_LEAVE_NOTIFY_MASK );

	gtk_window_set_title(GTK_WINDOW(wnd), g_get_application_name());

	gtk_paned_set_position(	GTK_PANED(wnd->vid_area_paned),
				MAIN_WINDOW_DEFAULT_WIDTH
				-PLAYLIST_DEFAULT_WIDTH );

	gtk_window_set_default_size(	GTK_WINDOW(wnd),
					MAIN_WINDOW_DEFAULT_WIDTH,
					MAIN_WINDOW_DEFAULT_HEIGHT );

	gtk_box_pack_start
		(GTK_BOX(wnd->main_box), wnd->vid_area_paned, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(wnd->main_box), wnd->control_box);
	gtk_container_add(GTK_CONTAINER(wnd), wnd->main_box);
}

GtkWidget *gmpv_main_window_new(GmpvApplication *app, GmpvPlaylist *playlist)
{
	return GTK_WIDGET(g_object_new(	gmpv_main_window_get_type(),
					"application", app,
					"playlist", playlist,
					NULL ));
}

GmpvPlaylistWidget *gmpv_main_window_get_playlist(GmpvMainWindow *wnd)
{
	return GMPV_PLAYLIST_WIDGET(wnd->playlist);
}

GmpvControlBox *gmpv_main_window_get_control_box(GmpvMainWindow *wnd)
{
	return GMPV_CONTROL_BOX(wnd->control_box);
}

GmpvVideoArea *gmpv_main_window_get_video_area(GmpvMainWindow *wnd)
{
	return GMPV_VIDEO_AREA(wnd->vid_area);
}

void gmpv_main_window_set_fullscreen(GmpvMainWindow *wnd, gboolean fullscreen)
{
	if(fullscreen != wnd->fullscreen)
	{
		GmpvVideoArea *vid_area = GMPV_VIDEO_AREA(wnd->vid_area);
		GtkContainer *main_box = GTK_CONTAINER(wnd->main_box);

		if(fullscreen)
		{
			g_object_ref(wnd->control_box);
			gtk_container_remove(main_box, wnd->control_box);
			gmpv_video_area_set_control_box
				(vid_area, wnd->control_box);
			g_object_unref(wnd->control_box);

			gmpv_video_area_set_fullscreen_state(vid_area, TRUE);
			gtk_window_fullscreen(GTK_WINDOW(wnd));
			gtk_window_present(GTK_WINDOW(wnd));

			wnd->pre_fs_playlist_visible = wnd->playlist_visible;
		}
		else
		{
			gmpv_video_area_set_fullscreen_state(vid_area, FALSE);

			g_object_ref(wnd->control_box);
			gmpv_video_area_set_control_box(vid_area, NULL);
			gtk_container_add(main_box, wnd->control_box);
			g_object_unref(wnd->control_box);

			gtk_window_unfullscreen(GTK_WINDOW(wnd));

			wnd->playlist_visible = wnd->pre_fs_playlist_visible;
		}

		if(!gmpv_main_window_get_csd_enabled(wnd))
		{
			gtk_application_window_set_show_menubar
				(GTK_APPLICATION_WINDOW(wnd), !fullscreen);
		}

		gmpv_video_area_set_fullscreen_state(vid_area, fullscreen);
		gtk_widget_set_visible(	wnd->playlist,
					!fullscreen &&
					wnd->pre_fs_playlist_visible );

		wnd->fullscreen = fullscreen;
	}
}

gboolean gmpv_main_window_get_fullscreen(GmpvMainWindow *wnd)
{
	return wnd->fullscreen;
}

void gmpv_main_window_toggle_fullscreen(GmpvMainWindow *wnd)
{
	gmpv_main_window_set_fullscreen(wnd, !wnd->fullscreen);
}

void gmpv_main_window_reset(GmpvMainWindow *wnd)
{
	GmpvPlaylist *store =	gmpv_playlist_widget_get_store
				(GMPV_PLAYLIST_WIDGET(wnd->playlist));

	gtk_window_set_title(GTK_WINDOW(wnd), g_get_application_name());
	gmpv_control_box_reset(GMPV_CONTROL_BOX(wnd->control_box));
	gmpv_playlist_set_indicator_pos(store, -1);
}

void gmpv_main_window_save_state(GmpvMainWindow *wnd)
{
	GSettings *settings;
	gint width;
	gint height;
	gint handle_pos;
	gdouble volume;
	gboolean controls_visible;

	settings = g_settings_new(CONFIG_WIN_STATE);
	handle_pos = gtk_paned_get_position(GTK_PANED(wnd->vid_area_paned));
	volume = gmpv_control_box_get_volume(GMPV_CONTROL_BOX(wnd->control_box));
	controls_visible = gtk_widget_get_visible(wnd->control_box);

	gtk_window_get_size(GTK_WINDOW(wnd), &width, &height);

	g_settings_set_int(settings, "width", width);
	g_settings_set_int(settings, "height", height);
	g_settings_set_double(settings, "volume", volume);
	g_settings_set_boolean(settings, "show-controls", controls_visible);
	g_settings_set_boolean(settings, "show-playlist", wnd->playlist_visible);

	if(gmpv_main_window_get_playlist_visible(wnd))
	{
		g_settings_set_int(	settings,
					"playlist-width",
					width-handle_pos );
	}
	else
	{
		g_settings_set_int(	settings,
					"playlist-width",
					wnd->playlist_width );
	}

	g_clear_object(&settings);
}

void gmpv_main_window_load_state(GmpvMainWindow *wnd)
{
	if(!gtk_widget_get_realized(GTK_WIDGET(wnd)))
	{
		GApplication *gapp = g_application_get_default();
		GSettings *settings = g_settings_new(CONFIG_WIN_STATE);
		GAction *playlist_action =	g_action_map_lookup_action
					(G_ACTION_MAP(gapp), "toggle-playlist");
		GAction *controls_action =	g_action_map_lookup_action
					(G_ACTION_MAP(gapp), "toggle-controls");
		gint width = g_settings_get_int(settings, "width");
		gint height = g_settings_get_int(settings, "height");
		gint handle_pos;
		gboolean controls_visible;
		gdouble volume;

		wnd->playlist_width
			= g_settings_get_int(settings, "playlist-width");
		wnd->playlist_visible
			= g_settings_get_boolean(settings, "show-playlist");
		controls_visible
			= g_settings_get_boolean(settings, "show-controls");
		volume = g_settings_get_double(settings, "volume");
		handle_pos = width - wnd->playlist_width;

		gmpv_control_box_set_volume
			(GMPV_CONTROL_BOX(wnd->control_box), volume);
		gtk_widget_set_visible(wnd->control_box, controls_visible);
		gtk_widget_set_visible(wnd->playlist, wnd->playlist_visible);
		gtk_window_resize(GTK_WINDOW(wnd), width, height);
		gtk_paned_set_position
			(GTK_PANED(wnd->vid_area_paned), handle_pos);

		g_action_change_state
			(controls_action, g_variant_new_boolean(controls_visible));
		g_action_change_state
			(playlist_action, g_variant_new_boolean(wnd->playlist_visible));

		g_clear_object(&settings);
	}
	else
	{
		g_critical(	"Attempted to call gmpv_main_window_load_state() "
				"on realized window" );
	}
}

void gmpv_main_window_set_geometry(	GmpvMainWindow *wnd,
					const GmpvGeometry *geom )
{
	if(geom)
	{
		if(!(geom->flags&GMPV_GEOMETRY_IGNORE_DIM))
		{
			gmpv_main_window_resize_video_area
				(wnd, (gint)geom->width, (gint)geom->height);
		}

		if(!(geom->flags&GMPV_GEOMETRY_IGNORE_POS))
		{
			set_window_pos(wnd, geom);
		}
	}
}

void gmpv_main_window_update_track_list(	GmpvMainWindow *wnd,
						const GSList *audio_list,
						const GSList *video_list,
						const GSList *sub_list )
{
	if(gmpv_main_window_get_csd_enabled(wnd))
	{
		gmpv_header_bar_update_track_list
			(	GMPV_HEADER_BAR(wnd->header_bar),
				audio_list,
				video_list,
				sub_list );
		gmpv_video_area_update_track_list
			(	GMPV_VIDEO_AREA(wnd->vid_area),
				audio_list,
				video_list,
				sub_list );
	}
	else
	{
		GtkApplication *app;
		GMenu *menu;

		app = gtk_window_get_application(GTK_WINDOW(wnd));
		menu = G_MENU(gtk_application_get_menubar(app));

		if(menu)
		{
			g_menu_remove_all(menu);
			gmpv_menu_build_full
				(menu, audio_list, video_list, sub_list);
		}
	}
}

void gmpv_main_window_resize_video_area(	GmpvMainWindow *wnd,
						gint width,
						gint height )
{
	g_signal_connect(	wnd->vid_area,
				"size-allocate",
				G_CALLBACK(resize_video_area_finalize),
				wnd );

	wnd->resize_target[0] = width;
	wnd->resize_target[1] = height;
	resize_to_target(wnd);

	/* The size may not change, so this is needed to ensure that
	 * resize_video_area_finalize() will be called so that the event handler
	 * will be disconnected.
	 */
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_queue_allocate(wnd->vid_area);
#else
	gtk_widget_queue_resize(wnd->vid_area);
#endif
}

void gmpv_main_window_enable_csd(GmpvMainWindow *wnd)
{
	wnd->csd = TRUE;
	wnd->playlist_width = PLAYLIST_DEFAULT_WIDTH;

	gtk_paned_set_position(	GTK_PANED(wnd->vid_area_paned),
				MAIN_WINDOW_DEFAULT_WIDTH
				-PLAYLIST_DEFAULT_WIDTH );

	gtk_window_set_titlebar(GTK_WINDOW(wnd), wnd->header_bar);
	gtk_window_set_title(GTK_WINDOW(wnd), g_get_application_name());
}

gboolean gmpv_main_window_get_csd_enabled(GmpvMainWindow *wnd)
{
	return wnd->csd;
}

void gmpv_main_window_set_playlist_visible(	GmpvMainWindow *wnd,
						gboolean visible )
{
	if(visible != wnd->playlist_visible && !wnd->fullscreen)
	{
		gint handle_pos;
		gint width;
		gint height;

		handle_pos =	gtk_paned_get_position
				(GTK_PANED(wnd->vid_area_paned));

		gtk_window_get_size(GTK_WINDOW(wnd), &width, &height);

		if(wnd->playlist_first_toggle && visible)
		{
			gint new_pos = width - wnd->playlist_width;

			if(gtk_check_version(3, 20, 0))
			{
				/* Workaround for window sizing bug affecting
				 * GTK+ < 3.20
				 */
				new_pos -= 52;
			}

			gtk_paned_set_position(	GTK_PANED(wnd->vid_area_paned),
						new_pos );
		}
		else if(!visible)
		{
			wnd->playlist_width = width-handle_pos;
		}

		wnd->playlist_visible = visible;
		gtk_widget_set_visible(wnd->playlist, visible);

		wnd->playlist_first_toggle = FALSE;
	}
}

gboolean gmpv_main_window_get_playlist_visible(GmpvMainWindow *wnd)
{
	return gtk_widget_get_visible(GTK_WIDGET(wnd->playlist));
}
