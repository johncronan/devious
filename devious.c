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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include <libgupnp/gupnp-control-point.h>
#include <libgupnp-av/gupnp-av.h>

#include "devious.h"

GUPnPDIDLLiteParser *didl_parser;

void add_view(GUPnPDIDLLiteParser *didl_parser, xmlNode *object_node,
              gpointer user_data)
{
    struct browse_data *data = (struct browse_data *)user_data;
    
    char *title = gupnp_didl_lite_object_get_title(object_node);

    GError *error = NULL;
    GdkPixbuf *icon = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
                                               "general_folder",
                                               HILDON_ICON_PIXEL_SIZE_FINGER,
                                               0, &error);
    GtkTreeIter iter;
    gtk_list_store_append(data->list, &iter);
    gtk_list_store_set(data->list, &iter,
                       COL_ICON, icon, COL_LABEL, title, -1);
}

struct browse_data *browse_data_new(GUPnPServiceProxy *content_dir,
                                    const char *id, guint32 starting_index,
                                    GtkListStore *list)
{
    struct browse_data *data;

    data = g_slice_new(struct browse_data);
    data->content_dir = g_object_ref(content_dir);
    data->id = g_strdup(id);
    data->starting_index = starting_index;
    data->list = list;

    return data;
}

void browse_data_free(struct browse_data *data)
{
    g_free(data->id);
    g_object_unref(data->content_dir);
    g_slice_free(struct browse_data, data);
}

void browse_cb(GUPnPServiceProxy *content_dir,
               GUPnPServiceProxyAction *action, gpointer user_data)
{
    struct browse_data *data;
    char *didl_xml;
    guint32 number_returned;
    guint32 total_matches;
    GError *error;

    data = (struct browse_data *)user_data;
    didl_xml = NULL;
    error = NULL;

    gupnp_service_proxy_end_action(content_dir, action, &error,
                                   "Result", G_TYPE_STRING, &didl_xml,
                                   "NumberReturned", G_TYPE_UINT,
                                       &number_returned,
                                   "TotalMatches", G_TYPE_UINT, &total_matches,
                                   NULL);
    if (didl_xml) {
        guint32 remaining;
        GError *error = NULL;

        if (!gupnp_didl_lite_parser_parse_didl(didl_parser, didl_xml,
                                               add_view, data, &error)) {
            g_warning("%s\n", error->message);
            g_error_free(error);
        }
        g_free(didl_xml);

        data->starting_index += number_returned;

        /* See if we have more objects to get */
        remaining = total_matches - data->starting_index;
        /* Keep browsing till we get each and every object */
        if (remaining != 0) browse(content_dir, data->id, data->starting_index,
                                   MIN(remaining, MAX_BROWSE), data->list);
    } else if (error) {
        GUPnPServiceInfo *info;

        info = GUPNP_SERVICE_INFO(content_dir);
        g_warning("Failed to browse '%s': %s\n",
                  gupnp_service_info_get_location (info),
                  error->message);

        g_error_free(error);
    }

    browse_data_free(data);
}

void browse(GUPnPServiceProxy *content_dir, const char *container_id,
            guint32 starting_index, guint32 requested_count,
            GtkListStore *list)
{
    struct browse_data *data;
    data = browse_data_new(content_dir, container_id, starting_index, list);

    gupnp_service_proxy_begin_action(content_dir, "Browse", browse_cb, data,
                                     "ObjectID", G_TYPE_STRING, container_id,
                                     "BrowseFlag", G_TYPE_STRING, 
                                         "BrowseDirectChildren",
                                     "Filter", G_TYPE_STRING, "*",
                                     "StartingIndex", G_TYPE_UINT,
                                         starting_index,
                                     "RequestedCount", G_TYPE_UINT,
                                         requested_count,
                                     "SortCriteria", G_TYPE_STRING, "",
                                     NULL);
}

void update_container(GUPnPServiceProxy *content_dir,
                      const char *container_id)
{
    GtkTreeModel *model;
    GtkTreeIter container_iter;
    
printf("qux: %s\n", container_id);
}

void on_container_update_ids(GUPnPServiceProxy *content_dir,
                             const char *variable, GValue *value,
                             gpointer user_data)
{
    char **tokens;
    guint  i;

    tokens = g_strsplit(g_value_get_string(value), ",", 0);
    for (i=0; tokens[i] != NULL && tokens[i+1] != NULL; i+=2) {
        update_container(content_dir, tokens[i]);
    }

    g_strfreev (tokens);
}

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

GtkWidget *server_window(struct proxy *server, GtkListStore **view_list)
{
    GtkWidget *window = hildon_stackable_window_new();
    gtk_window_set_title(GTK_WINDOW(window), server->name);

    GtkListStore *list = gtk_list_store_new(NUM_COLS-1,
                                            GDK_TYPE_PIXBUF, G_TYPE_STRING);

    GtkWidget *selector = new_selector(list);

    g_signal_connect(G_OBJECT(selector), "changed",
                     G_CALLBACK(view_select), server);

    gtk_container_add(GTK_CONTAINER(window), selector);

    *view_list = list;
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

    GUPnPServiceProxy *content_dir = GUPNP_SERVICE_PROXY(
        gupnp_device_info_get_service(GUPNP_DEVICE_INFO(server->proxy),
                                      CONTENT_DIR));

    GtkListStore *view_list;
    GtkWidget *window = server_window(server, &view_list);

    browse(content_dir, "0", 0, MAX_BROWSE, view_list);
    gupnp_service_proxy_add_notify(content_dir, "ContainerUpdateIDs",
                                   G_TYPE_STRING, on_container_update_ids,
                                   NULL);
    gupnp_service_proxy_set_subscribed(content_dir, TRUE);

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

    char *name = gupnp_device_info_get_friendly_name(GUPNP_DEVICE_INFO(proxy));
    GUPnPServiceInfo *content_dir =
        gupnp_device_info_get_service(GUPNP_DEVICE_INFO(proxy), CONTENT_DIR);
    
    if (!name || !content_dir) return;

    server = (struct proxy *)malloc(sizeof(struct proxy));
    server->proxy = proxy;
    server->name = name;
    
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
