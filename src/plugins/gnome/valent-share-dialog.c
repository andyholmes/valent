// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-share-target-chooser"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-share-dialog.h"
#include "valent-share-dialog-row.h"


struct _ValentShareDialog
{
  AdwWindow            parent_instance;

  ValentDeviceManager *manager;
  GListModel          *files;
  unsigned int         refresh_id;
  unsigned int         selection_mode : 1;

  GPtrArray           *rows;
  GCancellable        *cancellable;
  guint64              total_size;
  unsigned int         n_files;
  unsigned int         n_links;

  /* template */
  AdwNavigationView   *view;
  AdwActionRow        *single_row;
  GtkImage            *single_icon;
  AdwExpanderRow      *multiple_row;
  GtkImage            *multiple_icon;
  GtkListBox          *device_list;
  AdwEntryRow         *uri_entry;
};

G_DEFINE_FINAL_TYPE (ValentShareDialog, valent_share_dialog, ADW_TYPE_WINDOW)

enum {
  PROP_0,
  PROP_FILES,
  PROP_SELECTION_MODE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void   valent_share_dialog_set_files (ValentShareDialog    *self,
                                             GListModel           *files);
static void   valent_share_dialog_share     (ValentShareDialog    *self,
                                             ValentShareDialogRow *row);


/*
 * Summary
 */
typedef struct
{
  ValentShareDialog *self;
  GtkWidget         *row;
  GtkImage          *icon;
} EntryData;

static void
g_file_query_info_cb (GFile        *file,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  g_autofree EntryData *data = (EntryData *)user_data;
  GIcon *icon = NULL;
  g_autoptr (GFileInfo) info = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *size_str = NULL;
  g_autofree char *size_label = NULL;
  gsize size = 0;

  if ((info = g_file_query_info_finish (file, result, &error)) == NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
          gtk_widget_add_css_class (GTK_WIDGET (data->row), "error");
        }

      return;
    }

  if ((icon = g_file_info_get_icon (info)) != NULL)
    {
      gtk_image_set_from_gicon (data->icon, icon);
      gtk_widget_add_css_class (GTK_WIDGET (data->icon), "lowres-icon");
    }
  else
    {
      gtk_image_set_from_icon_name (data->icon, "share-file-symbolic");
      gtk_widget_remove_css_class (GTK_WIDGET (data->icon), "lowres-icon");
    }

  size = g_file_info_get_size (info);
  size_str = g_format_size (size);
  size_label = g_strdup_printf (_("Size: %s"), size_str);
  g_object_set (data->row, "subtitle", size_label, NULL);

  if (data->self->n_files > 1)
    {
      g_autofree char *total_str = NULL;
      g_autofree char *total_label = NULL;

      if ((G_MAXUINT64 - data->self->total_size) < size)
        data->self->total_size = G_MAXUINT64;
      else
        data->self->total_size += size;

      total_str = g_format_size (data->self->total_size);
      total_label = g_strdup_printf (_("Total size: %s"), total_str);
      g_object_set (data->self->multiple_row, "subtitle", total_label, NULL);
    }
}

static void
valent_share_dialog_add_entry (ValentShareDialog *self,
                               GFile             *file,
                               GCancellable      *cancellable)
{
  g_autofree char *title = NULL;
  const char *icon_name = NULL;
  gboolean is_file  = FALSE;
  GtkWidget *row;
  GtkWidget *icon;

  is_file = g_file_has_uri_scheme (file, "file");

  if (is_file)
    {
      self->n_files += 1;
      title = g_file_get_basename (file);
      icon_name = "share-file-symbolic";
    }
  else
    {
      g_autofree char *uri = NULL;

      uri = g_file_get_uri (file);

      self->n_links += 1;
      title = g_strdup_printf ("<a href=\"%s\">%s</a>", uri, uri);
      icon_name = "share-link-symbolic";
    }

  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "title",       title,
                      "title-lines", 1,
                      NULL);
  icon = g_object_new (GTK_TYPE_IMAGE,
                       "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                       "icon-name",       icon_name,
                       NULL);
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), icon);
  adw_expander_row_add_row (self->multiple_row, row);
  g_ptr_array_add (self->rows, row);

  if (is_file)
    {
      EntryData *data = NULL;

      data = g_new0 (EntryData, 1);
      data->self = self;
      data->row = (GtkWidget *)row;
      data->icon = (GtkImage *)icon;

      g_file_query_info_async (file,
                               G_FILE_ATTRIBUTE_STANDARD_SIZE","
                               G_FILE_ATTRIBUTE_STANDARD_ICON,
                               G_FILE_QUERY_INFO_NONE,
                               G_PRIORITY_DEFAULT_IDLE,
                               cancellable,
                               (GAsyncReadyCallback)g_file_query_info_cb,
                               g_steal_pointer (&data));
    }
}

static void
valent_share_dialog_reset (ValentShareDialog *self)
{
  g_assert (VALENT_IS_SHARE_DIALOG (self));

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();
  self->n_files = 0;
  self->n_links = 0;
  self->total_size = 0;

  g_object_set (self->single_row,
                "title",    NULL,
                "subtitle", NULL,
                NULL);
  g_object_set (self->multiple_row,
                "title",    NULL,
                "subtitle", NULL,
                NULL);
  gtk_editable_set_text (GTK_EDITABLE (self->uri_entry), "");

  if (self->rows != NULL)
    {
      for (unsigned int i = 0; i < self->rows->len; i++)
        {
          adw_expander_row_remove (self->multiple_row,
                                   g_ptr_array_index (self->rows, i));
        }
      g_clear_pointer (&self->rows, g_ptr_array_unref);
    }
}

static void
on_files_changed (GListModel        *list,
                  unsigned int       position,
                  unsigned int       removed,
                  unsigned int       added,
                  ValentShareDialog *self)
{
  unsigned int n_items = 0;

  g_assert (VALENT_IS_SHARE_DIALOG (self));

  valent_share_dialog_reset (self);

  if (self->files != NULL)
    n_items = g_list_model_get_n_items (self->files);

  if (n_items > 1)
    {
      g_autofree char *title = NULL;

      self->rows = g_ptr_array_sized_new (n_items);

      for (unsigned int i = 0; i < n_items; i++)
        {
          g_autoptr (GFile) file = NULL;

          file = g_list_model_get_item (self->files, i);
          valent_share_dialog_add_entry (self, file, self->cancellable);
        }

      if (self->n_files > 0 && self->n_links > 0)
        title = g_strdup_printf (_("%u files and links"), n_items);
      else if (self->n_files > 0)
        title = g_strdup_printf (_("%u files"), n_items);
      else if (self->n_links > 0)
        title = g_strdup_printf (_("%u links"), n_items);

      g_object_set (self->multiple_row,
                    "title",   title,
                    "visible", TRUE,
                    NULL);
    }
  else if (n_items > 0)
    {
      g_autofree char *title = NULL;
      g_autoptr (GFile) entry = NULL;
      const char *icon_name = NULL;

      entry = g_list_model_get_item (self->files, 0);

      if (g_file_has_uri_scheme (entry, "file"))
        {
          EntryData *data;

          data = g_new0 (EntryData, 1);
          data->self = self;
          data->row = (GtkWidget *)self->single_row;
          data->icon = self->single_icon;

          g_file_query_info_async (entry,
                                   G_FILE_ATTRIBUTE_STANDARD_SIZE","
                                   G_FILE_ATTRIBUTE_STANDARD_ICON,
                                   G_FILE_QUERY_INFO_NONE,
                                   G_PRIORITY_DEFAULT_IDLE,
                                   self->cancellable,
                                   (GAsyncReadyCallback)g_file_query_info_cb,
                                   g_steal_pointer (&data));

          title = g_file_get_basename (entry);
          icon_name = "share-symbolic";
        }
      else
        {
          g_autofree char *uri = NULL;

          uri = g_file_get_uri (entry);

          title = g_strdup_printf ("<a href=\"%s\">%s</a>", uri, uri);
          icon_name = "share-link-symbolic";
        }

      g_object_set (self->single_row,
                    "title",   title,
                    "visible", TRUE,
                    NULL);
      gtk_image_set_from_icon_name (self->single_icon, icon_name);
      gtk_widget_remove_css_class (GTK_WIDGET (self->single_icon), "lowres-icon");
    }
}

/*
 * Devices
 */
static void
on_action_added (GActionGroup *action_group,
                 const char   *action_name,
                 GtkWidget    *widget)
{
  gboolean visible = FALSE;

  if (g_action_group_get_action_enabled (action_group, action_name))
    visible = TRUE;

  gtk_widget_set_visible (widget, visible);
}

static void
on_action_removed (GActionGroup *action_group,
                   const char   *action_name,
                   GtkWidget    *widget)
{
  gtk_widget_set_visible (widget, FALSE);
}

static void
on_action_enabled_changed (GActionGroup *action_group,
                           const char   *action_name,
                           gboolean      enabled,
                           GtkWidget    *widget)
{
  gtk_widget_set_visible (widget, enabled);
}

static void
on_device_activated (GtkListBox           *box,
                     ValentShareDialogRow *row,
                     ValentShareDialog    *self)
{
  g_assert (GTK_IS_LIST_BOX (box));
  g_assert (VALENT_IS_SHARE_DIALOG_ROW (row));
  g_assert (VALENT_IS_SHARE_DIALOG (self));

  if (self->selection_mode)
    {
      gboolean selected;

      selected = valent_share_dialog_row_get_selected (row);
      valent_share_dialog_row_set_selected (row, !selected);
    }
  else
    {
      valent_share_dialog_share (self, row);
    }
}

static void
on_selected_changed (ValentShareDialog *self)
{
  GtkWidget *child;
  gboolean enabled = FALSE;

  if (!self->selection_mode)
    {
      gtk_widget_action_set_enabled (GTK_WIDGET (self),
                                     "chooser.share",
                                     enabled);
      return;
    }

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self->device_list));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (!VALENT_IS_SHARE_DIALOG_ROW (child))
        continue;

      if (valent_share_dialog_row_get_selected (VALENT_SHARE_DIALOG_ROW (child)))
        {
          enabled = TRUE;
          break;
        }
    }

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "chooser.share", enabled);
}

static GtkWidget *
valent_share_dialog_create_row (gpointer item,
                                gpointer user_data)
{
  ValentShareDialog *self = VALENT_SHARE_DIALOG (user_data);
  ValentDevice *device = VALENT_DEVICE (item);
  GtkWidget *row;

  g_assert (VALENT_IS_DEVICE (device));

  row = g_object_new (VALENT_TYPE_SHARE_DIALOG_ROW,
                      "device",         device,
                      "selection-mode", self->selection_mode,
                      NULL);
  g_object_bind_property (self, "selection-mode",
                          row,  "selection-mode",
                          G_BINDING_SYNC_CREATE);
  g_signal_connect_object (row,
                           "notify::selected",
                           G_CALLBACK (on_selected_changed),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (device,
                           "action-added::share.uris",
                           G_CALLBACK (on_action_added),
                           row, 0);
  g_signal_connect_object (device,
                           "action-removed::share.uris",
                           G_CALLBACK (on_action_removed),
                           row, 0);
  g_signal_connect_object (device,
                           "action-enabled-changed::share.uris",
                           G_CALLBACK (on_action_enabled_changed),
                           row, 0);
  on_action_added (G_ACTION_GROUP (device), "share.uris", row);

  return row;
}

static void
on_items_changed (GListModel        *list,
                  unsigned int       position,
                  unsigned int       removed,
                  unsigned int       added,
                  ValentShareDialog *self)
{
  g_assert (VALENT_IS_SHARE_DIALOG (self));

  while (removed--)
    {
      GtkListBoxRow *row;

      row = gtk_list_box_get_row_at_index (self->device_list, position);
      gtk_list_box_remove (self->device_list, GTK_WIDGET (row));
    }

  for (unsigned int i = 0; i < added; i++)
    {
      g_autoptr (GObject) item = NULL;
      g_autoptr (GtkWidget) widget = NULL;

      item = g_list_model_get_item (list, position + i);
      widget = valent_share_dialog_create_row (item, self);

      if (g_object_is_floating (widget))
        g_object_ref_sink (widget);

      gtk_list_box_insert (self->device_list, widget, position + i);
    }

  on_selected_changed (self);
}

static gboolean
valent_share_dialog_refresh (gpointer data)
{
  ValentDeviceManager *manager = VALENT_DEVICE_MANAGER (data);

  g_assert (VALENT_IS_DEVICE_MANAGER (manager));

  valent_device_manager_refresh (manager);

  return G_SOURCE_CONTINUE;
}

/*
 * URI Selector
 */
static void
on_uri_activated (GtkEditable       *editable,
                  ValentShareDialog *self)
{
  const char *text = NULL;

  g_assert (VALENT_IS_SHARE_DIALOG (self));

  text = gtk_editable_get_text (editable);
  if (text == NULL || *text == '\0')
    return;

  if (!gtk_widget_has_css_class (GTK_WIDGET (self->uri_entry), "error"))
    {
      g_autoptr (GListStore) files = NULL;
      g_autoptr (GFile) file = NULL;

      file = g_file_new_for_uri (text);
      files = g_list_store_new (G_TYPE_FILE);
      g_list_store_append (files, file);

      valent_share_dialog_set_files (self, G_LIST_MODEL (files));
    }
}

static void
on_uri_changed (GtkEditable       *editable,
                ValentShareDialog *self)
{
  const char *text = NULL;
  const char *scheme = NULL;

  g_assert (VALENT_IS_SHARE_DIALOG (self));

  text = gtk_editable_get_text (editable);
  if (text == NULL || *text == '\0')
    {
      gtk_widget_remove_css_class (GTK_WIDGET (self->uri_entry), "error");
      gtk_accessible_reset_state (GTK_ACCESSIBLE (self->uri_entry),
                                  GTK_ACCESSIBLE_STATE_INVALID);
      return;
    }

  scheme = g_uri_peek_scheme (text);
  if (scheme != NULL && !g_str_equal (scheme, "file"))
    {
      gtk_widget_remove_css_class (GTK_WIDGET (self->uri_entry), "error");
      gtk_accessible_reset_state (GTK_ACCESSIBLE (self->uri_entry),
                                  GTK_ACCESSIBLE_STATE_INVALID);
    }
  else
    {
      gtk_widget_add_css_class (GTK_WIDGET (self->uri_entry), "error");
      gtk_accessible_update_state (GTK_ACCESSIBLE (self->uri_entry),
                                   GTK_ACCESSIBLE_STATE_INVALID, TRUE,
                                   -1);
    }
}

/*
 * GAction
 */
static void
gtk_file_dialog_open_multiple_cb (GtkFileDialog     *dialog,
                                  GAsyncResult      *result,
                                  ValentShareDialog *self)
{
  g_autoptr (GListModel) files = NULL;
  g_autoptr (GError) error = NULL;

  files = gtk_file_dialog_open_multiple_finish (dialog, result, &error);

  if (files == NULL)
    {
      if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED) &&
          !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  valent_share_dialog_set_files (self, files);
}

static void
chooser_select_files_action (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *parameter)
{
  ValentShareDialog *self = VALENT_SHARE_DIALOG (widget);
  g_autoptr (GtkFileDialog) dialog = NULL;

  g_assert (VALENT_IS_SHARE_DIALOG (self));

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_open_multiple (dialog,
                                 GTK_WINDOW (self),
                                 NULL,
                                 (GAsyncReadyCallback)gtk_file_dialog_open_multiple_cb,
                                 self);
}

static void
chooser_share_action (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *parameter)
{
  ValentShareDialog *self = VALENT_SHARE_DIALOG (widget);
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self->device_list));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (!VALENT_IS_SHARE_DIALOG_ROW (child))
        continue;

      if (!valent_share_dialog_row_get_selected (VALENT_SHARE_DIALOG_ROW (child)))
        continue;

      valent_share_dialog_share (self, VALENT_SHARE_DIALOG_ROW (child));
    }
}

/*
 * ValentShareDialog
 */
static void
valent_share_dialog_set_files (ValentShareDialog *self,
                               GListModel        *files)
{
  unsigned int n_items = 0;

  g_assert (VALENT_IS_SHARE_DIALOG (self));
  g_assert (files == NULL || G_IS_LIST_MODEL (files));

  if (!g_set_object (&self->files, files))
    return;

  valent_share_dialog_reset (self);

  if (self->files != NULL)
    {
      n_items = g_list_model_get_n_items (files);
      g_signal_connect_object (self->files,
                               "items-changed",
                               G_CALLBACK (on_files_changed),
                               self,
                               G_CONNECT_DEFAULT);
      on_files_changed (self->files,
                        0,
                        self->n_links + self->n_files,
                        n_items,
                        self);
    }

  if (n_items > 0)
    adw_navigation_view_push_by_tag (self->view, "device");
  else
    adw_navigation_view_pop (self->view);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILES]);
}

static void
valent_share_dialog_set_selection_mode (ValentShareDialog *self,
                                        gboolean           selection_mode)
{
  g_assert (VALENT_IS_SHARE_DIALOG (self));

  selection_mode = !!selection_mode;
  if (self->selection_mode == selection_mode)
    return;

  gtk_list_box_set_selection_mode (self->device_list,
                                   selection_mode
                                    ? GTK_SELECTION_MULTIPLE
                                    : GTK_SELECTION_NONE);

  self->selection_mode = selection_mode;
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SELECTION_MODE]);
}

static void
valent_share_dialog_share (ValentShareDialog    *self,
                           ValentShareDialogRow *row)
{
  ValentDevice *device = NULL;
  GVariantBuilder builder;
  unsigned int n_files = 0;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);
  n_files = g_list_model_get_n_items (self->files);

  for (unsigned int i = 0; i < n_files; i++)
    {
      g_autoptr (GFile) file = g_list_model_get_item (self->files, i);
      GVariant *uri = g_variant_new_take_string (g_file_get_uri (file));

      g_variant_builder_add_value (&builder, uri);
    }

  device = valent_share_dialog_row_get_device (row);
  g_action_group_activate_action (G_ACTION_GROUP (device),
                                  "share.uris",
                                  g_variant_builder_end (&builder));

  gtk_window_close (GTK_WINDOW (self));
}

/*
 * GObject
 */
static void
valent_share_dialog_constructed (GObject *object)
{
  ValentShareDialog *self = VALENT_SHARE_DIALOG (object);

  self->manager = valent_device_manager_get_default ();
  g_signal_connect_object (self->manager,
                           "items-changed",
                           G_CALLBACK (on_items_changed),
                           self, 0);
  on_items_changed (G_LIST_MODEL (self->manager),
                    0,
                    0,
                    g_list_model_get_n_items (G_LIST_MODEL (self->manager)),
                    self);

  /* Broadcast every 5 seconds to re-connect devices that may have gone idle */
  valent_device_manager_refresh (self->manager);
  self->refresh_id = g_timeout_add_seconds_full (G_PRIORITY_LOW,
                                                 5,
                                                 valent_share_dialog_refresh,
                                                 g_object_ref (self->manager),
                                                 g_object_unref);

  G_OBJECT_CLASS (valent_share_dialog_parent_class)->constructed (object);
}

static void
valent_share_dialog_dispose (GObject *object)
{
  ValentShareDialog *self = VALENT_SHARE_DIALOG (object);

  g_clear_handle_id (&self->refresh_id, g_source_remove);

  if (self->manager != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->manager, self);
      self->manager = NULL;
    }

  if (self->cancellable != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  g_clear_object (&self->files);
  g_clear_pointer (&self->rows, g_ptr_array_unref);

  gtk_widget_dispose_template (GTK_WIDGET (object),
                               VALENT_TYPE_SHARE_DIALOG);

  G_OBJECT_CLASS (valent_share_dialog_parent_class)->dispose (object);
}

static void
valent_share_dialog_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentShareDialog *self = VALENT_SHARE_DIALOG (object);

  switch (prop_id)
    {
    case PROP_FILES:
      g_value_set_object (value, self->files);
      break;

    case PROP_SELECTION_MODE:
      g_value_set_boolean (value, self->selection_mode);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_share_dialog_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ValentShareDialog *self = VALENT_SHARE_DIALOG (object);

  switch (prop_id)
    {
    case PROP_FILES:
      valent_share_dialog_set_files (self, g_value_get_object (value));
      break;

    case PROP_SELECTION_MODE:
      valent_share_dialog_set_selection_mode (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_share_dialog_class_init (ValentShareDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_share_dialog_constructed;
  object_class->dispose = valent_share_dialog_dispose;
  object_class->get_property = valent_share_dialog_get_property;
  object_class->set_property = valent_share_dialog_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-share-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentShareDialog, view);
  gtk_widget_class_bind_template_child (widget_class, ValentShareDialog, device_list);
  gtk_widget_class_bind_template_child (widget_class, ValentShareDialog, single_row);
  gtk_widget_class_bind_template_child (widget_class, ValentShareDialog, single_icon);
  gtk_widget_class_bind_template_child (widget_class, ValentShareDialog, multiple_row);
  gtk_widget_class_bind_template_child (widget_class, ValentShareDialog, multiple_icon);
  gtk_widget_class_bind_template_child (widget_class, ValentShareDialog, uri_entry);
  gtk_widget_class_bind_template_callback (widget_class, on_device_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_uri_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_uri_changed);

  gtk_widget_class_install_action (widget_class, "chooser.share", NULL, chooser_share_action);
  gtk_widget_class_install_action (widget_class, "chooser.select-files", NULL, chooser_select_files_action);

  /**
   * ValentShareDialog:files:
   *
   * The URIs to share.
   */
  properties [PROP_FILES] =
    g_param_spec_object ("files", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentShareDialog:selection-mode:
   *
   * Whether multiple devices can be selected.
   */
  properties [PROP_SELECTION_MODE] =
    g_param_spec_boolean ("selection-mode", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_share_dialog_init (ValentShareDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

