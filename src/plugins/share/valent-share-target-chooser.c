// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-share-target-chooser"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>

#include "valent-share-target-chooser.h"


struct _ValentShareTargetChooser
{
  GtkWindow            parent_instance;

  ValentDeviceManager *manager;
  GListModel          *files;

  GtkListBox          *device_list;
  GHashTable          *device_rows;

  unsigned int         refresh_id;
};

G_DEFINE_TYPE (ValentShareTargetChooser, valent_share_target_chooser, GTK_TYPE_WINDOW)

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
  ValentDevice *device = VALENT_DEVICE (item);
  GtkWidget *row;

  g_assert (VALENT_IS_DEVICE (device));

  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "activatable",  TRUE,
                      "selectable",   FALSE,
                      "margin-top",   12,
                      "margin-start", 12,
                      "margin-end",   12,
                      NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (row), "card");

  g_object_bind_property (device, "icon-name",
                          row,    "icon-name",
                          G_BINDING_DEFAULT);
  g_object_bind_property (device, "name",
                          row,    "title",
                          G_BINDING_DEFAULT);
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

static gboolean
refresh_cb (gpointer data)
{
  ValentShareTargetChooser *self = VALENT_SHARE_TARGET_CHOOSER (data);

  g_assert (VALENT_IS_SHARE_TARGET_CHOOSER (self));

  valent_device_manager_identify (self->manager, NULL);

  return TRUE;
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

  gtk_list_box_bind_model (self->device_list,
                           G_LIST_MODEL (self->manager),
                           valent_share_target_chooser_create_row,
                           NULL, NULL);

  /* Broadcast every 5 seconds to re-connect devices that may have gone idle */
  valent_device_manager_identify (self->manager, NULL);
  self->refresh_id = g_timeout_add_seconds (5, refresh_cb, self);

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

  G_OBJECT_CLASS (valent_share_target_chooser_parent_class)->dispose (object);
}

static void
valent_share_target_chooser_finalize (GObject *object)
{
  ValentShareTargetChooser *self = VALENT_SHARE_TARGET_CHOOSER (object);

  g_clear_pointer (&self->device_rows, g_hash_table_unref);
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

  self->device_rows = g_hash_table_new (NULL, NULL);
}

