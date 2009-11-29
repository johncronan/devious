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

/*
gcc -pthread -L/usr/X11R6/lib -lm
`pkg-config --libs glib-2.0 gthread-2.0 hildon-1 gupnp-1.0 gupnp-av-1.0`
-O0 -g -Wall -I/usr/include -I/usr/X11R6/include
`pkg-config --cflags glib-2.0 gconf-2.0 gthread-2.0 hildon-1 gupnp-1.0 gupnp-av-1.0`
-o devious devious.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include <libgupnp/gupnp-control-point.h>
#include <libgupnp-av/gupnp-av.h>

#define APPLICATION_NAME "Devious"
#define ROW_HEIGHT 75
#define ICON_WIDTH 90
#define ICON_XALIGN 0.6
#define MEDIA_RENDERER "urn:schemas-upnp-org:device:MediaRenderer:*"
#define MEDIA_SERVER "urn:schemas-upnp-org:device:MediaServer:*"

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

enum {
    COL_ICON = 0,
    COL_LABEL,
    COL_ID,
    NUM_COLS
};


void set_panarea_padding(GtkWidget *child, gpointer data)
{
    void set_child_padding(GtkWidget *child, gpointer user_data)
    {
        GtkBox *box = GTK_BOX(user_data);
        gboolean expand, fill;
        guint pad;
        GtkPackType pack;
        
        gtk_box_query_child_packing(box, child, &expand, &fill, &pad, &pack);
        gtk_box_set_child_packing(box, child, expand, fill, 0, pack);
    }

    if (GTK_IS_CONTAINER(child))
        gtk_container_forall(GTK_CONTAINER(child), set_child_padding, child);
}

GtkWidget *new_selector(GtkListStore *list)
{
    GtkWidget *selector = hildon_touch_selector_new();
    
    HildonTouchSelectorColumn *column =
        hildon_touch_selector_append_column(HILDON_TOUCH_SELECTOR(selector),
                                            GTK_TREE_MODEL(list), NULL, NULL);
    g_object_unref(list);
    hildon_touch_selector_column_set_text_column(column, 1);

    hildon_touch_selector_set_hildon_ui_mode(HILDON_TOUCH_SELECTOR(selector),
                                             HILDON_UI_MODE_NORMAL);
    GtkCellRenderer *renderer;
    renderer = gtk_cell_renderer_pixbuf_new();
    g_object_set(renderer, "xalign", ICON_XALIGN, NULL);
    gtk_cell_renderer_set_fixed_size(renderer, ICON_WIDTH, ROW_HEIGHT);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(column), renderer,
                                   "pixbuf", 0, NULL);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(column), renderer,
                                   "text", 1, NULL);

    gtk_container_forall(GTK_CONTAINER(selector), set_panarea_padding, NULL);

    gtk_widget_show_all(selector);

    return selector;
}

GtkWidget *artists_window(struct proxy *server, char *artist)
{
    GtkWidget *window = hildon_stackable_window_new();
    if (!artist)
        gtk_window_set_title(GTK_WINDOW(window), "Artists");

    GError *error = NULL;
    GdkPixbuf *icon = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
                                               "general_folder",
                                               HILDON_ICON_PIXEL_SIZE_FINGER,
                                               0, &error);
    GtkListStore *list = gtk_list_store_new(NUM_COLS-1,
                                            GDK_TYPE_PIXBUF, G_TYPE_STRING);
    GtkWidget *selector = new_selector(list);
    
//    g_signal_connect(G_OBJECT(selector), "changed",
//                     G_CALLBACK(view_select), server);
    
    gtk_container_add(GTK_CONTAINER(window), selector);

    return window;
}

void view_select(HildonTouchSelector *selector, gint column, gpointer data)
{
    GtkTreePath *path;
    path = hildon_touch_selector_get_last_activated_row(selector, column);
    if (!path) return;
    
    GtkTreeModel *model = hildon_touch_selector_get_model(selector, column);
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, path);

    char *label;
    gtk_tree_model_get(model, &iter, COL_LABEL, &label, -1);
    
    struct proxy *server = (struct proxy *)data;
    GtkWidget *window = NULL;
    if (!strcmp(label, "Artists")) {
        window = artists_window(server, NULL);
    }
    
    if (!window) return;
    HildonWindowStack *stack = hildon_window_stack_get_default();
    hildon_window_stack_push_1(stack, HILDON_STACKABLE_WINDOW(window));
}

GtkWidget *server_window(struct proxy *server)
{
    GtkWidget *window = hildon_stackable_window_new();
    gtk_window_set_title(GTK_WINDOW(window), server->name);

    GError *error = NULL;
    GdkPixbuf *icon = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
                                               "general_folder",
                                               HILDON_ICON_PIXEL_SIZE_FINGER,
                                               0, &error);
    GtkListStore *list = gtk_list_store_new(NUM_COLS-1,
                                            GDK_TYPE_PIXBUF, G_TYPE_STRING);
    GtkTreeIter iter;
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       COL_ICON, icon, COL_LABEL, "All tracks", -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       COL_ICON, icon, COL_LABEL, "Albums", -1);
    gtk_list_store_append(list, &iter);
    gtk_list_store_set(list, &iter,
                       COL_ICON, icon, COL_LABEL, "Artists", -1);

    GtkWidget *selector = new_selector(list);
    
    g_signal_connect(G_OBJECT(selector), "changed",
                     G_CALLBACK(view_select), server);
    
    gtk_container_add(GTK_CONTAINER(window), selector);

    return window;
}

void server_select(HildonTouchSelector *selector, gint column, gpointer data)
{
    GtkTreePath *path;
    path = hildon_touch_selector_get_last_activated_row(selector, column);
    if (!path) return;
    
    GtkTreeModel *model = hildon_touch_selector_get_model(selector, column);
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, path);

    char *udn;
    gtk_tree_model_get(model, &iter, COL_ID, &udn, -1);
    
    GHashTable *servers = (GHashTable *)data;
    struct proxy *server = g_hash_table_lookup(servers, udn);

    GtkWidget *window = server_window(server);
    
    /* TODO: GList *child_devices = gupnp_device_info_list_devices(GUPNP_DEVICE_INFO(server->proxy)); */

    HildonWindowStack *stack = hildon_window_stack_get_default();
    hildon_window_stack_push_1(stack, HILDON_STACKABLE_WINDOW(window));        
}

GtkWidget *main_window(HildonProgram *program, struct proxy_set *proxy_set)
{
    GtkWidget *window = hildon_stackable_window_new();
    hildon_program_add_window(program, HILDON_WINDOW(window));    

    gtk_window_set_title(GTK_WINDOW(window), APPLICATION_NAME);
    
    GError *error = NULL;
    proxy_set->icon = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
                                               "control_bluetooth_lan",
                                               HILDON_ICON_PIXEL_SIZE_FINGER,
                                               0, &error);
    proxy_set->server_list = gtk_list_store_new(NUM_COLS, GDK_TYPE_PIXBUF,
                                                G_TYPE_STRING, G_TYPE_STRING);
    
    GtkWidget *selector = new_selector(proxy_set->server_list);
    
    g_signal_connect(G_OBJECT(selector), "changed",
                     G_CALLBACK(server_select), proxy_set->servers);
    
    gtk_container_add(GTK_CONTAINER(window), selector);

    return window;
}

void add_renderer(GUPnPDeviceProxy *proxy, struct proxy_set *proxy_set)
{

}

void add_server(GUPnPDeviceProxy *proxy, struct proxy_set *proxy_set)
{
    const char *udn = gupnp_device_info_get_udn(GUPNP_DEVICE_INFO(proxy));
    struct proxy *server = g_hash_table_lookup(proxy_set->servers, udn);
    if (server) return;

    server = (struct proxy *)malloc(sizeof(struct proxy));
    server->proxy = proxy;
    server->name =
        gupnp_device_info_get_friendly_name(GUPNP_DEVICE_INFO(proxy));

    GtkTreeIter iter;
    gtk_list_store_append(proxy_set->server_list, &iter);
    gtk_list_store_set(proxy_set->server_list, &iter,
        COL_ICON, proxy_set->icon, COL_LABEL, server->name, COL_ID, udn, -1);

    GtkTreeModel *model = GTK_TREE_MODEL(proxy_set->server_list);
    GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
    server->row = gtk_tree_row_reference_new(model, path);
    
    g_hash_table_replace(proxy_set->servers, (char *)udn, server);
}

void remove_server(GUPnPDeviceProxy *proxy, struct proxy_set *proxy_set)
{
    const char *udn = gupnp_device_info_get_udn(GUPNP_DEVICE_INFO(proxy));
    struct proxy *server = g_hash_table_lookup(proxy_set->servers, udn);
    if (!server) return;
    
    GtkTreeModel *model = GTK_TREE_MODEL(proxy_set->server_list);
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter,
                            gtk_tree_row_reference_get_path(server->row));
    gtk_list_store_remove(proxy_set->server_list, &iter);
    g_hash_table_remove(proxy_set->servers, udn);
    free(server);
}

void device_proxy_available(GUPnPControlPoint *cp,
                            GUPnPDeviceProxy *proxy, gpointer user_data)
{
    struct proxy_set *proxy_set = (struct proxy_set *)user_data;
    const char *type;
    type = gupnp_device_info_get_device_type(GUPNP_DEVICE_INFO(proxy));

    if (g_pattern_match_simple(MEDIA_RENDERER, type)) {

    } else if (g_pattern_match_simple(MEDIA_SERVER, type)) {
        add_server(proxy, proxy_set);
    }
}

void device_proxy_unavailable(GUPnPControlPoint *cp,
                              GUPnPDeviceProxy *proxy, gpointer user_data)
{
    struct proxy_set *proxy_set = (struct proxy_set *)user_data;
    const char *type;
    type = gupnp_device_info_get_device_type(GUPNP_DEVICE_INFO(proxy));

    if (g_pattern_match_simple(MEDIA_RENDERER, type)) {

    } else if (g_pattern_match_simple(MEDIA_SERVER, type)) {
        remove_server(proxy, proxy_set);
    }
}

GUPnPContext *init_upnp(struct proxy_set *proxy_set)
{
    GError *error = NULL;
    
    g_type_init();
    
    GUPnPContext *context = gupnp_context_new(NULL, NULL, 0, &error);
    if (error) {
        g_printerr("Error creating the GUPnP context: %s\n", error->message);
        g_error_free(error);
        exit(1);
    }
    
    GUPnPControlPoint *cp = gupnp_control_point_new(context, "ssdp:all");
    
    g_signal_connect(cp, "device-proxy-available",
                     G_CALLBACK(device_proxy_available), proxy_set);
    g_signal_connect(cp, "device-proxy-unavailable",
                     G_CALLBACK(device_proxy_unavailable), proxy_set);

    gssdp_resource_browser_set_active(GSSDP_RESOURCE_BROWSER(cp), TRUE);
    
    return context;
}

int main(int argc, char *argv[])
{
    g_thread_init(NULL);
    
    hildon_gtk_init(&argc, &argv);

    HildonProgram *program = hildon_program_get_instance();
    g_set_application_name(APPLICATION_NAME);

    struct proxy_set proxy_set;
    proxy_set.renderers = g_hash_table_new(g_str_hash, g_str_equal);
    proxy_set.servers = g_hash_table_new(g_str_hash, g_str_equal);

    GtkWidget *window = main_window(program, &proxy_set);
    g_signal_connect(G_OBJECT(window), "destroy",
                     G_CALLBACK(gtk_main_quit), NULL);
    
    GUPnPContext *context = init_upnp(&proxy_set);

    gtk_widget_show(window);
    gtk_main();

    return 0;
}
