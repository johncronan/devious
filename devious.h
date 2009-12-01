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
#define PLAY_WINDOW_TITLE "Now playing"
#define CHOOSE_TARGET "Choose target"
#define ROW_HEIGHT 75
#define ICON_WIDTH 90
#define ICON_XALIGN 0.6
#define MEDIA_RENDERER "urn:schemas-upnp-org:device:MediaRenderer:*"
#define MEDIA_SERVER "urn:schemas-upnp-org:device:MediaServer:*"
#define CONTENT_DIR "urn:schemas-upnp-org:service:ContentDirectory"
#define CONNECTION_MANAGER "urn:schemas-upnp-org:service:ConnectionManager"
#define AV_TRANSPORT "urn:schemas-upnp-org:service:AVTransport"
#define MAX_BROWSE 64

struct proxy {
    GtkTreeRowReference *row;
    GUPnPDeviceProxy *proxy;
    char *name;
    struct proxy_set *set;
    char **protocols;
    GtkTreeRowReference *current_selection;
};

struct proxy_set {
    GHashTable *renderers;
    GHashTable *servers;
    GtkListStore *renderer_list;
    GtkListStore *server_list;
    GdkPixbuf *icon;
    HildonPickerButton *renderer_picker;
    GUPnPDIDLLiteParser *didl_parser;
};

struct browse_data {
    GUPnPServiceProxy *content_dir;
    gchar *id;
    guint32 starting_index;
    GtkListStore *list;
    GUPnPDIDLLiteParser *didl_parser;
};

enum {
    COL_ICON = 0,
    COL_LABEL,
    COL_ID,
    COL_CONTENT,
    COL_CONTAINER,
    NUM_COLS
};


void browse(GUPnPServiceProxy *content_dir, const char *container_id,
            guint32 starting_index, guint32 requested_count,
            GtkListStore *list, GUPnPDIDLLiteParser *didl_parser);

void set_panarea_padding(GtkWidget *child, gpointer data);

GtkWidget *new_selector(GtkListStore *list);

void content_select(HildonTouchSelector *selector, gint column, gpointer data);

GtkWidget *content_window(struct proxy *server, char *title,
                          GtkListStore **list);

void server_select(HildonTouchSelector *selector, gint column, gpointer data);

GtkWidget *main_window(HildonProgram *program, struct proxy_set *proxy_set);

void add_renderer(GUPnPDeviceProxy *proxy, struct proxy_set *proxy_set);

void add_server(GUPnPDeviceProxy *proxy, struct proxy_set *proxy_set);

void remove_server(GUPnPDeviceProxy *proxy, struct proxy_set *proxy_set);

void device_proxy_available(GUPnPControlPoint *cp,
                            GUPnPDeviceProxy *proxy, gpointer user_data);

void init_upnp(struct proxy_set *proxy_set);

#endif /* INCLUDED_DEVIOUS_H */
