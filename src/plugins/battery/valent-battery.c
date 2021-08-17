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

  unsigned int  charging : 1;
  int           level;
  unsigned int  threshold;
};


G_DEFINE_TYPE (ValentBattery, valent_battery, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CHARGING,
  PROP_LEVEL,
  PROP_THRESHOLD,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static ValentBattery *default_battery = NULL;


/*
 * GDBusProxy
 */
static void
on_properties_changed (GDBusProxy    *proxy,
                       GVariant      *changed_properties,
                       GStrv          invalidated_properties,
                       ValentBattery *self)
{
  double level;
  guint32 state;
  guint32 warning;

  g_assert (VALENT_IS_BATTERY (self));

  if (g_variant_lookup (changed_properties, "Percentage", "d", &level))
    {
      self->level = floor (level);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LEVEL]);
    }

  if (g_variant_lookup (changed_properties, "State", "u", &state))
    {
      self->charging = state == 1;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CHARGING]);
    }

  if (g_variant_lookup (changed_properties, "WarningLevel", "u", &warning))
    {
      self->threshold = warning >= 3;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_THRESHOLD]);
    }

  g_signal_emit (G_OBJECT (self), signals [CHANGED], 0);
}

static void
valent_battery_init_properties (ValentBattery *self)
{
  GDBusProxy *proxy = self->proxy;
  GVariant *value;

  if ((value = g_dbus_proxy_get_cached_property (proxy, "Type")) != NULL)
    {
      guint32 type = g_variant_get_uint32 (value);

      g_variant_unref (value);

      if (type != 2)
        return;
    }

  if ((value = g_dbus_proxy_get_cached_property (proxy, "Percentage")) != NULL)
    {
      self->level = floor (g_variant_get_double (value));
      g_variant_unref (value);
    }

  if ((value = g_dbus_proxy_get_cached_property (proxy, "State")) != NULL)
    {
      self->charging = g_variant_get_uint32 (value) == 1;
      g_variant_unref (value);
    }

  if ((value = g_dbus_proxy_get_cached_property (proxy, "WarningLevel")) != NULL)
    {
      self->threshold = g_variant_get_uint32 (value) >= 3;
      g_variant_unref (value);
    }

  g_signal_connect (self->proxy,
                    "g-properties-changed",
                    G_CALLBACK (on_properties_changed),
                    self);

  g_signal_emit (G_OBJECT (self), signals [CHANGED], 0);
}

static void
new_for_bus_cb (GObject       *object,
                GAsyncResult  *result,
                ValentBattery *self)
{
  g_autoptr (GError) error = NULL;

  self->proxy = g_dbus_proxy_new_for_bus_finish (result, &error);

  if (G_IS_DBUS_PROXY (self->proxy))
    valent_battery_init_properties (self);
  else if (error != NULL)
    g_warning ("%s: %s", G_OBJECT_TYPE_NAME (self), error->message);
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
valent_battery_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentBattery *self = VALENT_BATTERY (object);

  switch (prop_id)
    {
    case PROP_CHARGING:
      g_value_set_boolean (value, self->charging);
      break;

    case PROP_LEVEL:
      g_value_set_int (value, self->level);
      break;

    case PROP_THRESHOLD:
      g_value_set_uint (value, self->threshold);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_battery_class_init (ValentBatteryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = valent_battery_dispose;
  object_class->finalize = valent_battery_finalize;
  object_class->get_property = valent_battery_get_property;


  /**
   * ValentBattery:charging:
   *
   * Whether the battery is charging.
   */
  properties [PROP_CHARGING] =
    g_param_spec_boolean ("charging",
                          "Charging",
                          "Whether the battery is charging",
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentBattery:level:
   *
   * The current charge level.
   */
  properties [PROP_LEVEL] =
    g_param_spec_int ("level",
                      "Level",
                      "Power Level",
                      -1, 100,
                      -1,
                      (G_PARAM_READABLE |
                       G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS));

  /**
   * ValentBattery:threshold:
   *
   * Whether the battery is below the level considered low.
   */
  properties [PROP_THRESHOLD] =
    g_param_spec_uint ("threshold",
                       "Threshold",
                       "Whether the battery is below the level considered low",
                       0, 1,
                       0,
                       (G_PARAM_READABLE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);


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
  self->charging = FALSE;
  self->level = -1;
  self->threshold = 0;

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.UPower",
                            "/org/freedesktop/UPower/devices/DisplayDevice",
                            "org.freedesktop.UPower.Device",
                            self->cancellable,
                            (GAsyncReadyCallback)new_for_bus_cb,
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
 * valent_battery_get_charging:
 * @battery: a #ValentBattery
 *
 * Get whether the battery is charging.
 *
 * Returns: %TRUE if the battery is charging
 */
gboolean
valent_battery_get_charging (ValentBattery *battery)
{
  g_return_val_if_fail (VALENT_IS_BATTERY (battery), -1);

  return battery->charging;
}

/**
 * valent_battery_get_level:
 * @battery: a #ValentBattery
 *
 * Get the charge level of @battery.
 *
 * Returns: a charge level
 */
int
valent_battery_get_level (ValentBattery *battery)
{
  g_return_val_if_fail (VALENT_IS_BATTERY (battery), -1);

  return battery->level;
}

/**
 * valent_battery_get_threshold:
 * @battery: a #ValentBattery
 *
 * Get whether the battery is below the level considered low for @battery.
 *
 * Returns: `1` if below the threshold, or `0` otherwise
 */
unsigned int
valent_battery_get_threshold (ValentBattery *battery)
{
  g_return_val_if_fail (VALENT_IS_BATTERY (battery), 0);

  return battery->threshold;
}

