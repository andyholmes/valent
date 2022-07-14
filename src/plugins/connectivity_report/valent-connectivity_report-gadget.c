// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-connectivity_report-gadget"

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-ui.h>

#include "valent-connectivity_report-gadget.h"


struct _ValentConnectivityReportGadget
{
  GtkWidget     parent_instance;

  ValentDevice *device;

  GHashTable   *connections;

  GtkWidget    *button;
  GtkWidget    *box;
};


/* Interfaces */
static void valent_device_gadget_iface_init (ValentDeviceGadgetInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentConnectivityReportGadget, valent_connectivity_report_gadget, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_GADGET, valent_device_gadget_iface_init))


enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};


static void
on_action_state_changed (GActionGroup        *action_group,
                         const char          *action_name,
                         GVariant            *value,
                         ValentConnectivityReportGadget *self)
{
  GtkWidget *child;
  g_autoptr (GVariant) signal_strengths = NULL;
  GVariantIter iter;
  char *signal_id = NULL;
  GVariant *signal_state;
  const char *icon_name;
  const char *title;

  g_assert (VALENT_IS_CONNECTIVITY_REPORT_GADGET (self));

  /* Clear the popup */
  while ((child = gtk_widget_get_first_child (self->box)) != NULL)
    gtk_box_remove (GTK_BOX (self->box), child);

  if (!g_variant_lookup (value, "signal-strengths", "@a{sv}", &signal_strengths))
    {
      gtk_widget_set_visible (self->button, FALSE);
      return;
    }

  /* Add each signal */
  g_variant_iter_init (&iter, signal_strengths);

  while (g_variant_iter_loop (&iter, "{sv}", &signal_id, &signal_state))
    {
      GtkWidget *box;
      GtkWidget *level;
      GtkWidget *icon;
      const char *signal_icon;
      const char *network_type;
      gint64 signal_strength;

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

      gtk_box_append (GTK_BOX (self->box), box);
    }

  /* Add status properties */
  if (g_variant_lookup (value, "icon-name", "&s", &icon_name))
    gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (self->button), icon_name);

  if (g_variant_lookup (value, "title", "&s", &title))
    gtk_widget_set_tooltip_text (GTK_WIDGET (self->button), title);

  gtk_widget_set_visible (self->button, TRUE);
}

static void
on_action_enabled_changed (GActionGroup        *action_group,
                           const char          *action_name,
                           gboolean             enabled,
                           ValentConnectivityReportGadget *self)
{
  g_autoptr (GVariant) state = NULL;

  gtk_widget_set_visible (self->button, enabled);

  if (enabled)
    state = g_action_group_get_action_state (action_group, action_name);

  if (state != NULL)
    on_action_state_changed (action_group, action_name, state, self);
}

/*
 * ValentDeviceGadget
 */
static void
valent_device_gadget_iface_init (ValentDeviceGadgetInterface *iface)
{
}

/*
 * GObject
 */
static void
valent_connectivity_report_gadget_constructed (GObject *object)
{
  ValentConnectivityReportGadget *self = VALENT_CONNECTIVITY_REPORT_GADGET (object);
  GActionGroup *action_group = G_ACTION_GROUP (self->device);
  gboolean enabled = FALSE;

  g_signal_connect (action_group,
                    "action-state-changed::connectivity_report.state",
                    G_CALLBACK (on_action_state_changed),
                    self);

  g_signal_connect (action_group,
                    "action-enabled-changed::connectivity_report.state",
                    G_CALLBACK (on_action_enabled_changed),
                    self);

  enabled = g_action_group_get_action_enabled (action_group, "connectivity_report.state");
  on_action_enabled_changed (action_group, "connectivity_report.state", enabled, self);

  G_OBJECT_CLASS (valent_connectivity_report_gadget_parent_class)->constructed (object);
}

static void
valent_connectivity_report_gadget_dispose (GObject *object)
{
  ValentConnectivityReportGadget *self = VALENT_CONNECTIVITY_REPORT_GADGET (object);

  g_signal_handlers_disconnect_by_data (self->device, self);
  g_clear_pointer (&self->button, gtk_widget_unparent);

  G_OBJECT_CLASS (valent_connectivity_report_gadget_parent_class)->dispose (object);
}

static void
valent_connectivity_report_gadget_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ValentConnectivityReportGadget *self = VALENT_CONNECTIVITY_REPORT_GADGET (object);

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
valent_connectivity_report_gadget_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ValentConnectivityReportGadget *self = VALENT_CONNECTIVITY_REPORT_GADGET (object);

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
valent_connectivity_report_gadget_class_init (ValentConnectivityReportGadgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_connectivity_report_gadget_constructed;
  object_class->dispose = valent_connectivity_report_gadget_dispose;
  object_class->get_property = valent_connectivity_report_gadget_get_property;
  object_class->set_property = valent_connectivity_report_gadget_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
valent_connectivity_report_gadget_init (ValentConnectivityReportGadget *self)
{
  GtkWidget *popover;

  /* Popover */
  self->box = g_object_new (GTK_TYPE_BOX,
                            "margin-top",    6,
                            "margin-bottom", 6,
                            "margin-start",  6,
                            "margin-end",    6,
                            "orientation",   GTK_ORIENTATION_VERTICAL,
                            "spacing",       6,
                            NULL);

  popover = g_object_new (GTK_TYPE_POPOVER,
                          "autohide", TRUE,
                          "child",    self->box,
                          NULL);

  self->button = g_object_new (GTK_TYPE_MENU_BUTTON,
                               "icon-name", "network-cellular-offline-symbolic",
                               "popover",   popover,
                               "has-frame", FALSE,
                               NULL);
  gtk_widget_set_parent (self->button, GTK_WIDGET (self));
}

