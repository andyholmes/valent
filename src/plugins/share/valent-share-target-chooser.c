// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-share-target-chooser"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-share-target-chooser.h"


struct _ValentShareTargetChooser
{
  GtkWindow            parent_instance;

  ValentDeviceManager *manager;
  GListModel          *files;
  unsigned int         refresh_id;

  /* template */
  GtkListBox          *device_list;
};

G_DEFINE_FINAL_TYPE (ValentShareTargetChooser, valent_share_target_chooser, GTK_TYPE_WINDOW)

enum {
  PROP_0,
  PROP_DEVICE_MANAGER,
  PROP_FILES,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


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

static GtkWidget *
valent_share_target_chooser_create_row (gpointer item,
                                        gpointer user_data)
{
  ValentShareTargetChooser *self = VALENT_SHARE_TARGET_CHOOSER (user_data);
  ValentDevice *device = VALENT_DEVICE (item);
  GtkWidget *row;

  g_assert (VALENT_IS_DEVICE (device));

  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "activatable",  TRUE,
                      "selectable",   FALSE,
                      NULL);

  g_object_bind_property (self->device_list, "activate-on-single-click",
                          row,               "selectable",
                          G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE);
  g_object_bind_property (device, "icon-name",
                          row,    "icon-name",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (device, "name",
                          row,    "title",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_set_data_full (G_OBJECT (row),
                          "device",
                          g_object_ref (device),
                          g_object_unref);

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
on_items_changed (GListModel               *list,
                  unsigned int              position,
                  unsigned int              removed,
                  unsigned int              added,
                  ValentShareTargetChooser *self)
{
  g_assert (VALENT_IS_SHARE_TARGET_CHOOSER (self));

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
      widget = valent_share_target_chooser_create_row (item, self);

      if (g_object_is_floating (widget))
        g_object_ref_sink (widget);

      gtk_list_box_insert (self->device_list, widget, position + i);
    }
}

static void
on_row_activated (GtkListBox               *box,
                  GtkListBoxRow            *row,
                  ValentShareTargetChooser *self)
{
  ValentDevice *device = NULL;
  GVariantBuilder builder;
  unsigned int n_files = 0;

  g_assert (GTK_IS_LIST_BOX (box));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (VALENT_IS_SHARE_TARGET_CHOOSER (self));

  g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);
  n_files = g_list_model_get_n_items (self->files);

  for (unsigned int i = 0; i < n_files; i++)
    {
      g_autoptr (GFile) file = g_list_model_get_item (self->files, i);
      GVariant *uri = g_variant_new_take_string (g_file_get_uri (file));

      g_variant_builder_add_value (&builder, uri);
    }

  device = g_object_get_data (G_OBJECT (row), "device");
  g_action_group_activate_action (G_ACTION_GROUP (device),
                                  "share.uris",
                                  g_variant_builder_end (&builder));

  gtk_window_close (GTK_WINDOW (self));
}

static void
on_selected_rows_changed (GtkListBox               *box,
                          ValentShareTargetChooser *self)
{
  g_autoptr (GList) rows = NULL;
  unsigned int n_rows = 0;
  unsigned int n_files = 0;

  g_assert (GTK_IS_LIST_BOX (box));
  g_assert (VALENT_IS_SHARE_TARGET_CHOOSER (self));

  if ((rows = gtk_list_box_get_selected_rows (box)) != NULL)
    n_rows = g_list_length (rows);

  if (self->files != NULL)
    n_files = g_list_model_get_n_items (self->files);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "chooser.open",
                                 (n_rows > 0 && n_files == 1));
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "chooser.share",
                                 (n_rows > 0 && n_files >= 1));
}

static gboolean
valent_share_target_chooser_refresh (gpointer data)
{
  ValentDeviceManager *manager = VALENT_DEVICE_MANAGER (data);

  g_assert (VALENT_IS_DEVICE_MANAGER (manager));

  valent_device_manager_refresh (manager);

  return G_SOURCE_CONTINUE;
}

/*
 * GAction
 */
static void
chooser_cancel_action (GtkWidget  *widget,
                       const char *action_name,
                       GVariant   *parameter)
{
  ValentShareTargetChooser *self = VALENT_SHARE_TARGET_CHOOSER (widget);

  g_assert (VALENT_IS_SHARE_TARGET_CHOOSER (self));

  gtk_window_destroy (GTK_WINDOW (self));
}

static void
chooser_open_action (GtkWidget  *widget,
                     const char *action_name,
                     GVariant   *parameter)
{
  ValentShareTargetChooser *self = VALENT_SHARE_TARGET_CHOOSER (widget);
  GtkListBoxRow *row;
  ValentDevice *device = NULL;
  g_autoptr (GFile) file = NULL;
  GVariant *target = NULL;

  g_assert (VALENT_IS_SHARE_TARGET_CHOOSER (self));

  if ((row = gtk_list_box_get_selected_row (self->device_list)) == NULL ||
      g_list_model_get_n_items (self->files) == 0)
    return;

  device = g_object_get_data (G_OBJECT (row), "device");
  file = g_list_model_get_item (self->files, 0);
  target = g_variant_new_take_string (g_file_get_uri (file));

  g_action_group_activate_action (G_ACTION_GROUP (device), "share.open", target);

  gtk_window_close (GTK_WINDOW (self));
}

static void
chooser_share_action (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *parameter)
{
  ValentShareTargetChooser *self = VALENT_SHARE_TARGET_CHOOSER (widget);
  GtkListBoxRow *row;

  row = gtk_list_box_get_selected_row (self->device_list);
  on_row_activated (self->device_list, row, self);
}

/*
 * GObject
 */
static void
valent_share_target_chooser_constructed (GObject *object)
{
  ValentShareTargetChooser *self = VALENT_SHARE_TARGET_CHOOSER (object);

  g_assert (VALENT_IS_DEVICE_MANAGER (self->manager));
  g_assert (G_IS_LIST_MODEL (self->files));

  g_signal_connect_object (self->manager,
                           "items-changed",
                           G_CALLBACK (on_items_changed),
                           self, 0);
  on_items_changed (G_LIST_MODEL (self->manager),
                    0,
                    0,
                    g_list_model_get_n_items (G_LIST_MODEL (self->manager)),
                    self);
  on_selected_rows_changed (self->device_list, self);

  /* Broadcast every 5 seconds to re-connect devices that may have gone idle */
  valent_device_manager_refresh (self->manager);
  self->refresh_id = g_timeout_add_seconds_full (G_PRIORITY_LOW,
                                                 5,
                                                 valent_share_target_chooser_refresh,
                                                 g_object_ref (self->manager),
                                                 g_object_unref);

  G_OBJECT_CLASS (valent_share_target_chooser_parent_class)->constructed (object);
}

static void
valent_share_target_chooser_dispose (GObject *object)
{
  ValentShareTargetChooser *self = VALENT_SHARE_TARGET_CHOOSER (object);

  g_clear_handle_id (&self->refresh_id, g_source_remove);

  if (self->manager != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->manager, self);
      g_clear_object (&self->manager);
    }

  gtk_widget_dispose_template (GTK_WIDGET (object),
                               VALENT_TYPE_SHARE_TARGET_CHOOSER);

  G_OBJECT_CLASS (valent_share_target_chooser_parent_class)->dispose (object);
}

static void
valent_share_target_chooser_finalize (GObject *object)
{
  ValentShareTargetChooser *self = VALENT_SHARE_TARGET_CHOOSER (object);

  g_clear_object (&self->files);

  G_OBJECT_CLASS (valent_share_target_chooser_parent_class)->finalize (object);
}

static void
valent_share_target_chooser_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  ValentShareTargetChooser *self = VALENT_SHARE_TARGET_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_DEVICE_MANAGER:
      g_value_set_object (value, self->manager);
      break;

    case PROP_FILES:
      g_value_set_object (value, self->files);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_share_target_chooser_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  ValentShareTargetChooser *self = VALENT_SHARE_TARGET_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_DEVICE_MANAGER:
      self->manager = g_value_dup_object (value);
      break;

    case PROP_FILES:
      self->files = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_share_target_chooser_class_init (ValentShareTargetChooserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_share_target_chooser_constructed;
  object_class->dispose = valent_share_target_chooser_dispose;
  object_class->finalize = valent_share_target_chooser_finalize;
  object_class->get_property = valent_share_target_chooser_get_property;
  object_class->set_property = valent_share_target_chooser_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/share/valent-share-target-chooser.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentShareTargetChooser, device_list);
  gtk_widget_class_bind_template_callback (widget_class, on_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_selected_rows_changed);

  gtk_widget_class_install_action (widget_class, "chooser.cancel", NULL, chooser_cancel_action);
  gtk_widget_class_install_action (widget_class, "chooser.open", NULL, chooser_open_action);
  gtk_widget_class_install_action (widget_class, "chooser.share", NULL, chooser_share_action);

  /**
   * ValentShareTargetChooser:device-manager:
   *
   * The [class@Valent.DeviceManager] that the window represents.
   */
  properties [PROP_DEVICE_MANAGER] =
    g_param_spec_object ("device-manager", NULL, NULL,
                         VALENT_TYPE_DEVICE_MANAGER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentShareTargetChooser:files:
   *
   * The URIs to pass to the selected [class@Valent.Device].
   */
  properties [PROP_FILES] =
    g_param_spec_object ("files", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_share_target_chooser_init (ValentShareTargetChooser *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

