// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-battery-gadget"

#include "config.h"

#include <math.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-device.h>
#include <libvalent-ui.h>

#include "valent-battery-gadget.h"


struct _ValentBatteryGadget
{
  ValentDeviceGadget  parent_instance;

  GtkWidget          *button;
  GtkWidget          *level_bar;
  GtkWidget          *label;
};

G_DEFINE_FINAL_TYPE (ValentBatteryGadget, valent_battery_gadget, VALENT_TYPE_DEVICE_GADGET)


static void
on_action_state_changed (GActionGroup        *action_group,
                         const char          *action_name,
                         GVariant            *value,
                         ValentBatteryGadget *self)
{
  g_autofree char *label = NULL;
  gboolean charging = FALSE;
  gboolean is_present = FALSE;
  double percentage = 0.0;
  const char *icon_name;

  g_assert (VALENT_IS_BATTERY_GADGET (self));

  g_variant_lookup (value, "is-present", "b", &is_present);

  if (!is_present)
    {
      gtk_widget_set_visible (self->button, FALSE);
      return;
    }

  if (!g_variant_lookup (value, "percentage", "d", &percentage) ||
      !g_variant_lookup (value, "charging", "b", &charging))
    {
      gtk_widget_set_visible (self->button, FALSE);
      return;
    }

  if (!g_variant_lookup (value, "icon-name", "&s", &icon_name))
    icon_name = "battery-missing-symbolic";

  if (percentage == 100)
    {
      /* TRANSLATORS: When the battery level is 100% */
      label = g_strdup (_("Fully Charged"));
    }
  else
    {
      gint64 total_seconds = 0;
      gint64 total_minutes;
      int minutes;
      int hours;

      if (charging)
        g_variant_lookup (value, "time-to-full", "x", &total_seconds);
      else
        g_variant_lookup (value, "time-to-empty", "x", &total_seconds);

      if (total_seconds > 0)
        {
          total_minutes = floor (total_seconds / 60);
          minutes = total_minutes % 60;
          hours = floor (total_minutes / 60);
        }

      if (total_seconds <= 0)
        {
          /* TRANSLATORS: This is <percentage> (Estimating…) */
          label = g_strdup_printf (_("%g%% (Estimating…)"), percentage);
        }
      else if (charging)
        {
          /* TRANSLATORS: This is <percentage> (<hours>:<minutes> Until Full) */
          label = g_strdup_printf (_("%g%% (%d∶%02d Until Full)"),
                                   percentage, hours, minutes);
        }
      else
        {
          /* TRANSLATORS: This is <percentage> (<hours>:<minutes> Remaining) */
          label = g_strdup_printf (_("%g%% (%d∶%02d Remaining)"),
                                   percentage, hours, minutes);
        }
    }

  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (self->button), icon_name);

  gtk_level_bar_set_value (GTK_LEVEL_BAR (self->level_bar), percentage);
  gtk_label_set_text (GTK_LABEL (self->label), label);

  if (g_action_group_get_action_enabled (action_group, action_name))
    gtk_widget_set_visible (self->button, TRUE);
}

static void
on_action_enabled_changed (GActionGroup        *action_group,
                           const char          *action_name,
                           gboolean             enabled,
                           ValentBatteryGadget *self)
{
  g_autoptr (GVariant) state = NULL;

  gtk_widget_set_visible (self->button, enabled);

  if (enabled)
    state = g_action_group_get_action_state (action_group, action_name);

  if (state != NULL)
    on_action_state_changed (action_group, action_name, state, self);
}

/*
 * GObject
 */
static void
valent_battery_gadget_constructed (GObject *object)
{
  ValentBatteryGadget *self = VALENT_BATTERY_GADGET (object);
  GActionGroup *action_group = NULL;
  gboolean enabled = FALSE;

  g_object_get (object, "device", &action_group, NULL);
  g_signal_connect_object (action_group,
                           "action-state-changed::battery.state",
                           G_CALLBACK (on_action_state_changed),
                           self, 0);
  g_signal_connect_object (action_group,
                           "action-enabled-changed::battery.state",
                           G_CALLBACK (on_action_enabled_changed),
                           self, 0);

  enabled = g_action_group_get_action_enabled (action_group, "battery.state");
  on_action_enabled_changed (action_group, "battery.state", enabled, self);
  g_clear_object (&action_group);

  G_OBJECT_CLASS (valent_battery_gadget_parent_class)->constructed (object);
}

static void
valent_battery_gadget_dispose (GObject *object)
{
  ValentBatteryGadget *self = VALENT_BATTERY_GADGET (object);

  g_clear_pointer (&self->button, gtk_widget_unparent);

  G_OBJECT_CLASS (valent_battery_gadget_parent_class)->dispose (object);
}

static void
valent_battery_gadget_class_init (ValentBatteryGadgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_battery_gadget_constructed;
  object_class->dispose = valent_battery_gadget_dispose;
}

static void
valent_battery_gadget_init (ValentBatteryGadget *self)
{
  GtkWidget *popover;
  GtkWidget *box;

  /* Popover */
  popover = g_object_new (GTK_TYPE_POPOVER,
                          "autohide", TRUE,
                          NULL);

  box = g_object_new (GTK_TYPE_BOX,
                      "margin-top",    6,
                      "margin-bottom", 6,
                      "margin-start",  6,
                      "margin-end",    6,
                      "orientation",   GTK_ORIENTATION_VERTICAL,
                      "spacing",       6,
                      NULL);
  gtk_popover_set_child (GTK_POPOVER (popover), box);

  self->label = gtk_label_new (NULL);
  gtk_box_append (GTK_BOX (box), self->label);

  self->level_bar = g_object_new (GTK_TYPE_LEVEL_BAR,
                                  "min-value",     0.0,
                                  "max-value",     100.0,
                                  "value",         42.0,
                                  "width-request", 100,
                                  "height-request", 3,
                                  NULL);
  gtk_box_append (GTK_BOX (box), self->level_bar);

  /* Button */
  self->button = g_object_new (GTK_TYPE_MENU_BUTTON,
                               "icon-name", "battery-missing-symbolic",
                               "popover",   popover,
                               "has-frame", FALSE,
                               NULL);
  gtk_widget_set_parent (self->button, GTK_WIDGET (self));
}

