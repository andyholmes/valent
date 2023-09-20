// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <glib/gi18n.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <libvalent-core.h>
#include <libvalent-device.h>

#include "valent-device-gadget.h"
#include "valent-device-page.h"
#include "valent-device-preferences-window.h"
#include "valent-menu-list.h"
#include "valent-menu-stack.h"


struct _ValentDevicePage
{
  AdwNavigationPage  parent_instance;

  ValentDevice      *device;
  GHashTable        *plugins;
  GtkWindow         *preferences;

  /* template */
  GtkStack          *stack;

  GtkWidget         *pair_request;
  GtkSpinner        *pair_spinner;
  GtkWidget         *verification_key;

  GtkWidget         *gadgets;
  ValentMenuStack   *menu_actions;
};

G_DEFINE_FINAL_TYPE (ValentDevicePage, valent_device_page, ADW_TYPE_NAVIGATION_PAGE)

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
  GtkWidget *gadgets;
  GtkWidget *gadget;
} PluginData;

static void
plugin_data_free (gpointer data)
{
  PluginData *plugin = (PluginData *)data;

  if (plugin->gadgets != NULL && plugin->gadget != NULL)
    gtk_box_remove (GTK_BOX (plugin->gadgets), plugin->gadget);

  g_free (plugin);
}

static void
valent_device_page_add_plugin (ValentDevicePage *self,
                               const char       *module)
{
  PeasEngine *engine;
  PeasPluginInfo *info;
  PluginData *plugin;

  g_assert (VALENT_IS_DEVICE_PAGE (self));
  g_assert (module != NULL && *module != '\0');

  engine = valent_get_plugin_engine ();
  info = peas_engine_get_plugin_info (engine, module);
  plugin = g_new0 (PluginData, 1);

  /* Gadgets (eg. HeaderBar widgets) */
  if (peas_engine_provides_extension (engine, info, VALENT_TYPE_DEVICE_GADGET))
    {
      GObject *gadget;

      gadget = peas_engine_create_extension (engine,
                                             info,
                                             VALENT_TYPE_DEVICE_GADGET,
                                             "device", self->device,
                                             NULL);

      if (gadget != NULL)
        {
          gtk_box_append (GTK_BOX (self->gadgets), GTK_WIDGET (gadget));
          plugin->gadgets = GTK_WIDGET (self->gadgets);
          plugin->gadget = GTK_WIDGET (gadget);
        }
    }

  g_hash_table_replace (self->plugins,
                        g_strdup (module),
                        g_steal_pointer (&plugin));
}

static void
on_plugins_changed (ValentDevice     *device,
                    GParamSpec       *pspec,
                    ValentDevicePage *self)
{
  g_auto (GStrv) plugins = NULL;
  GHashTableIter iter;
  const char *module;

  plugins = valent_device_get_plugins (device);
  g_hash_table_iter_init (&iter, self->plugins);

  while (g_hash_table_iter_next (&iter, (void **)&module, NULL))
    {
      if (!g_strv_contains ((const char * const *)plugins, module))
        g_hash_table_iter_remove (&iter);
    }

  for (unsigned int i = 0; plugins[i] != NULL; i++)
    {
      if (!g_hash_table_contains (self->plugins, plugins[i]))
        valent_device_page_add_plugin (self, plugins[i]);
    }
}

/*
 * Pairing
 */
static void
on_state_changed (ValentDevice     *device,
                  GParamSpec       *pspec,
                  ValentDevicePage *self)
{
  ValentDeviceState state = VALENT_DEVICE_STATE_NONE;
  gboolean connected, paired;

  g_assert (VALENT_IS_DEVICE (device));
  g_assert (VALENT_IS_DEVICE_PAGE (self));

  state = valent_device_get_state (self->device);
  connected = (state & VALENT_DEVICE_STATE_CONNECTED) != 0;
  paired = (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  /* Ensure the proper controls are displayed */
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "page.pair", !paired);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "page.unpair", paired);

  if (!connected)
    {
      gtk_stack_set_visible_child_name (self->stack, "disconnected");
    }
  else if (!paired)
    {
      g_autoptr (ValentChannel) channel = NULL;
      const char *verification_key = NULL;
      gboolean pair_incoming, pair_outgoing;

      /* Get the channel verification key */
      if ((channel = valent_device_ref_channel (self->device)) != NULL)
        verification_key = valent_channel_get_verification_key (channel);
      else
        verification_key = _("Unavailable");

      gtk_label_set_text (GTK_LABEL (self->verification_key), verification_key);

      /* Adjust the actions */
      pair_incoming = (state & VALENT_DEVICE_STATE_PAIR_INCOMING) != 0;
      pair_outgoing = (state & VALENT_DEVICE_STATE_PAIR_OUTGOING) != 0;

      gtk_widget_set_visible (self->pair_request, !pair_incoming);
      gtk_widget_set_sensitive (self->pair_request, !pair_outgoing);
      gtk_spinner_set_spinning (self->pair_spinner, pair_outgoing);

      gtk_stack_set_visible_child_name (self->stack, "pairing");
    }
  else
    {
      gtk_stack_set_visible_child_name (self->stack, "connected");
    }
}

static void
page_preferences_action (GtkWidget  *widget,
                         const char *action_name,
                         GVariant   *parameter)
{
  ValentDevicePage *self = VALENT_DEVICE_PAGE (widget);

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
                                        "modal",          FALSE,
                                        "transient-for",  window,
                                        NULL);

      g_object_add_weak_pointer (G_OBJECT (self->preferences),
                                 (gpointer)&self->preferences);
    }

  gtk_window_present (self->preferences);
}

static void
page_pair_action (GtkWidget  *widget,
                  const char *action_name,
                  GVariant   *parameter)
{
  ValentDevicePage *self = VALENT_DEVICE_PAGE (widget);

  g_assert (VALENT_IS_DEVICE (self->device));

  g_action_group_activate_action (G_ACTION_GROUP (self->device), "pair", NULL);
}

static void
page_unpair_action (GtkWidget  *widget,
                    const char *action_name,
                    GVariant   *parameter)
{
  ValentDevicePage *self = VALENT_DEVICE_PAGE (widget);

  g_assert (VALENT_IS_DEVICE (self->device));

  g_action_group_activate_action (G_ACTION_GROUP (self->device), "unpair", NULL);
}

/*
 * GObject
 */
static void
valent_device_page_constructed (GObject *object)
{
  ValentDevicePage *self = VALENT_DEVICE_PAGE (object);
  GMenuModel *menu;

  g_object_bind_property (self->device, "id",
                          self,         "tag",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self->device, "name",
                          self,         "title",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  /* Actions & Menu */
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "device",
                                  G_ACTION_GROUP (self->device));

  menu = valent_device_get_menu (self->device);
  valent_menu_stack_set_menu_model (self->menu_actions, menu);

  /* Pair Section */
  g_signal_connect_object (self->device,
                           "notify::state",
                           G_CALLBACK (on_state_changed),
                           self, 0);
  on_state_changed (self->device, NULL, self);

  /* Plugin Gadgets */
  g_signal_connect_object (self->device,
                           "notify::plugins",
                           G_CALLBACK (on_plugins_changed),
                           self, 0);
  on_plugins_changed (self->device, NULL, self);

  G_OBJECT_CLASS (valent_device_page_parent_class)->constructed (object);
}

static void
valent_device_page_dispose (GObject *object)
{
  ValentDevicePage *self = VALENT_DEVICE_PAGE (object);

  g_clear_object (&self->device);
  g_clear_pointer (&self->plugins, g_hash_table_unref);
  g_clear_pointer (&self->preferences, gtk_window_destroy);

  gtk_widget_dispose_template (GTK_WIDGET (object), VALENT_TYPE_DEVICE_PAGE);

  G_OBJECT_CLASS (valent_device_page_parent_class)->dispose (object);
}

static void
valent_device_page_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ValentDevicePage *self = VALENT_DEVICE_PAGE (object);

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
valent_device_page_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ValentDevicePage *self = VALENT_DEVICE_PAGE (object);

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
valent_device_page_class_init (ValentDevicePageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_device_page_constructed;
  object_class->dispose = valent_device_page_dispose;
  object_class->get_property = valent_device_page_get_property;
  object_class->set_property = valent_device_page_set_property;

  /* template */
  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Valent/ui/valent-device-page.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePage, gadgets);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePage, stack);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePage, pair_request);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePage, pair_spinner);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePage, verification_key);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePage, menu_actions);

  gtk_widget_class_install_action (widget_class, "page.preferences", NULL, page_preferences_action);
  gtk_widget_class_install_action (widget_class, "page.pair", NULL, page_pair_action);
  gtk_widget_class_install_action (widget_class, "page.unpair", NULL, page_unpair_action);

  /**
   * ValentDevicePage:device:
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
valent_device_page_init (ValentDevicePage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->plugins = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         plugin_data_free);
}

