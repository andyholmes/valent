// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <glib/gi18n.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <libvalent-core.h>
#include <libvalent-device.h>

#include "valent-device-gadget.h"
#include "valent-device-panel.h"
#include "valent-device-preferences-window.h"
#include "valent-menu-list.h"
#include "valent-menu-stack.h"


struct _ValentDevicePanel
{
  GtkBox               parent_instance;

  ValentDevice        *device;

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

  GHashTable          *plugins;
  GtkWindow           *preferences;
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
  GtkWidget *gadget;
} PluginWidgets;

static void
plugin_widgets_free (ValentDevicePanel *self,
                     PluginWidgets     *widgets)
{
  g_assert (VALENT_IS_DEVICE_PANEL (self));

  if (widgets->gadget != NULL)
    gtk_box_remove (GTK_BOX (self->gadgets), widgets->gadget);

  g_free (widgets);
}

static void
on_plugin_added (ValentDevice      *device,
                 PeasPluginInfo    *info,
                 ValentDevicePanel *self)
{
  PeasEngine *engine;
  PluginWidgets *widgets;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (info != NULL);
  g_assert (VALENT_IS_DEVICE_PANEL (self));

  /* Track the plugin's widgets */
  widgets = g_new0 (PluginWidgets, 1);
  g_hash_table_insert (self->plugins, info, widgets);

  engine = valent_get_plugin_engine ();

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
  ValentDeviceState state = VALENT_DEVICE_STATE_NONE;
  g_autoptr (ValentChannel) channel = NULL;
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
  if ((channel = valent_device_ref_channel (self->device)) != NULL)
    verification_key = valent_channel_get_verification_key (channel);
  else
    verification_key = _("Unavailable");

  gtk_label_set_text (GTK_LABEL (self->verification_key), verification_key);

  /* Adjust the actions */
  gtk_widget_set_visible (self->pair_spinner, pair_outgoing);
  gtk_widget_set_visible (self->pair_request, !pair_incoming);
  gtk_widget_set_sensitive (self->pair_request, !pair_outgoing);
}

static void
preferences_action (GtkWidget  *widget,
                    const char *action_name,
                    GVariant   *parameter)
{
  ValentDevicePanel *self = VALENT_DEVICE_PANEL (widget);

  if (self->preferences == NULL)
    {
      GtkAllocation allocation;
      GtkRoot *window;

      gtk_widget_get_allocation (widget, &allocation);
      window = gtk_widget_get_root (widget);

      self->preferences = g_object_new (VALENT_TYPE_DEVICE_PREFERENCES_WINDOW,
                                        "default-width",  allocation.width,
                                        "default-height", allocation.height,
                                        "device",         self->device,
                                        "transient-for",  window,
                                        NULL);

      g_object_add_weak_pointer (G_OBJECT (self->preferences),
                                 (gpointer)&self->preferences);
    }

  gtk_window_present (self->preferences);
}

/*
 * GObject
 */
static void
valent_device_panel_constructed (GObject *object)
{
  ValentDevicePanel *self = VALENT_DEVICE_PANEL (object);
  g_autoptr (GPtrArray) plugins = NULL;
  GMenuModel *menu;

  g_object_bind_property (self->device, "name",
                          self->title,  "title",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  /* Actions & Menu */
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "device",
                                  G_ACTION_GROUP (self->device));

  menu = valent_device_get_menu (self->device);
  valent_menu_stack_set_menu_model (self->menu_actions, menu);

  /* Pair Section */
  g_signal_connect (self->device,
                    "notify::state",
                    G_CALLBACK (on_state_changed),
                    self);
  on_state_changed (self->device, NULL, self);

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

  g_clear_pointer (&self->preferences, gtk_window_destroy);
  g_signal_handlers_disconnect_by_data (self->device, self);
  g_clear_object (&self->device);

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
      self->device = g_value_dup_object (value);
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

  gtk_widget_class_install_action (widget_class, "panel.preferences", NULL, preferences_action);

  /**
   * ValentDevicePanel:device:
   *
   * The device this panel controls and represents.
   */
  properties [PROP_DEVICE] =
    g_param_spec_object ("device", NULL, NULL,
                         VALENT_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /* Ensure the private types we need are ready */
  g_type_ensure (VALENT_TYPE_MENU_LIST);
  g_type_ensure (VALENT_TYPE_MENU_STACK);
}

static void
valent_device_panel_init (ValentDevicePanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->plugins = g_hash_table_new_full (NULL, NULL, NULL, g_free);
}

/**
 * valent_device_panel_close_preferences:
 * @panel: a #ValentDevicePanel
 *
 * Close the preferences page.
 *
 * This is called by [class@Valent.Window] when the `win.page` action is
 * activated, to ensure the new page is not blocked by a modal window.
 */
void
valent_device_panel_close_preferences (ValentDevicePanel *panel)
{
  g_assert (VALENT_IS_DEVICE_PANEL (panel));

  g_clear_pointer (&panel->preferences, gtk_window_destroy);
}

