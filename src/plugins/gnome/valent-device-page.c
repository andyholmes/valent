// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <glib/gi18n-lib.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <valent.h>

#include "valent-device-page.h"
#include "valent-device-preferences-dialog.h"
#include "valent-menu-list.h"
#include "valent-menu-stack.h"


struct _ValentDevicePage
{
  AdwNavigationPage  parent_instance;

  ValentDevice      *device;
  GHashTable        *plugins;
  AdwDialog         *preferences;

  /* template */
  GtkStack          *stack;
  GtkWidget         *gadgets;
  GtkWidget         *battery_status;
  GtkWidget         *battery_status_label;
  GtkWidget         *battery_status_level;
  GtkWidget         *connectivity_status;
  GtkWidget         *connectivity_status_box;
  AdwStatusPage     *pair_page;
  GtkWidget         *pair_request;
  GtkSpinner        *pair_spinner;
  GtkWidget         *verification_key;

  ValentMenuStack   *menu_actions;
};

G_DEFINE_FINAL_TYPE (ValentDevicePage, valent_device_page, ADW_TYPE_NAVIGATION_PAGE)

typedef enum {
  PROP_DEVICE = 1,
} ValentDevicePageProperty;

static GParamSpec *properties[PROP_DEVICE + 1] = { NULL, };

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
      g_autofree char *description = NULL;
      g_autofree char *verification_key = NULL;
      gboolean pair_incoming, pair_outgoing;

      pair_incoming = (state & VALENT_DEVICE_STATE_PAIR_INCOMING) != 0;
      pair_outgoing = (state & VALENT_DEVICE_STATE_PAIR_OUTGOING) != 0;

      /* Get the channel verification key */
      if (pair_incoming || pair_outgoing)
        {
          description = g_strdup_printf (_("Please confirm the verification key "
                                           "below matches the one on “%s”"),
                                         valent_device_get_name (self->device));
          verification_key = valent_device_get_verification_key (self->device);
        }
      else
        {
          description = g_strdup_printf (_("Open the app on your “%s” to "
                                           "request or accept pairing."),
                                         valent_device_get_name (self->device));
        }

      gtk_label_set_text (GTK_LABEL (self->verification_key), verification_key);
      adw_status_page_set_description (self->pair_page, description);

      /* Adjust the actions */
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

/*
 * Battery
 */
static void
on_battery_state_changed (GActionGroup     *action_group,
                         const char       *action_name,
                         GVariant         *value,
                         ValentDevicePage *self)
{
  g_autofree char *label = NULL;
  gboolean charging = FALSE;
  gboolean is_present = FALSE;
  double percentage = 0.0;
  const char *icon_name;

  g_assert (VALENT_IS_DEVICE_PAGE (self));

  g_variant_lookup (value, "is-present", "b", &is_present);
  if (!is_present)
    {
      gtk_widget_set_visible (self->battery_status, FALSE);
      return;
    }

  if (!g_variant_lookup (value, "percentage", "d", &percentage) ||
      !g_variant_lookup (value, "charging", "b", &charging))
    {
      gtk_widget_set_visible (self->battery_status, FALSE);
      return;
    }

  if (!g_variant_lookup (value, "icon-name", "&s", &icon_name))
    icon_name = "battery-missing-symbolic";

  if (percentage >= 100.0)
    {
      /* TRANSLATORS: When the battery level is 100%
       */
      label = g_strdup (_("Fully Charged"));
    }
  else
    {
      int64_t total_seconds = 0;
      unsigned int total_minutes;
      unsigned int minutes;
      unsigned int hours;

      if (charging)
        g_variant_lookup (value, "time-to-full", "x", &total_seconds);
      else
        g_variant_lookup (value, "time-to-empty", "x", &total_seconds);

      if (total_seconds > 0)
        {
          total_minutes = (unsigned int)floor (total_seconds / 60);
          minutes = total_minutes % 60;
          hours = (unsigned int)floor (total_minutes / 60);
        }

      if (total_seconds <= 0)
        {
          /* TRANSLATORS: This is <percentage> (Estimating…)
           */
          label = g_strdup_printf (_("%g%% (Estimating…)"), percentage);
        }
      else if (charging)
        {
          /* TRANSLATORS: This is <percentage> (<hours>:<minutes> Until Full)
           */
          label = g_strdup_printf (_("%g%% (%d∶%02d Until Full)"),
                                   percentage, hours, minutes);
        }
      else
        {
          /* TRANSLATORS: This is <percentage> (<hours>:<minutes> Remaining)
           */
          label = g_strdup_printf (_("%g%% (%d∶%02d Remaining)"),
                                   percentage, hours, minutes);
        }
    }

  if (g_action_group_get_action_enabled (action_group, action_name))
    {
      gtk_widget_set_visible (self->battery_status, TRUE);
      gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (self->battery_status),
                                     icon_name);

      gtk_label_set_text (GTK_LABEL (self->battery_status_label), label);
      gtk_level_bar_set_value (GTK_LEVEL_BAR (self->battery_status_level),
                               percentage);
    }
}

static void
on_battery_enabled_changed (GActionGroup     *action_group,
                            const char       *action_name,
                            gboolean          enabled,
                            ValentDevicePage *self)
{
  g_autoptr (GVariant) state = NULL;

  gtk_widget_set_visible (self->battery_status, enabled);

  if (enabled)
    state = g_action_group_get_action_state (action_group, action_name);

  if (state != NULL)
    on_battery_state_changed (action_group, action_name, state, self);
}

/*
 * Connectivity Status
 */
static void
on_connectivity_state_changed (GActionGroup     *action_group,
                               const char       *action_name,
                               GVariant         *value,
                               ValentDevicePage *self)
{
  GtkWidget *child;
  g_autoptr (GVariant) signal_strengths = NULL;
  GVariantIter iter;
  char *signal_id = NULL;
  GVariant *signal_state;
  const char *icon_name;
  const char *title;

  g_assert (VALENT_IS_DEVICE_PAGE (self));

  /* Clear the popup
   */
  while ((child = gtk_widget_get_first_child (self->connectivity_status_box)) != NULL)
    gtk_box_remove (GTK_BOX (self->connectivity_status_box), child);

  if (!g_variant_lookup (value, "signal-strengths", "@a{sv}", &signal_strengths))
    {
      gtk_widget_set_visible (self->connectivity_status, FALSE);
      return;
    }

  /* Add each signal
   */
  g_variant_iter_init (&iter, signal_strengths);

  while (g_variant_iter_loop (&iter, "{sv}", &signal_id, &signal_state))
    {
      GtkWidget *box;
      GtkWidget *level;
      GtkWidget *icon;
      const char *signal_icon;
      const char *network_type;
      int64_t signal_strength;

      box = g_object_new (GTK_TYPE_BOX,
                          "spacing", 6,
                          NULL);

      icon = g_object_new (GTK_TYPE_IMAGE,
                           "pixel-size", 16,
                           "valign",     GTK_ALIGN_CENTER,
                           NULL);
      gtk_box_append (GTK_BOX (box), icon);

      level = g_object_new (GTK_TYPE_LEVEL_BAR,
                            "mode",           GTK_LEVEL_BAR_MODE_DISCRETE,
                            "min-value",      0.0,
                            "max-value",      5.0,
                            "value",          0.0,
                            "valign",         GTK_ALIGN_CENTER,
                            "hexpand",        TRUE,
                            "height-request", 3,
                            "width-request",  64,
                            NULL);
      gtk_box_append (GTK_BOX (box), level);

      if (g_variant_lookup (signal_state, "icon-name", "&s", &signal_icon))
        gtk_image_set_from_icon_name (GTK_IMAGE (icon), signal_icon);

      if (g_variant_lookup (signal_state, "network-type", "&s", &network_type))
        gtk_widget_set_tooltip_text (GTK_WIDGET (icon), network_type);

      if (g_variant_lookup (signal_state, "signal-strength", "x", &signal_strength))
        gtk_level_bar_set_value (GTK_LEVEL_BAR (level), signal_strength);

      gtk_box_append (GTK_BOX (self->connectivity_status_box), box);
    }

  /* Add status properties
   */
  if (g_action_group_get_action_enabled (action_group, action_name))
    {
      gtk_widget_set_visible (self->connectivity_status, TRUE);

      if (g_variant_lookup (value, "icon-name", "&s", &icon_name))
        gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (self->connectivity_status), icon_name);

      if (g_variant_lookup (value, "title", "&s", &title))
        gtk_accessible_update_property (GTK_ACCESSIBLE (self->connectivity_status),
                                        GTK_ACCESSIBLE_PROPERTY_LABEL, title,
                                        -1);
    }
}

static void
on_connectivity_enabled_changed (GActionGroup     *action_group,
                                 const char       *action_name,
                                 gboolean          enabled,
                                 ValentDevicePage *self)
{
  g_autoptr (GVariant) state = NULL;

  /* gtk_widget_set_visible (self->button, enabled); */

  if (enabled)
    state = g_action_group_get_action_state (action_group, action_name);

  if (state != NULL)
    on_connectivity_state_changed (action_group, action_name, state, self);
}

/*
 * GAction
 */
static void
page_preferences_action (GtkWidget  *widget,
                         const char *action_name,
                         GVariant   *parameter)
{
  ValentDevicePage *self = VALENT_DEVICE_PAGE (widget);
  GtkRoot *window = gtk_widget_get_root (widget);

  if (self->preferences == NULL)
    {


      self->preferences = g_object_new (VALENT_TYPE_DEVICE_PREFERENCES_DIALOG,
                                        "device",         self->device,
                                        NULL);

      g_object_add_weak_pointer (G_OBJECT (self->preferences),
                                 (gpointer)&self->preferences);
    }

  adw_dialog_present (ADW_DIALOG (self->preferences), GTK_WIDGET (window));
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
  GActionGroup *action_group = G_ACTION_GROUP (self->device);
  GMenuModel *menu;
  gboolean enabled;

  G_OBJECT_CLASS (valent_device_page_parent_class)->constructed (object);

  g_object_bind_property (self->device, "id",
                          self,         "tag",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property (self->device, "name",
                          self,         "title",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  /* Actions & Menu
   */
  gtk_widget_insert_action_group (GTK_WIDGET (self), "device", action_group);
  menu = valent_device_get_menu (self->device);
  valent_menu_stack_set_menu_model (self->menu_actions, menu);

  /* Pair Section
   */
  g_signal_connect_object (self->device,
                           "notify::state",
                           G_CALLBACK (on_state_changed),
                           self,
                           G_CONNECT_DEFAULT);
  on_state_changed (self->device, NULL, self);

  /* Battery Status
   */
  g_signal_connect_object (action_group,
                           "action-state-changed::battery.state",
                           G_CALLBACK (on_battery_state_changed),
                           self,
                           G_CONNECT_DEFAULT);
  g_signal_connect_object (action_group,
                           "action-enabled-changed::battery.state",
                           G_CALLBACK (on_battery_enabled_changed),
                           self,
                           G_CONNECT_DEFAULT);

  enabled = g_action_group_get_action_enabled (action_group, "battery.state");
  on_battery_enabled_changed (action_group, "battery.state", enabled, self);

  /* Connectivity Status
   */
  g_signal_connect_object (action_group,
                           "action-state-changed::connectivity_report.state",
                           G_CALLBACK (on_connectivity_state_changed),
                           self,
                           G_CONNECT_DEFAULT);
  g_signal_connect_object (action_group,
                           "action-enabled-changed::connectivity_report.state",
                           G_CALLBACK (on_connectivity_enabled_changed),
                           self,
                           G_CONNECT_DEFAULT);

  enabled = g_action_group_get_action_enabled (action_group, "connectivity_report.state");
  on_connectivity_enabled_changed (action_group, "connectivity_report.state", enabled, self);
}

static void
valent_device_page_dispose (GObject *object)
{
  ValentDevicePage *self = VALENT_DEVICE_PAGE (object);

  g_clear_object (&self->device);
  g_clear_pointer (&self->preferences, adw_dialog_force_close);

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

  switch ((ValentDevicePageProperty)prop_id)
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

  switch ((ValentDevicePageProperty)prop_id)
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
  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-device-page.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePage, gadgets);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePage, battery_status);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePage, battery_status_label);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePage, battery_status_level);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePage, connectivity_status);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePage, connectivity_status_box);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePage, stack);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePage, pair_page);
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

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

  /* Ensure the private types we need are ready */
  g_type_ensure (VALENT_TYPE_MENU_LIST);
  g_type_ensure (VALENT_TYPE_MENU_STACK);
}

static void
valent_device_page_init (ValentDevicePage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

