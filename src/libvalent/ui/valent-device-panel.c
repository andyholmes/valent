// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <libvalent-core.h>

#include "valent-device-gadget.h"
#include "valent-device-panel.h"
#include "valent-menu-stack.h"
#include "valent-plugin-preferences.h"
#include "valent-plugin-row.h"


struct _ValentDevicePanel
{
  GtkBox               parent_instance;

  ValentDevice        *device;
  GSettings           *settings;

  AdwWindowTitle      *title;
  GtkWidget           *stack;

  /* Main */
  GtkWidget           *pair_group;
  GtkWidget           *pair_request;
  GtkWidget           *pair_spinner;
  GtkWidget           *verification_key;

  GtkWidget           *connected_group;
  GtkWidget           *gadgets;
  ValentMenuStack     *menu_actions;

  /* Settings */
  AdwPreferencesGroup *general_group;
  GtkLabel            *download_folder_label;

  AdwPreferencesGroup *plugin_group;
  GtkListBox          *plugin_list;
  GHashTable          *plugins;

  AdwPreferencesGroup *unpair_group;
};

G_DEFINE_TYPE (ValentDevicePanel, valent_device_panel, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * Plugin Callbacks
 */
typedef struct
{
  GtkWidget *row;
  GtkWidget *preferences;
  GtkWidget *gadget;
  GtkWidget *activity;
} PluginWidgets;

static void
plugin_widgets_free (ValentDevicePanel *self,
                     PluginWidgets     *widgets)
{
  g_assert (VALENT_IS_DEVICE_PANEL (self));

  if (widgets->row != NULL)
    gtk_list_box_remove (self->plugin_list, widgets->row);

  if (widgets->preferences != NULL)
    gtk_stack_remove (GTK_STACK (self->stack), widgets->preferences);

  if (widgets->gadget != NULL)
    gtk_box_remove (GTK_BOX (self->gadgets), widgets->gadget);

  g_clear_pointer (&widgets->activity, gtk_widget_unparent);
  g_free (widgets);
}

static void
on_plugin_added (ValentDevice      *device,
                 PeasPluginInfo    *info,
                 ValentDevicePanel *self)
{
  PeasEngine *engine;
  const char *module;
  const char *title;
  const char *device_id;
  PluginWidgets *widgets;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (info != NULL);
  g_assert (VALENT_IS_DEVICE_PANEL (self));

  /* Track the plugin's widgets */
  widgets = g_new0 (PluginWidgets, 1);
  g_hash_table_insert (self->plugins, info, widgets);

  engine = valent_get_engine ();
  module = peas_plugin_info_get_module_name (info);
  title = peas_plugin_info_get_name (info);
  device_id = valent_device_get_id (device);

  /* Plugin Row */
  widgets->row = g_object_new (VALENT_TYPE_PLUGIN_ROW,
                               "plugin-context", device_id,
                               "plugin-info",    info,
                               "plugin-type",    VALENT_TYPE_DEVICE_PLUGIN,
                               NULL);
  gtk_list_box_insert (self->plugin_list, widgets->row, -1);

  /* Preferences */
  if (peas_engine_provides_extension (engine,
                                      info,
                                      VALENT_TYPE_PLUGIN_PREFERENCES))
    {
      PeasExtension *prefs;

      prefs = peas_engine_create_extension (engine,
                                            info,
                                            VALENT_TYPE_PLUGIN_PREFERENCES,
                                            "plugin-context", device_id,
                                            NULL);

      if (prefs != NULL)
        {
          widgets->preferences = GTK_WIDGET (prefs);
          gtk_stack_add_titled (GTK_STACK (self->stack), widgets->preferences,
                                module, title);
        }
    }

  /* Gadgets (eg. HeaderBar widgets) */
  if (peas_engine_provides_extension (engine, info, VALENT_TYPE_DEVICE_GADGET))
    {
      PeasExtension *gadget;

      gadget = peas_engine_create_extension (engine,
                                             info,
                                             VALENT_TYPE_DEVICE_GADGET,
                                             "device", device,
                                             NULL);

      if (gadget != NULL)
        {
          widgets->gadget = GTK_WIDGET (gadget);
          gtk_box_append (GTK_BOX (self->gadgets), widgets->gadget);
        }
    }
}

static void
on_plugin_removed (ValentDevice      *device,
                   PeasPluginInfo    *info,
                   ValentDevicePanel *self)
{
  gpointer widgets;

  if (g_hash_table_steal_extended (self->plugins, info, NULL, &widgets))
    plugin_widgets_free (self, widgets);
}

/*
 * Pairing
 */
static void
on_state_changed (ValentDevice      *device,
                  GParamSpec        *pspec,
                  ValentDevicePanel *self)
{
  ValentDeviceState state = 0;
  ValentChannel *channel;
  const char *verification_key = NULL;
  gboolean connected, paired, pair_incoming, pair_outgoing;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (VALENT_IS_DEVICE_PANEL (self));

  state = valent_device_get_state (self->device);
  connected = (state & VALENT_DEVICE_STATE_CONNECTED);
  paired = (state & VALENT_DEVICE_STATE_PAIRED);
  pair_incoming = (state & VALENT_DEVICE_STATE_PAIR_INCOMING);
  pair_outgoing = (state & VALENT_DEVICE_STATE_PAIR_OUTGOING);

  /* Ensure the proper controls are displayed */
  gtk_widget_set_visible (self->connected_group, connected);
  gtk_widget_set_visible (self->pair_group, !paired);

  if (paired)
    return;

  /* Get the channel verification key */
  channel = valent_device_get_channel (self->device);

  if (channel != NULL)
    verification_key = valent_channel_get_verification_key (channel);

  if (verification_key != NULL)
    gtk_label_set_text (GTK_LABEL (self->verification_key), verification_key);
  else
    gtk_label_set_text (GTK_LABEL (self->verification_key), _("Unavailable"));

  /* Adjust the actions */
  gtk_widget_set_visible (self->pair_spinner, pair_outgoing);
  gtk_widget_set_visible (self->pair_request, !pair_incoming);
  gtk_widget_set_sensitive (self->pair_request, !pair_outgoing);
}

/*
 * Download Folder
 */
static gboolean
on_download_folder_changed (GValue   *value,
                            GVariant *variant,
                            gpointer  user_data)
{
  const char *label;
  g_autofree char *basename = NULL;
  g_autofree char *result = NULL;

  label = g_variant_get_string (variant, NULL);
  basename = g_path_get_basename (label);
  result = g_strdup_printf ("…/%s", basename);

  g_value_set_string (value, result);

  return TRUE;
}

static void
on_download_folder_response (GtkNativeDialog   *dialog,
                             int                response_id,
                             ValentDevicePanel *self)
{
  g_autoptr (GFile) file = NULL;

  g_assert (VALENT_IS_DEVICE_PANEL (self));

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      const char *path;

      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
      path = g_file_peek_path (file);
      g_settings_set_string (self->settings, "download-folder", path);
    }

  gtk_native_dialog_destroy (dialog);
}

static void
on_download_folder_clicked (AdwActionRow      *row,
                            ValentDevicePanel *self)
{
  GtkNativeDialog *dialog;
  GtkRoot *root;
  g_autofree char *path = NULL;

  g_assert (VALENT_IS_DEVICE_PANEL (self));

  root = gtk_widget_get_root (GTK_WIDGET (self));
  dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_NATIVE,
                         "modal",         (root != NULL),
                         "transient-for", root,
                         "action",        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                         "title",         _("Select download folder"),
                         "accept-label",  _("Open"),
                         "cancel-label",  _("Cancel"),
                         NULL);

  path = g_settings_get_string (self->settings, "download-folder");

  if (strlen (path) > 0)
    {
      g_autoptr (GFile) file = NULL;

      file = g_file_new_for_path (path);
      gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
                                           file, NULL);
    }

  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (on_download_folder_response),
                    self);

  gtk_native_dialog_show (dialog);
}

/*
 * GObject
 */
static void
valent_device_panel_constructed (GObject *object)
{
  ValentDevicePanel *self = VALENT_DEVICE_PANEL (object);
  g_autofree char *path = NULL;
  g_autoptr (GPtrArray) plugins = NULL;
  GActionGroup *actions;
  GMenuModel *menu;

  g_object_bind_property (self->device,
                          "name",
                          self->title,
                          "title",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  /* Actions & Menu */
  actions = valent_device_get_actions (self->device);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "device", actions);

  menu = valent_device_get_menu (self->device);
  valent_menu_stack_bind_model (self->menu_actions, menu);

  /* Pair Section */
  g_signal_connect (self->device,
                    "notify::state",
                    G_CALLBACK (on_state_changed),
                    self);
  on_state_changed (self->device, NULL, self);

  /* GSettings*/
  path = g_strdup_printf ("/ca/andyholmes/valent/device/%s/",
                          valent_device_get_id (self->device));
  self->settings = g_settings_new_with_path ("ca.andyholmes.Valent.Device", path);

  g_settings_bind_with_mapping (self->settings,              "download-folder",
                                self->download_folder_label, "label",
                                G_SETTINGS_BIND_GET,
                                on_download_folder_changed,
                                NULL,
                                NULL, NULL);

  /* Plugin list */
  plugins = valent_device_get_plugins (self->device);

  for (unsigned int i = 0; i < plugins->len; i++)
    on_plugin_added (self->device, g_ptr_array_index (plugins, i), self);

  g_signal_connect (self->device,
                    "plugin-added",
                    G_CALLBACK (on_plugin_added),
                    self);
  g_signal_connect (self->device,
                    "plugin-removed",
                    G_CALLBACK (on_plugin_removed),
                    self);

  G_OBJECT_CLASS (valent_device_panel_parent_class)->constructed (object);
}

static void
valent_device_panel_dispose (GObject *object)
{
  ValentDevicePanel *self = VALENT_DEVICE_PANEL (object);

  g_signal_handlers_disconnect_by_data (self->device, self);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (valent_device_panel_parent_class)->dispose (object);
}

static void
valent_device_panel_finalize (GObject *object)
{
  ValentDevicePanel *self = VALENT_DEVICE_PANEL (object);

  g_clear_pointer (&self->plugins, g_hash_table_unref);

  G_OBJECT_CLASS (valent_device_panel_parent_class)->finalize (object);
}

static void
valent_device_panel_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentDevicePanel *self = VALENT_DEVICE_PANEL (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_panel_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ValentDevicePanel *self = VALENT_DEVICE_PANEL (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_panel_class_init (ValentDevicePanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_device_panel_constructed;
  object_class->dispose = valent_device_panel_dispose;
  object_class->finalize = valent_device_panel_finalize;
  object_class->get_property = valent_device_panel_get_property;
  object_class->set_property = valent_device_panel_set_property;

  /* Template */
  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Valent/ui/valent-device-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePanel, title);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePanel, gadgets);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePanel, stack);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePanel, pair_group);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePanel, pair_request);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePanel, pair_spinner);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePanel, verification_key);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePanel, connected_group);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePanel, menu_actions);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePanel, general_group);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePanel, download_folder_label);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePanel, plugin_group);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePanel, plugin_list);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePanel, unpair_group);

  gtk_widget_class_bind_template_callback (widget_class, on_download_folder_clicked);

  /**
   * ValentDevicePanel:device:
   *
   * The device this panel controls and represents.
   */
  properties [PROP_DEVICE] =
    g_param_spec_object ("device",
                         "device",
                         "The device for this settings widget",
                         VALENT_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_device_panel_init (ValentDevicePanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (self->plugin_list,
                              valent_plugin_preferences_row_sort,
                              NULL,
                              NULL);
  self->plugins = g_hash_table_new_full (NULL, NULL, NULL, g_free);
}

/**
 * valent_device_panel_new:
 * @device: a #ValentDevice
 *
 * Create a new panel (with headerbar) for @device.
 *
 * Returns: (transfer full): a new #GtkWidget
 */
GtkWidget *
valent_device_panel_new (ValentDevice *device)
{
  return g_object_new (VALENT_TYPE_DEVICE_PANEL,
                       "device", device,
                       NULL);
}

