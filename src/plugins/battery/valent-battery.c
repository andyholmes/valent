// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-battery"

#include "config.h"

#include <gio/gio.h>
#include <math.h>

#include "valent-battery.h"


struct _ValentBattery
{
  GObject       parent_instance;

  GDBusProxy   *proxy;
  GCancellable *cancellable;

  int           current_charge;
  gboolean      is_charging;
  unsigned int  threshold_event;
};

G_DEFINE_TYPE (ValentBattery, valent_battery, G_TYPE_OBJECT)

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static ValentBattery *default_battery = NULL;


/*
 * These are a convenient representation of the values returned by UPower D-Bus
 * service, that would otherwise be opaque integers.
 *
 * See: https://upower.freedesktop.org/docs/Device.html
 */
enum {
  UPOWER_KIND_UNKNOWN,
  UPOWER_KIND_LINE_POWER,
  UPOWER_KIND_BATTERY,
  UPOWER_KIND_UPS,
  UPOWER_KIND_MONITOR,
  UPOWER_KIND_MOUSE,
  UPOWER_KIND_KEYBOARD,
  UPOWER_KIND_PDA,
  UPOWER_KIND_PHONE,
};

enum {
  UPOWER_LEVEL_UNKNOWN,
  UPOWER_LEVEL_NONE,
  UPOWER_LEVEL_DISCHARGING,
  UPOWER_LEVEL_LOW,
  UPOWER_LEVEL_CRITICAL,
  UPOWER_LEVEL_ACTION,
  UPOWER_LEVEL_NORMAL,
  UPOWER_LEVEL_HIGH,
  UPOWER_LEVEL_FULL
};

enum {
  UPOWER_STATE_UNKNOWN,
  UPOWER_STATE_CHARGING,
  UPOWER_STATE_DISCHARGING,
  UPOWER_STATE_EMPTY,
  UPOWER_STATE_FULLY_CHARGED,
  UPOWER_STATE_PENDING_CHARGE,
  UPOWER_STATE_PENDING_DISCHARGE
};


/*
 * These are convenience functions for translating UPower states and levels into
 * values expected by KDE Connect.
 */
static inline gboolean
translate_state (guint32 state)
{
  switch (state)
    {
    case UPOWER_STATE_CHARGING:
    case UPOWER_STATE_FULLY_CHARGED:
    case UPOWER_STATE_PENDING_CHARGE:
      return TRUE;

    case UPOWER_STATE_DISCHARGING:
    case UPOWER_STATE_EMPTY:
    case UPOWER_STATE_PENDING_DISCHARGE:
      return FALSE;

    default:
      return FALSE;
    }
}

static inline unsigned int
translate_warning_level (guint32 warning_level)
{
  switch (warning_level)
    {
    case UPOWER_LEVEL_NONE:
      return 0;

    case UPOWER_LEVEL_LOW:
    case UPOWER_LEVEL_CRITICAL:
    case UPOWER_LEVEL_ACTION:
      return 1;

    default:
      return 0;
    }
}

/*
 * GDBusProxy
 */
static void
valent_battery_load_properties (ValentBattery *self)
{
  g_autoptr (GVariant) value = NULL;

  g_assert (VALENT_IS_BATTERY (self));

  if ((value = g_dbus_proxy_get_cached_property (self->proxy, "Percentage")) != NULL)
    {
      double percentage = g_variant_get_double (value);

      self->current_charge = floor (percentage);
      g_clear_pointer (&value, g_variant_unref);
    }

  if ((value = g_dbus_proxy_get_cached_property (self->proxy, "State")) != NULL)
    {
      guint32 state = g_variant_get_uint32 (value);

      self->is_charging = translate_state (state);
      g_clear_pointer (&value, g_variant_unref);
    }

  if ((value = g_dbus_proxy_get_cached_property (self->proxy, "WarningLevel")) != NULL)
    {
      guint32 warning_level = g_variant_get_uint32 (value);

      self->threshold_event = translate_warning_level (warning_level);
      g_clear_pointer (&value, g_variant_unref);
    }
}

static void
on_properties_changed (GDBusProxy    *proxy,
                       GVariant      *changed_properties,
                       GStrv          invalidated_properties,
                       ValentBattery *self)
{
  gboolean changed = FALSE;
  gboolean is_present;
  double percentage;
  guint32 state;
  guint32 warning_level;

  g_assert (VALENT_IS_BATTERY (self));

  /* If the battery was inserted or removed, the properties need to be either
   * entirely reloaded or reset, respectively. */
  if (g_variant_lookup (changed_properties, "IsPresent", "b", &is_present))
    {
      /* An existing battery was physically inserted */
      if (is_present && self->current_charge < 0)
        {
          valent_battery_load_properties (self);
          changed = TRUE;
        }

      /* An existing battery was physically removed */
      else if (!is_present && self->current_charge >= 0)
        {
          self->current_charge = -1;
          self->is_charging = FALSE;
          self->threshold_event = 0;
          changed = TRUE;
        }

      if (changed)
        {
          g_signal_emit (G_OBJECT (self), signals [CHANGED], 0);
          return;
        }
    }

  if (g_variant_lookup (changed_properties, "Percentage", "d", &percentage))
    {
      int current_charge = floor (percentage);

      if (self->current_charge != current_charge)
        {
          self->current_charge = current_charge;
          changed = TRUE;
        }
    }

  if (g_variant_lookup (changed_properties, "State", "u", &state))
    {
      gboolean is_charging = translate_state (state);

      if (self->is_charging != is_charging)
        {
          self->is_charging = is_charging;
          changed = TRUE;
        }
    }

  if (g_variant_lookup (changed_properties, "WarningLevel", "u", &warning_level))
    {
      unsigned int threshold_event = translate_warning_level (warning_level);

      if (self->threshold_event != threshold_event)
        {
          self->threshold_event = threshold_event;
          changed = TRUE;
        }
    }

  if (changed)
    g_signal_emit (G_OBJECT (self), signals [CHANGED], 0);
}

static void
g_dbus_proxy_new_for_bus_cb (GObject       *object,
                             GAsyncResult  *result,
                             ValentBattery *self)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GVariant) type = NULL;
  g_autoptr (GVariant) value = NULL;

  if ((self->proxy = g_dbus_proxy_new_for_bus_finish (result, &error)) == NULL)
    {
      g_warning ("%s: %s", G_OBJECT_TYPE_NAME (self), error->message);
      return;
    }

  if ((type = g_dbus_proxy_get_cached_property (self->proxy, "Type")) == NULL ||
      g_variant_get_uint32 (type) != UPOWER_KIND_BATTERY)
    {
      g_debug ("%s: not a battery", G_OBJECT_TYPE_NAME (self));
      return;
    }

  if ((value = g_dbus_proxy_get_cached_property (self->proxy, "IsPresent")) != NULL)
    {
      gboolean is_present = g_variant_get_boolean (value);

      if (is_present)
        valent_battery_load_properties (self);
    }

  g_signal_connect (self->proxy,
                    "g-properties-changed",
                    G_CALLBACK (on_properties_changed),
                    self);
  g_signal_emit (G_OBJECT (self), signals [CHANGED], 0);
}

/*
 * GObject
 */
static void
valent_battery_dispose (GObject *object)
{
  ValentBattery *self = VALENT_BATTERY (object);

  g_cancellable_cancel (self->cancellable);

  G_OBJECT_CLASS (valent_battery_parent_class)->dispose (object);
}

static void
valent_battery_finalize (GObject *object)
{
  ValentBattery *self = VALENT_BATTERY (object);

  g_clear_object (&self->cancellable);
  g_clear_object (&self->proxy);

  G_OBJECT_CLASS (valent_battery_parent_class)->finalize (object);
}

static void
valent_battery_class_init (ValentBatteryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = valent_battery_dispose;
  object_class->finalize = valent_battery_finalize;

  /**
   * ValentBattery::changed:
   * @self: a #ValentBattery
   *
   * #ValentBattery::changed is emitted whenever a relevant property changes.
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
valent_battery_init (ValentBattery *self)
{
  self->cancellable = g_cancellable_new ();
  self->current_charge = -1;
  self->is_charging = FALSE;
  self->threshold_event = 0;

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.UPower",
                            "/org/freedesktop/UPower/devices/DisplayDevice",
                            "org.freedesktop.UPower.Device",
                            self->cancellable,
                            (GAsyncReadyCallback)g_dbus_proxy_new_for_bus_cb,
                            self);
}

/**
 * valent_battery_get_default:
 *
 * Get the default #ValentBattery.
 *
 * Returns: (transfer none): The default #ValentBattery
 */
ValentBattery *
valent_battery_get_default (void)
{
  if (default_battery == NULL)
    {
      default_battery = g_object_new (VALENT_TYPE_BATTERY, NULL);

      g_object_add_weak_pointer (G_OBJECT (default_battery),
                                 (gpointer) &default_battery);
    }

  return default_battery;
}

/**
 * valent_battery_current_charge:
 * @battery: a #ValentBattery
 *
 * Get the charge level of @battery.
 *
 * The value returned by this method is a simplification of a UPower device
 * battery percentage, useful for KDE Connect clients.
 *
 * Returns: a charge percentage, or `-1` if unavailable
 */
int
valent_battery_current_charge (ValentBattery *battery)
{
  g_return_val_if_fail (VALENT_IS_BATTERY (battery), -1);

  return battery->current_charge;
}

/**
 * valent_battery_is_charging:
 * @battery: a #ValentBattery
 *
 * Get whether the battery is charging.
 *
 * The value returned by this method is a simplification of a UPower device
 * state to a value useful for KDE Connect clients.
 *
 * Returns: %TRUE if the battery is charging
 */
gboolean
valent_battery_is_charging (ValentBattery *battery)
{
  g_return_val_if_fail (VALENT_IS_BATTERY (battery), FALSE);

  return battery->is_charging;
}

/**
 * valent_battery_is_charging:
 * @battery: a #ValentBattery
 *
 * Get whether the battery is charging.
 *
 * The value returned by this method is a simplification of a UPower device
 * level to a value useful for KDE Connect clients.
 *
 * Returns: `1` if the level is below the threshold, `0` otherwise
 */
unsigned int
valent_battery_threshold_event (ValentBattery *battery)
{
  g_return_val_if_fail (VALENT_IS_BATTERY (battery), FALSE);

  return battery->threshold_event;
}

