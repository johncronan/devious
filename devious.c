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


void add_content(GUPnPDIDLLiteParser *didl_parser, xmlNode *object_node,
                 gpointer user_data)
{
    struct browse_data *data = (struct browse_data *)user_data;
    
    char *title = gupnp_didl_lite_object_get_title(object_node);
    char *id = gupnp_didl_lite_object_get_id(object_node);
    gboolean container = gupnp_didl_lite_object_is_container(object_node);
    GError *error = NULL;
    GdkPixbuf *icon = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(),
                                               (container ? "general_folder"
                                                   : "general_audio_file"),
                                               HILDON_ICON_PIXEL_SIZE_FINGER,
                                               0, &error);
    GtkTreeIter iter;
    gtk_list_store_append(data->list, &iter);
    gtk_list_store_set(data->list, &iter,
                       COL_ICON, icon, COL_LABEL, title,
                       COL_ID, id, COL_CONTENT, data->content_dir,
                       COL_CONTAINER, container, -1);
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

        if (!gupnp_didl_lite_parser_parse_didl(data->didl_parser, didl_xml,
                                               add_content, data, &error)) {
            g_warning("%s\n", error->message);
            g_error_free(error);
        }
        g_free(didl_xml);

        data->starting_index += number_returned;

        /* See if we have more objects to get */
        remaining = total_matches - data->starting_index;
        /* Keep browsing till we get each and every object */
        if (remaining != 0) browse(content_dir, data->id, data->starting_index,
                                   MIN(remaining, MAX_BROWSE),
                                   data->list, data->didl_parser);
    } else if (error) {
        GUPnPServiceInfo *info;

        info = GUPNP_SERVICE_INFO(content_dir);
        g_warning("Failed to browse '%s': %s\n",
                  gupnp_service_info_get_location(info),
                  error->message);

        g_error_free(error);
    }

    browse_data_free(data);
}

void browse(GUPnPServiceProxy *content_dir, const char *container_id,
            guint32 starting_index, guint32 requested_count,
            GtkListStore *list, GUPnPDIDLLiteParser *didl_parser)
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
//    TODO
//    GtkTreeModel *model;
//    GtkTreeIter container_iter;
}

void on_container_update_ids(GUPnPServiceProxy *content_dir,
                             const char *variable, GValue *value,
                             gpointer user_data)
{
    char **tokens = g_strsplit(g_value_get_string(value), ",", 0);
    guint i;
    for (i=0; tokens[i] != NULL && tokens[i+1] != NULL; i+=2) {
        update_container(content_dir, tokens[i]);
    }
    g_strfreev(tokens);
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

void transport_uri(GUPnPServiceProxy *av_transport,
                   GUPnPServiceProxyAction *action, gpointer user_data)
{
    GError *error = NULL;
    if (gupnp_service_proxy_end_action(av_transport, action, &error, NULL)) {
        /* TODO: do something with duration? */
    } else {
        g_warning("Failed to set URI");
        g_error_free(error);
    }
}

gboolean mime_type_is_a(const char *mime_type1, const char *mime_type2)
{
    gboolean ret;

    char *content_type1 = g_content_type_from_mime_type(mime_type1);
    char *content_type2 = g_content_type_from_mime_type(mime_type2);
    if (content_type1 == NULL || content_type2 == NULL) {
        /* Uknown content type, just do a simple comarison */
        ret = g_ascii_strcasecmp(mime_type1, mime_type2) == 0;
    } else {
        ret = g_content_type_is_a(content_type1, content_type2);
    }

    g_free(content_type1);
    g_free(content_type2);

    return ret;
}

gboolean is_transport_compat(const gchar *renderer_protocol,
                             const gchar *renderer_host,
                             const gchar *item_protocol,
                             const gchar *item_host)
{
    if (g_ascii_strcasecmp(renderer_protocol, item_protocol) != 0 &&
        g_ascii_strcasecmp(renderer_protocol, "*") != 0) {
        return FALSE;
    } else if (g_ascii_strcasecmp("INTERNAL", renderer_protocol) == 0 &&
               g_ascii_strcasecmp(renderer_host, item_host) != 0) {
               /* Host must be the same in case of INTERNAL protocol */
        return FALSE;
    } else {
        return TRUE;
    }
}

gboolean is_content_format_compat(const gchar *renderer_content_format,
                                  const gchar *item_content_format)
{
    if(g_ascii_strcasecmp(renderer_content_format, "*") != 0 &&
        !mime_type_is_a(item_content_format, renderer_content_format)) {
        return FALSE;
    } else {
        return TRUE;
    }
}

gchar *get_dlna_pn(gchar **additional_info_fields)
{
    gchar *pn = NULL;
    gint i;
    for (i = 0; additional_info_fields[i]; i++) {
        pn = g_strstr_len(additional_info_fields[i],
                          strlen(additional_info_fields[i]), "DLNA.ORG_PN=");
        if (pn) {
            pn += 12; /* end of "DLNA.ORG_PN=" */
            break;
        }
    }

    return pn;
}

gboolean is_additional_info_compat(const gchar *renderer_additional_info,
                                   const gchar *item_additional_info)
{
    gboolean ret = FALSE;

    if (g_ascii_strcasecmp(renderer_additional_info, "*") == 0) {
        return TRUE;
    }

    char **renderer_tokens = g_strsplit(renderer_additional_info, ";", -1);
    if (renderer_tokens == NULL) {
        return FALSE;
    }

    char **item_tokens = g_strsplit(item_additional_info, ";", -1);
    if (item_tokens == NULL) {
        goto no_item_tokens;
    }

    char *renderer_pn = get_dlna_pn(renderer_tokens);
    char *item_pn = get_dlna_pn(item_tokens);
    if (renderer_pn == NULL || item_pn == NULL) {
        goto no_renderer_pn;
    }

    if (g_ascii_strcasecmp(renderer_pn, item_pn) == 0) {
        ret = TRUE;
    }

no_renderer_pn:
    g_strfreev(item_tokens);
no_item_tokens:
    g_strfreev(renderer_tokens);

    return ret;
}

gboolean is_protocol_info_compat(xmlNode *res_node,
                                 const gchar *renderer_protocol)
{
    gchar *item_protocol;
    gchar **item_proto_tokens;
    gchar **renderer_proto_tokens;
    gboolean ret = FALSE;

    item_protocol = gupnp_didl_lite_property_get_attribute(res_node,
                                                           "protocolInfo");
    if (!item_protocol) return FALSE;

    item_proto_tokens = g_strsplit(item_protocol, ":", 4);
    renderer_proto_tokens = g_strsplit(renderer_protocol, ":", 4);

    if (!item_proto_tokens[0] || !item_proto_tokens[1] ||
        !item_proto_tokens[2] || !item_proto_tokens[3] ||
        !renderer_proto_tokens[0] || !renderer_proto_tokens[1] ||
        !renderer_proto_tokens[2] || !renderer_proto_tokens[3])
        goto return_point;

    if (is_transport_compat(renderer_proto_tokens[0], renderer_proto_tokens[2],
                            item_proto_tokens[0], item_proto_tokens[1]) &&
        is_content_format_compat(renderer_proto_tokens[2],
                                  item_proto_tokens[2]) &&
        is_additional_info_compat(renderer_proto_tokens[3],
                                  item_proto_tokens[3]))
        ret = TRUE;

return_point:
    g_free(item_protocol);
    g_strfreev(renderer_proto_tokens);
    g_strfreev(item_proto_tokens);

    return ret;
}

char *find_compat_uri_from_metadata(const char *metadata, char **duration,
                                    struct proxy *renderer)
{
    char *uri = NULL;
    void on_didl_item_available(GUPnPDIDLLiteParser *didl_parser,
                                xmlNode *item_node, gpointer user_data)
    {
        GList *resources =
            gupnp_didl_lite_object_get_property(item_node, "res");
        if (!resources) return;

        int i;
        for (i=0; renderer->protocols[i] && uri == NULL; i++) {
            GList *res, *compat_res = NULL;
            xmlNode *res_node;
            for (res = resources; res != NULL; res = res->next) {
                res_node = (xmlNode *)res->data;

                int j;
                for (j=0; renderer->protocols[j]; j++) {
                    if (is_protocol_info_compat(res_node,
                                                renderer->protocols[j])) {
                        compat_res = res;
                        break;
                    }
                }
            }
            if (!compat_res) continue;

            res_node = (xmlNode *)compat_res->data;
            uri = gupnp_didl_lite_property_get_value(res_node);
            *duration = gupnp_didl_lite_property_get_attribute(res_node,
                                                               "duration");
        }
        g_list_free(resources);
    }

    GError *error = NULL;
    /* Assumption: metadata only contains a single didl object */
    gupnp_didl_lite_parser_parse_didl(renderer->set->didl_parser, metadata,
                                      on_didl_item_available, NULL, &error);
    if (error) {
        g_warning("%s\n", error->message);
        g_error_free(error);
    }
    
    return uri;
}

struct proxy *current_renderer(struct proxy_set *proxy_set)
{
    int i = hildon_picker_button_get_active(proxy_set->renderer_picker);
    GtkTreeIter iter;
    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(proxy_set->renderer_list),
                                  &iter, NULL, i);
    char *udn;
    gtk_tree_model_get(GTK_TREE_MODEL(proxy_set->renderer_list), &iter,
                       1, &udn, -1);
    
    return g_hash_table_lookup(proxy_set->renderers, udn);
}

void set_av_transport_uri(GUPnPServiceProxy *content_dir,
                          GUPnPServiceProxyAction *action, gpointer user_data)
{
    GError *error = NULL;
    char *metadata;
    gupnp_service_proxy_end_action(content_dir, action, &error,
                                   "Result", G_TYPE_STRING, &metadata, NULL);
    if (!metadata) return;
    if (error) {
        g_warning("Failed to get metadata for content");
        g_error_free(error);
    }

    struct proxy_set *proxy_set = (struct proxy_set *)user_data;
    struct proxy *renderer = current_renderer(proxy_set);

    char *duration;
    char *uri = find_compat_uri_from_metadata(metadata, &duration, renderer);
    if (!uri) {
        g_warning("no compatible URI found.");
        return;
    }

    GUPnPServiceProxy *av_transport = GUPNP_SERVICE_PROXY(
        gupnp_device_info_get_service(GUPNP_DEVICE_INFO(renderer->proxy),
                                      AV_TRANSPORT));

    gupnp_service_proxy_begin_action(av_transport, "SetAVTransportURI",
                                         transport_uri, NULL,
                                     "InstanceID", G_TYPE_UINT, 0,
                                     "CurrentURI", G_TYPE_STRING, uri,
                                     "CurrentURIMetaData", G_TYPE_STRING,
                                         metadata,
                                     NULL);
}

void av_transport_action_cb(GUPnPServiceProxy *av_transport,
                            GUPnPServiceProxyAction *action, gpointer data)
{
    const char *action_name = (const char *)data;
    GError *error = NULL;

    if (!gupnp_service_proxy_end_action(av_transport, action, &error, NULL)) {
        g_warning("Failed to send action '%s': %s",
                  action_name, error->message);
        g_error_free(error);
    }
}

void g_value_free(gpointer data)
{
    g_value_unset((GValue *)data);
    g_slice_free(GValue, data);
}

GHashTable *create_av_transport_args_hash(char **additional_args)
{
    GHashTable *args = g_hash_table_new_full(g_str_hash, g_str_equal,
                                             NULL, g_value_free);

    GValue *instance_id = g_slice_alloc0(sizeof(GValue));
    g_value_init(instance_id, G_TYPE_UINT);
    g_value_set_uint(instance_id, 0);

    g_hash_table_insert(args, "InstanceID", instance_id);

    if (additional_args) {
        int i;
        for (i=0; additional_args[i]; i += 2) {
            GValue *value = g_slice_alloc0(sizeof(GValue));
            g_value_init(value, G_TYPE_STRING);
            g_value_set_string(value, additional_args[i + 1]);
            g_hash_table_insert(args, additional_args[i], value);
        }
    }
    return args;
}

void av_transport_send_action(GUPnPServiceProxy *av_transport, char *action,
                              char *additional_args[])
{
    GHashTable *args = create_av_transport_args_hash(additional_args);

    gupnp_service_proxy_begin_action_hash(av_transport, action,
                                          av_transport_action_cb,
                                          (char *)action,
                                          args);
    g_hash_table_unref(args);
}

void transport_selection(struct proxy *server, GtkTreeRowReference *row)
{
    server->current_selection = row;
    
    GtkTreeModel *model = gtk_tree_row_reference_get_model(row);
    GtkTreePath *path = gtk_tree_row_reference_get_path(row);

    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, path);

    char *label;
    char *id;
    GUPnPServiceProxy *content;
    gtk_tree_model_get(model, &iter, COL_LABEL, &label,
                                     COL_CONTENT, &content, COL_ID, &id, -1);
    
    gupnp_service_proxy_begin_action(content, "Browse",
                                         set_av_transport_uri, server->set,
                                     "ObjectID", G_TYPE_STRING, id,
                                     "BrowseFlag", G_TYPE_STRING,
                                         "BrowseMetadata",
                                     "Filter", G_TYPE_STRING, "*",
                                     "StartingIndex", G_TYPE_UINT, 0,
                                     "RequestedCount", G_TYPE_UINT, 0,
                                     "SortCriteria", G_TYPE_STRING, "",
                                     NULL);
}

void play_button(GtkWidget *button, gpointer data)
{
    struct proxy_set *proxy_set = (struct proxy_set *)data;
    struct proxy *renderer = current_renderer(proxy_set);
    GUPnPServiceProxy *av_transport = GUPNP_SERVICE_PROXY(
        gupnp_device_info_get_service(GUPNP_DEVICE_INFO(renderer->proxy),
                                      AV_TRANSPORT));
    char *args[] = {"Speed", "1", NULL};
    av_transport_send_action(av_transport, "Play", args);
}

GtkWidget *play_window(struct proxy *server, GtkTreeRowReference *row)
{
    GtkWidget *window = hildon_stackable_window_new();
    gtk_window_set_title(GTK_WINDOW(window), PLAY_WINDOW_TITLE);
    
    GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 6);
    
    GtkWidget *image = gtk_image_new();
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 6);
    
    GtkWidget *inner_box = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), inner_box, TRUE, TRUE, 6);

    GtkWidget *button_box = gtk_hbutton_box_new();
    gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 6);
    
    GtkWidget *button =
        hildon_button_new_with_text(HILDON_SIZE_FINGER_HEIGHT,
                                    HILDON_BUTTON_ARRANGEMENT_VERTICAL,
                                    "Play", "");
    
    g_signal_connect(button, "clicked", G_CALLBACK(play_button), server->set);
    
    gtk_box_pack_start(GTK_BOX(button_box), button, FALSE, FALSE, 0);
    gtk_widget_show_all(GTK_WIDGET(vbox));
    
    return window;
}

void content_select(HildonTouchSelector *selector, gint column, gpointer data)
{
    GtkTreePath *path;
    path = hildon_touch_selector_get_last_activated_row(selector, column);
    if (!path) return;

    GtkTreeModel *model = hildon_touch_selector_get_model(selector, column);
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, path);

    GUPnPServiceProxy *content;
    char *id, *label;
    gboolean container;
    gtk_tree_model_get(model, &iter, COL_CONTENT, &content, COL_ID, &id,
                                     COL_CONTAINER, &container,
                                     COL_LABEL, &label, -1);

    struct proxy *server = (struct proxy *)data;
    GtkWidget *window;
    
    if (container) {
        GtkListStore *view_list;
        window = content_window(server, label, &view_list);
        browse(content, id, 0, MAX_BROWSE, view_list, server->set->didl_parser);
        gupnp_service_proxy_add_notify(content, "ContainerUpdateIDs",
                                       G_TYPE_STRING, on_container_update_ids,
                                       NULL);
        gupnp_service_proxy_set_subscribed(content, TRUE);
    } else {
        GtkTreeRowReference *row = gtk_tree_row_reference_new(model, path);
        transport_selection(server, row);
        window = play_window(server, row);
    }
    
    HildonWindowStack *stack = hildon_window_stack_get_default();
    hildon_window_stack_push_1(stack, HILDON_STACKABLE_WINDOW(window));
}

GtkWidget *content_window(struct proxy *server, char *title,
                          GtkListStore **view_list)
{
    GtkWidget *window = hildon_stackable_window_new();
    gtk_window_set_title(GTK_WINDOW(window), title);

    GtkListStore *list = gtk_list_store_new(NUM_COLS, GDK_TYPE_PIXBUF,
                                            G_TYPE_STRING, G_TYPE_STRING,
                                            G_TYPE_POINTER, G_TYPE_BOOLEAN);

    GtkWidget *selector = new_selector(list);

    g_signal_connect(G_OBJECT(selector), "changed",
                     G_CALLBACK(content_select), server);

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
    GtkWidget *window = content_window(server, server->name, &view_list);

    browse(content_dir, "0", 0, MAX_BROWSE, view_list,
           server->set->didl_parser);

    gupnp_service_proxy_add_notify(content_dir, "ContainerUpdateIDs",
                                   G_TYPE_STRING, on_container_update_ids,
                                   NULL);
    gupnp_service_proxy_set_subscribed(content_dir, TRUE);

    /* TODO: GList *child_devices = gupnp_device_info_list_devices(GUPNP_DEVICE_INFO(server->proxy)); */

    HildonWindowStack *stack = hildon_window_stack_get_default();
    hildon_window_stack_push_1(stack, HILDON_STACKABLE_WINDOW(window));
}

GtkWidget *target_selector(struct proxy_set *proxy_set)
{
    GtkWidget *selector;
    selector = hildon_touch_selector_new();

    HildonTouchSelectorColumn *column =
        hildon_touch_selector_append_column(HILDON_TOUCH_SELECTOR(selector),
            GTK_TREE_MODEL(proxy_set->renderer_list), NULL, NULL);
    g_object_unref(proxy_set->renderer_list);
    hildon_touch_selector_column_set_text_column(column, 0);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
    gtk_cell_renderer_set_fixed_size(renderer, -1, ROW_HEIGHT);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(column), renderer,
                                   "text", 0, NULL);

    gtk_widget_show_all(selector);
    return selector;
}

GtkWidget *main_menu(struct proxy_set *proxy_set)
{
    GtkWidget *menu = hildon_app_menu_new();
    GtkWidget *button;
    button = hildon_picker_button_new(HILDON_SIZE_AUTO,
                                      HILDON_BUTTON_ARRANGEMENT_VERTICAL);
    hildon_button_set_title(HILDON_BUTTON(button), CHOOSE_TARGET);
    
    GtkWidget *selector = target_selector(proxy_set);
    hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(button),
                                      HILDON_TOUCH_SELECTOR(selector));
    proxy_set->renderer_picker = HILDON_PICKER_BUTTON(button);
    
    hildon_app_menu_append(HILDON_APP_MENU(menu), GTK_BUTTON(button));

    gtk_widget_show_all(menu);
    return menu;
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
                                                G_TYPE_STRING, G_TYPE_STRING,
                                                G_TYPE_POINTER, G_TYPE_BOOLEAN);

    GtkWidget *selector = new_selector(proxy_set->server_list);

    g_signal_connect(G_OBJECT(selector), "changed",
                     G_CALLBACK(server_select), proxy_set->servers);

    gtk_container_add(GTK_CONTAINER(window), selector);

    GtkWidget *menu = main_menu(proxy_set);
    hildon_window_set_app_menu(HILDON_WINDOW(window), HILDON_APP_MENU(menu));
    
    return window;
}

void protocol_info(GUPnPServiceProxy *cm, GUPnPServiceProxyAction *action,
                   gpointer user_data)
{
    gchar *sink_protocols;
    GError *error = NULL;
    const gchar *udn = gupnp_service_info_get_udn(GUPNP_SERVICE_INFO(cm));

    if (!gupnp_service_proxy_end_action(cm, action, &error, "Sink",
                                        G_TYPE_STRING, &sink_protocols,
                                        NULL)) {
        g_warning("Failed to get sink protocol info from "
                       "media renderer '%s':%s\n",
                   udn, error->message);
        g_error_free(error);
        return;
    }

    struct proxy *server = (struct proxy *)user_data;
    server->protocols = g_strsplit(sink_protocols, ",", 0);
}

void add_renderer(GUPnPDeviceProxy *proxy, struct proxy_set *proxy_set)
{
    const char *udn = gupnp_device_info_get_udn(GUPNP_DEVICE_INFO(proxy));
    if (!udn) return;
    
    struct proxy *server = g_hash_table_lookup(proxy_set->renderers, udn);
    if (server) return;
    
    char *name = gupnp_device_info_get_friendly_name(GUPNP_DEVICE_INFO(proxy));
    if (!name) return;
    
    server = (struct proxy *)g_malloc(sizeof(struct proxy));
    server->proxy = proxy;
    server->name = name;
    server->set = proxy_set;

    GtkTreeIter iter;
    gtk_list_store_append(proxy_set->renderer_list, &iter);
    gtk_list_store_set(proxy_set->renderer_list, &iter,
        0, server->name, 1, udn, -1);
    
    /* TODO: default to saved value */
    hildon_picker_button_set_active(proxy_set->renderer_picker, 0);

    GtkTreeModel *model = GTK_TREE_MODEL(proxy_set->renderer_list);
    GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
    server->row = gtk_tree_row_reference_new(model, path);

    g_hash_table_replace(proxy_set->renderers, (char *)udn, server);

    GUPnPServiceProxy *cm = GUPNP_SERVICE_PROXY(
        gupnp_device_info_get_service(GUPNP_DEVICE_INFO(proxy),
                                      CONNECTION_MANAGER));
    gupnp_service_proxy_begin_action(cm, "GetProtocolInfo", protocol_info,
                                     server, NULL);
}

void add_server(GUPnPDeviceProxy *proxy, struct proxy_set *proxy_set)
{
    const char *udn = gupnp_device_info_get_udn(GUPNP_DEVICE_INFO(proxy));
    if (!udn) return;
    
    struct proxy *server = g_hash_table_lookup(proxy_set->servers, udn);
    if (server) return;

    char *name = gupnp_device_info_get_friendly_name(GUPNP_DEVICE_INFO(proxy));
    GUPnPServiceInfo *content_dir =
        gupnp_device_info_get_service(GUPNP_DEVICE_INFO(proxy), CONTENT_DIR);
    
    if (!name || !content_dir) return;

    server = (struct proxy *)g_malloc(sizeof(struct proxy));
    server->proxy = proxy;
    server->name = name;
    server->set = proxy_set;
    
    GtkTreeIter iter;
    gtk_list_store_append(proxy_set->server_list, &iter);
    gtk_list_store_set(proxy_set->server_list, &iter,
        COL_ICON, proxy_set->icon, COL_LABEL, server->name, COL_ID, udn,
        COL_CONTENT, NULL, COL_CONTAINER, TRUE, -1);

    GtkTreeModel *model = GTK_TREE_MODEL(proxy_set->server_list);
    GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
    server->row = gtk_tree_row_reference_new(model, path);

    g_hash_table_replace(proxy_set->servers, (char *)udn, server);
}

void remove_renderer(GUPnPDeviceProxy *proxy, struct proxy_set *proxy_set)
{
    const char *udn = gupnp_device_info_get_udn(GUPNP_DEVICE_INFO(proxy));
    struct proxy *server = g_hash_table_lookup(proxy_set->renderers, udn);
    if (!server) return;

    GtkTreeModel *model = GTK_TREE_MODEL(proxy_set->renderer_list);
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter,
                            gtk_tree_row_reference_get_path(server->row));
    gtk_list_store_remove(proxy_set->renderer_list, &iter);
    g_hash_table_remove(proxy_set->renderers, udn);
    free(server);
    
    /* TODO: change current selection if necessary */
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
    
    /* TODO: bring user back to server menu if necessary */
}

void device_proxy_available(GUPnPControlPoint *cp,
                            GUPnPDeviceProxy *proxy, gpointer user_data)
{
    struct proxy_set *proxy_set = (struct proxy_set *)user_data;
    const char *type;
    type = gupnp_device_info_get_device_type(GUPNP_DEVICE_INFO(proxy));

    if (g_pattern_match_simple(MEDIA_RENDERER, type)) {
        add_renderer(proxy, proxy_set);
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
        remove_renderer(proxy, proxy_set);
    } else if (g_pattern_match_simple(MEDIA_SERVER, type)) {
        remove_server(proxy, proxy_set);
    }
}

void init_upnp(struct proxy_set *proxy_set)
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

    proxy_set.renderer_list = gtk_list_store_new(2, G_TYPE_STRING,
                                                    G_TYPE_STRING);
    proxy_set.didl_parser = gupnp_didl_lite_parser_new();
    GtkWidget *window = main_window(program, &proxy_set);
    g_signal_connect(G_OBJECT(window), "destroy",
                     G_CALLBACK(gtk_main_quit), NULL);
    
    init_upnp(&proxy_set);

    gtk_widget_show(window);
    gtk_main();

    return 0;
}
