/*
 * Devious: UPNP Control Point for Maemo 5
 *
 * Copyright (C) 2009 Kyle Cronan
 *
 * Author: Kyle Cronan <kyle@pbx.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef INCLUDED_DEVIOUS_H
#define INCLUDED_DEVIOUS_H 1

#define APPLICATION_NAME "Devious"
#define ROW_HEIGHT 75
#define ICON_WIDTH 90
#define ICON_XALIGN 0.6
#define MEDIA_RENDERER "urn:schemas-upnp-org:device:MediaRenderer:*"
#define MEDIA_SERVER "urn:schemas-upnp-org:device:MediaServer:*"
#define MAX_BROWSE 64
#define CONTENT_DIR "urn:schemas-upnp-org:service:ContentDirectory"

struct proxy {
    GtkTreeRowReference *row;
    GUPnPDeviceProxy *proxy;
    char *name;
};

struct proxy_set {
    GHashTable *renderers;
    GHashTable *servers;
    GtkListStore *renderer_list;
    GtkListStore *server_list;
    GdkPixbuf *icon;
};

struct browse_data {
    GUPnPServiceProxy *content_dir;
    gchar *id;
    guint32 starting_index;
    GtkListStore *list;
};

enum {
    COL_ICON = 0,
    COL_LABEL,
    COL_ID,
    NUM_COLS
};

void browse(GUPnPServiceProxy *content_dir, const char *container_id,
            guint32 starting_index, guint32 requested_count,
            GtkListStore *list);

void set_panarea_padding(GtkWidget *child, gpointer data);

GtkWidget *new_selector(GtkListStore *list);

GtkWidget *artists_window(struct proxy *server, char *artist);

void view_select(HildonTouchSelector *selector, gint column, gpointer data);

GtkWidget *server_window(struct proxy *server, GtkListStore **list);

void server_select(HildonTouchSelector *selector, gint column, gpointer data);

GtkWidget *main_window(HildonProgram *program, struct proxy_set *proxy_set);

void add_renderer(GUPnPDeviceProxy *proxy, struct proxy_set *proxy_set);

void add_server(GUPnPDeviceProxy *proxy, struct proxy_set *proxy_set);

void remove_server(GUPnPDeviceProxy *proxy, struct proxy_set *proxy_set);

void device_proxy_available(GUPnPControlPoint *cp,
                            GUPnPDeviceProxy *proxy, gpointer user_data);

GUPnPContext *init_upnp(struct proxy_set *proxy_set);

#endif /* INCLUDED_DEVIOUS_H */
