// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-row"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <valent.h>

#include "valent-device-row.h"
#include "valent-ui-utils-private.h"


struct _ValentDeviceRow
{
  GtkListBoxRow   parent_instance;

  ValentDevice   *device;
  AdwAnimation   *animation;
  unsigned int    selected : 1;
  unsigned int    selection_mode : 1;

  /* template */
  GtkRevealer    *revealer;
  GtkCheckButton *selected_button;
  GtkWidget      *device_icon;
  GtkLabel       *device_name;
  GtkLabel       *device_status;
  GtkStack       *status_stack;
  GtkWidget      *battery_status;
  GtkWidget      *connectivity_status;
  GtkImage       *suffixes;
};

G_DEFINE_FINAL_TYPE (ValentDeviceRow, valent_device_row, GTK_TYPE_LIST_BOX_ROW)

typedef enum {
  PROP_DEVICE = 1,
  PROP_SELECTED,
  PROP_SELECTION_MODE,
} ValentDeviceRowProperty;

static GParamSpec *properties[PROP_SELECTION_MODE + 1] = { NULL, };

static void
on_selection_enable (ValentDeviceRow *self)
{
  GtkRoot *root = NULL;
  GParamSpec *pspec = NULL;

  if (self->selection_mode)
    return;

  // FIXME
  root = gtk_widget_get_root (GTK_WIDGET (self));
  if (root != NULL)
    {
      pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (root),
                                            "selection-mode");
    }

  if (pspec != NULL)
    {
      g_object_set (root, "selection-mode", TRUE, NULL);
      valent_device_row_set_selected (self, TRUE);
    }
}

static void
on_selection_disable (ValentDeviceRow *self)
{
  if (!gtk_revealer_get_child_revealed (self->revealer))
    valent_device_row_set_selected (self, FALSE);
}

/*
 * Battery
 */
static void
on_battery_state_changed (GActionGroup    *action_group,
                          const char      *action_name,
                          GVariant        *value,
                          ValentDeviceRow *self)
{
  g_autofree char *label = NULL;
  gboolean charging = FALSE;
  gboolean is_present = FALSE;
  double percentage = 0.0;
  const char *icon_name;

  g_assert (VALENT_IS_DEVICE_ROW (self));

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
      int64_t total_minutes;
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

  gtk_image_set_from_icon_name (GTK_IMAGE (self->battery_status), icon_name);
  gtk_widget_set_tooltip_text (self->battery_status, label);
  gtk_widget_set_visible (self->battery_status, TRUE);
}

static void
on_battery_enabled_changed (GActionGroup    *action_group,
                            const char      *action_name,
                            gboolean         enabled,
                            ValentDeviceRow *self)
{
  g_autoptr (GVariant) state = NULL;

  if (enabled)
    state = g_action_group_get_action_state (action_group, action_name);

  if (state != NULL)
    on_battery_state_changed (action_group, action_name, state, self);

  gtk_widget_set_visible (self->battery_status, enabled);
}

/*
 * Connectivity Status
 */
static void
on_connectivity_state_changed (GActionGroup    *action_group,
                               const char      *action_name,
                               GVariant        *value,
                               ValentDeviceRow *self)
{
  const char *icon_name;
  const char *title;

  g_assert (VALENT_IS_DEVICE_ROW (self));

  if (g_variant_lookup (value, "icon-name", "&s", &icon_name))
    gtk_image_set_from_icon_name (GTK_IMAGE (self->connectivity_status), icon_name);

  if (g_variant_lookup (value, "title", "&s", &title))
    gtk_widget_set_tooltip_text (self->battery_status, title);

  gtk_widget_set_visible (self->connectivity_status, TRUE);
}

static void
on_connectivity_enabled_changed (GActionGroup    *action_group,
                                 const char      *action_name,
                                 gboolean         enabled,
                                 ValentDeviceRow *self)
{
  g_autoptr (GVariant) state = NULL;

  if (enabled)
    state = g_action_group_get_action_state (action_group, action_name);

  if (state != NULL)
    on_connectivity_state_changed (action_group, action_name, state, self);

  gtk_widget_set_visible (self->connectivity_status, enabled);
}

static void
valent_device_row_sync (ValentDeviceRow *self)
{
  GActionGroup *action_group = G_ACTION_GROUP (self->device);
  ValentDeviceState state;
  gboolean enabled;

  g_assert (VALENT_IS_DEVICE_ROW (self));
  g_assert (VALENT_IS_DEVICE (self->device));

  state = valent_device_get_state (self->device);
  if ((state & VALENT_DEVICE_STATE_PAIRED) == 0)
    {
      gtk_label_set_label (self->device_status, _("Unpaired"));
      gtk_widget_remove_css_class (GTK_WIDGET (self->device_status), "dim-label");
    }
  else if ((state & VALENT_DEVICE_STATE_CONNECTED) == 0)
    {
      gtk_label_set_label (self->device_status, _("Disconnected"));
      gtk_widget_add_css_class (GTK_WIDGET (self->device_status), "dim-label");
    }
  else
    {
      gtk_label_set_label (self->device_status, _("Connected"));
      gtk_widget_remove_css_class (GTK_WIDGET (self->device_status), "dim-label");
    }

  enabled = g_action_group_get_action_enabled (action_group, "battery.state");
  on_battery_enabled_changed (action_group, "battery.state", enabled, self);

  enabled = g_action_group_get_action_enabled (action_group, "connectivity_report.state");
  on_connectivity_enabled_changed (action_group, "connectivity_report.state", enabled, self);
}

/*
 * GObject
 */
static void
valent_device_row_constructed (GObject *object)
{
  ValentDeviceRow *self = VALENT_DEVICE_ROW (object);

  G_OBJECT_CLASS (valent_device_row_parent_class)->constructed (object);

  if (self->device != NULL)
    {
      g_object_bind_property (self->device,      "name",
                              self->device_name, "label",
                              G_BINDING_SYNC_CREATE);
      g_object_bind_property (self->device,      "icon-name",
                              self->device_icon, "icon-name",
                              G_BINDING_SYNC_CREATE);
      g_signal_connect_object (self->device,
                               "notify::state",
                               G_CALLBACK (valent_device_row_sync),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (self->device,
                               "action-state-changed::battery.state",
                               G_CALLBACK (on_battery_state_changed),
                               self,
                               G_CONNECT_DEFAULT);
      g_signal_connect_object (self->device,
                               "action-enabled-changed::battery.state",
                               G_CALLBACK (on_battery_enabled_changed),
                               self,
                               G_CONNECT_DEFAULT);
      g_signal_connect_object (self->device,
                               "action-state-changed::connectivity_report.state",
                               G_CALLBACK (on_connectivity_state_changed),
                               self, 0);
      g_signal_connect_object (self->device,
                               "action-enabled-changed::connectivity_report.state",
                               G_CALLBACK (on_connectivity_enabled_changed),
                               self,
                               G_CONNECT_DEFAULT);
    }

  valent_device_row_sync (self);
}

static void
valent_device_row_dispose (GObject *object)
{
  ValentDeviceRow *self = VALENT_DEVICE_ROW (object);

  if (self->animation != NULL)
    {
      adw_animation_skip (self->animation);
      g_clear_object (&self->animation);
    }

  g_clear_object (&self->device);

  G_OBJECT_CLASS (valent_device_row_parent_class)->dispose (object);
}

static void
valent_device_row_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ValentDeviceRow *self = VALENT_DEVICE_ROW (object);

  switch ((ValentDeviceRowProperty)prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    case PROP_SELECTED:
      g_value_set_boolean (value, self->selected);
      break;

    case PROP_SELECTION_MODE:
      g_value_set_boolean (value, self->selection_mode);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_row_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ValentDeviceRow *self = VALENT_DEVICE_ROW (object);

  switch ((ValentDeviceRowProperty)prop_id)
    {
    case PROP_DEVICE:
      g_assert (self->device == NULL);
      self->device = g_value_dup_object (value);
      break;

    case PROP_SELECTED:
      valent_device_row_set_selected (self, g_value_get_boolean (value));
      break;

    case PROP_SELECTION_MODE:
      valent_device_row_set_selection_mode (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_row_class_init (ValentDeviceRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_device_row_constructed;
  object_class->dispose = valent_device_row_dispose;
  object_class->get_property = valent_device_row_get_property;
  object_class->set_property = valent_device_row_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-device-row.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentDeviceRow, revealer);
  gtk_widget_class_bind_template_child (widget_class, ValentDeviceRow, selected_button);
  gtk_widget_class_bind_template_child (widget_class, ValentDeviceRow, device_icon);
  gtk_widget_class_bind_template_child (widget_class, ValentDeviceRow, device_name);
  gtk_widget_class_bind_template_child (widget_class, ValentDeviceRow, device_status);
  gtk_widget_class_bind_template_child (widget_class, ValentDeviceRow, status_stack);
  gtk_widget_class_bind_template_child (widget_class, ValentDeviceRow, battery_status);
  gtk_widget_class_bind_template_child (widget_class, ValentDeviceRow, connectivity_status);
  gtk_widget_class_bind_template_child (widget_class, ValentDeviceRow, suffixes);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_enable);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_disable);

  /**
   * ValentDeviceRow:device
   *
   * The `ValentDevice` for this row.
   */
  properties [PROP_DEVICE] =
    g_param_spec_object ("device", NULL, NULL,
                         VALENT_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentDeviceRow:selected: (getter get_selected) (setter set_selected)
   *
   * Whether the row is selected.
   */
  properties [PROP_SELECTED] =
    g_param_spec_boolean ("selected", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentDeviceRow:selection-mode: (getter get_selection_mode) (setter set_selection_mode)
   *
   * Whether the row is in selection mode.
   */
  properties [PROP_SELECTION_MODE] =
    g_param_spec_boolean ("selection-mode", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_device_row_init (ValentDeviceRow *self)
{
  AdwAnimationTarget *target = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  target = adw_property_animation_target_new (G_OBJECT (self->suffixes),
                                              "opacity");
  self->animation = adw_timed_animation_new (GTK_WIDGET (self),
                                             0.0, 1.0, 250,
                                             target);
}

/**
 * valent_device_row_get_device:
 * @row: a `ValentDeviceRow`
 *
 * Get the `ValentDevice` for @row.
 *
 * Returns: (transfer none): a `ValentDevice`
 */
ValentDevice *
valent_device_row_get_device (ValentDeviceRow *row)
{
  g_return_val_if_fail (VALENT_IS_DEVICE_ROW (row), NULL);

  return row->device;
}

/**
 * valent_device_row_get_selected: (get-property selected)
 * @row: a `ValentDeviceRow`
 *
 * Get whether the row is selected.
 *
 * Returns: %TRUE if the row is selected, or %FALSE if not
 */
gboolean
valent_device_row_get_selected (ValentDeviceRow *row)
{
  g_return_val_if_fail (VALENT_IS_DEVICE_ROW (row), FALSE);

  return row->selected;
}

/**
 * valent_device_row_set_selected: (set-property selected)
 * @row: a `ValentDeviceRow`
 * @selected: whether to select the row
 *
 * Set whether the row is selected.
 */
void
valent_device_row_set_selected (ValentDeviceRow *row,
                                      gboolean              selected)
{
  g_return_if_fail (VALENT_IS_DEVICE_ROW (row));

  selected = !!selected;
  if (row->selected == selected)
    return;

  gtk_accessible_update_state (GTK_ACCESSIBLE (row),
                               GTK_ACCESSIBLE_STATE_SELECTED, selected,
                               -1);

  row->selected = selected;
  g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_SELECTED]);
}

/**
 * valent_device_row_get_selection_mode:
 * @row: a `ValentDeviceRow`
 *
 * Get whether selection mode is enabled.
 *
 * Returns: %TRUE if selection mode is enabled, or %FALSE if not
 */
gboolean
valent_device_row_get_selection_mode (ValentDeviceRow *row)
{
  g_return_val_if_fail (VALENT_IS_DEVICE_ROW (row), FALSE);

  return row->selection_mode;
}

/**
 * valent_device_row_set_selection_mode: (set-property selection-mode)
 * @row: a `ValentDeviceRow`
 * @selection_mode: whether to select the row
 *
 * Set whether selection mode is enabled.
 */
void
valent_device_row_set_selection_mode (ValentDeviceRow *row,
                                      gboolean         selection_mode)
{
  g_return_if_fail (VALENT_IS_DEVICE_ROW (row));

  selection_mode = !!selection_mode;
  if (row->selection_mode == selection_mode)
    return;

  if (selection_mode)
    {
      gtk_accessible_update_state (GTK_ACCESSIBLE (row),
                                   GTK_ACCESSIBLE_STATE_SELECTED, FALSE,
                                   -1);
    }
  else
    {
      gtk_accessible_reset_state (GTK_ACCESSIBLE (row),
                                  GTK_ACCESSIBLE_STATE_SELECTED);
    }

  adw_animation_skip (row->animation);
  adw_timed_animation_set_reverse (ADW_TIMED_ANIMATION (row->animation),
                                   selection_mode);
  adw_animation_play (row->animation);

  row->selection_mode = selection_mode;
  g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_SELECTION_MODE]);
}

