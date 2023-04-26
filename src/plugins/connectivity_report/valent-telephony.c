// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-telephony"

#include "config.h"

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-telephony.h"


/**
 * ValentTelephony:
 *
 * A class for controlling pointer and keyboard devices.
 *
 * #ValentTelephony is an abstraction of telephony support, intended for use by
 * [class@Valent.DevicePlugin] implementations.
 */

struct _ValentTelephony
{
  ValentObject        parent_instance;

  GDBusObjectManager *manager;
  GDBusProxy         *modem;
  GHashTable         *modems;
};

G_DEFINE_FINAL_TYPE (ValentTelephony, valent_telephony, VALENT_TYPE_OBJECT)

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


static ValentTelephony *default_telephony = NULL;

/*
 * ModemManager-enums.h
 */
typedef enum { /*< underscore_name=mm_modem_access_technology >*/
  MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN     = 0,
  MM_MODEM_ACCESS_TECHNOLOGY_POTS        = 1 << 0,
  MM_MODEM_ACCESS_TECHNOLOGY_GSM         = 1 << 1,
  MM_MODEM_ACCESS_TECHNOLOGY_GSM_COMPACT = 1 << 2,
  MM_MODEM_ACCESS_TECHNOLOGY_GPRS        = 1 << 3,
  MM_MODEM_ACCESS_TECHNOLOGY_EDGE        = 1 << 4,
  MM_MODEM_ACCESS_TECHNOLOGY_UMTS        = 1 << 5,
  MM_MODEM_ACCESS_TECHNOLOGY_HSDPA       = 1 << 6,
  MM_MODEM_ACCESS_TECHNOLOGY_HSUPA       = 1 << 7,
  MM_MODEM_ACCESS_TECHNOLOGY_HSPA        = 1 << 8,
  MM_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS   = 1 << 9,
  MM_MODEM_ACCESS_TECHNOLOGY_1XRTT       = 1 << 10,
  MM_MODEM_ACCESS_TECHNOLOGY_EVDO0       = 1 << 11,
  MM_MODEM_ACCESS_TECHNOLOGY_EVDOA       = 1 << 12,
  MM_MODEM_ACCESS_TECHNOLOGY_EVDOB       = 1 << 13,
  MM_MODEM_ACCESS_TECHNOLOGY_LTE         = 1 << 14,
  MM_MODEM_ACCESS_TECHNOLOGY_5GNR        = 1 << 15,
  MM_MODEM_ACCESS_TECHNOLOGY_ANY         = 0xFFFFFFFF,
} ValentModemAccessTechnology;

typedef enum { /*< underscore_name=mm_modem_state >*/
  MM_MODEM_STATE_FAILED        = -1,
  MM_MODEM_STATE_UNKNOWN       = 0,
  MM_MODEM_STATE_INITIALIZING  = 1,
  MM_MODEM_STATE_LOCKED        = 2,
  MM_MODEM_STATE_DISABLED      = 3,
  MM_MODEM_STATE_DISABLING     = 4,
  MM_MODEM_STATE_ENABLING      = 5,
  MM_MODEM_STATE_ENABLED       = 6,
  MM_MODEM_STATE_SEARCHING     = 7,
  MM_MODEM_STATE_REGISTERED    = 8,
  MM_MODEM_STATE_DISCONNECTING = 9,
  MM_MODEM_STATE_CONNECTING    = 10,
  MM_MODEM_STATE_CONNECTED     = 11,
} ValentModemState;


static inline const char *
get_telephony_type_string (unsigned int flags)
{
  switch (flags)
    {
    case MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN:
    case MM_MODEM_ACCESS_TECHNOLOGY_POTS:
      return "Unknown";

    case MM_MODEM_ACCESS_TECHNOLOGY_GSM:
    case MM_MODEM_ACCESS_TECHNOLOGY_GSM_COMPACT:
      return "GSM";

    case MM_MODEM_ACCESS_TECHNOLOGY_GPRS:
      return "GPRS";

    case MM_MODEM_ACCESS_TECHNOLOGY_EDGE:
      return "EDGE";

    case MM_MODEM_ACCESS_TECHNOLOGY_UMTS:
      return "UMTS";

    case MM_MODEM_ACCESS_TECHNOLOGY_HSDPA:
    case MM_MODEM_ACCESS_TECHNOLOGY_HSUPA:
    case MM_MODEM_ACCESS_TECHNOLOGY_HSUPA | MM_MODEM_ACCESS_TECHNOLOGY_HSDPA:
    case MM_MODEM_ACCESS_TECHNOLOGY_HSPA:
    case MM_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS:
      return "HSPA";

    case MM_MODEM_ACCESS_TECHNOLOGY_1XRTT:
    case MM_MODEM_ACCESS_TECHNOLOGY_EVDO0:
    case MM_MODEM_ACCESS_TECHNOLOGY_EVDOA:
    case MM_MODEM_ACCESS_TECHNOLOGY_EVDOB:
      return "CDMA2000";

    case MM_MODEM_ACCESS_TECHNOLOGY_LTE:
      return "LTE";

    case MM_MODEM_ACCESS_TECHNOLOGY_5GNR:
      return "5G";

    case MM_MODEM_ACCESS_TECHNOLOGY_ANY:
      return "Unknown";

    default:
      return "Unknown";
    }
}

static void
on_properties_changed (GDBusProxy      *proxy,
                       GVariant        *changed_properties,
                       GStrv            invaliated,
                       ValentTelephony *self)
{
  GVariantIter iter;
  const char *property;
  GVariant *value;
  gboolean changed = FALSE;

  g_variant_iter_init (&iter, changed_properties);

  while (g_variant_iter_loop (&iter, "{sv}", &property, &value))
    {
      if (g_str_equal (property, "AccessTechnologies"))
        changed = TRUE;
      else if (g_str_equal (property, "SignalQuality"))
        changed = TRUE;
      else if (g_str_equal (property, "State"))
        changed = TRUE;
    }

  if (changed)
    g_signal_emit (G_OBJECT (self), signals [CHANGED], 0);
}

static void
g_dbus_proxy_new_for_bus_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr (ValentTelephony) self = VALENT_TELEPHONY (user_data);
  g_autoptr (GDBusProxy) proxy = NULL;
  g_autoptr (GError) error = NULL;
  const char *object_path;

  proxy = g_dbus_proxy_new_for_bus_finish (result, &error);

  if (proxy == NULL)
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      return;
    }

  g_signal_connect (proxy,
                    "g-properties-changed",
                    G_CALLBACK (on_properties_changed),
                    self);

  object_path = g_dbus_proxy_get_object_path (proxy);
  g_hash_table_replace (self->modems,
                        g_strdup (object_path),
                        g_steal_pointer (&proxy));

  g_signal_emit (G_OBJECT (self), signals [CHANGED], 0);
}

static void
on_modem_added (GDBusObjectManager *manager,
                GDBusObject        *object,
                ValentTelephony    *self)
{
  const char *object_path;

  g_assert (G_IS_DBUS_OBJECT_MANAGER (manager));
  g_assert (G_IS_DBUS_OBJECT (object));
  g_assert (VALENT_IS_TELEPHONY (self));

  object_path = g_dbus_object_get_object_path (object);

  if (!g_str_has_prefix (object_path, "/org/freedesktop/ModemManager1/Modem"))
    return;

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                            G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                            NULL,
                            "org.freedesktop.ModemManager1",
                            object_path,
                            "org.freedesktop.ModemManager1.Modem",
                            NULL,
                            (GAsyncReadyCallback)g_dbus_proxy_new_for_bus_cb,
                            g_object_ref (self));
}

static void
on_modem_removed (GDBusObjectManager *manager,
                  GDBusObject        *object,
                  ValentTelephony    *self)
{
  const char *object_path;

  g_assert (G_IS_DBUS_OBJECT_MANAGER (manager));
  g_assert (G_IS_DBUS_OBJECT (object));
  g_assert (VALENT_IS_TELEPHONY (self));

  object_path = g_dbus_object_get_object_path (object);

  if (!g_str_has_prefix (object_path, "/org/freedesktop/ModemManager1/Modem"))
    return;

  if (g_hash_table_remove (self->modems, object_path))
    g_signal_emit (G_OBJECT (self), signals [CHANGED], 0);
}

static void
g_dbus_object_manager_client_new_for_bus_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data)
{
  g_autoptr (ValentTelephony) self = VALENT_TELEPHONY (user_data);
  g_autolist (GDBusObject) modems = NULL;
  g_autoptr (GError) error = NULL;

  self->manager = g_dbus_object_manager_client_new_for_bus_finish (result,
                                                                   &error);

  if (self->manager == NULL)
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      return;
    }

  modems = g_dbus_object_manager_get_objects (self->manager);

  for (const GList *iter = modems; iter; iter = iter->next)
    on_modem_added (self->manager, iter->data, self);

  g_signal_connect (self->manager,
                    "object-added",
                    G_CALLBACK (on_modem_added),
                    self);

  g_signal_connect (self->manager,
                    "object-removed",
                    G_CALLBACK (on_modem_removed),
                    self);
}

/*
 * GObject
 */
static void
valent_telephony_constructed (GObject *object)
{
  ValentTelephony *self = VALENT_TELEPHONY (object);

  g_dbus_object_manager_client_new_for_bus (G_BUS_TYPE_SYSTEM,
                                            G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START,
                                            "org.freedesktop.ModemManager1",
                                            "/org/freedesktop/ModemManager1",
                                            NULL, NULL, NULL,
                                            NULL,
                                            (GAsyncReadyCallback)g_dbus_object_manager_client_new_for_bus_cb,
                                            g_object_ref (self));

  G_OBJECT_CLASS (valent_telephony_parent_class)->constructed (object);
}

static void
valent_telephony_dispose (GObject *object)
{
  ValentTelephony *self = VALENT_TELEPHONY (object);

  g_signal_handlers_disconnect_by_data (self->manager, self);
  g_clear_object (&self->manager);

  G_OBJECT_CLASS (valent_telephony_parent_class)->dispose (object);
}

static void
valent_telephony_finalize (GObject *object)
{
  ValentTelephony *self = VALENT_TELEPHONY (object);

  g_clear_pointer (&self->modems, g_hash_table_unref);

  G_OBJECT_CLASS (valent_telephony_parent_class)->finalize (object);
}

static void
valent_telephony_class_init (ValentTelephonyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_telephony_constructed;
  object_class->dispose = valent_telephony_dispose;
  object_class->finalize = valent_telephony_finalize;

  /**
   * ValentTelephony::changed:
   * @self: a #ValentTelephony
   *
   * #ValentTelephony::changed is emitted whenever a relevant property changes.
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
valent_telephony_init (ValentTelephony *self)
{
  self->modems = g_hash_table_new_full (g_str_hash,
                                        g_str_equal,
                                        g_free,
                                        g_object_unref);
}

/**
 * valent_telephony_get_default:
 *
 * Get the default [class@Valent.Network].
 *
 * Returns: (transfer none) (not nullable): a #ValentTelephony
 *
 * Since: 1.0
 */
ValentTelephony *
valent_telephony_get_default (void)
{
  if (default_telephony == NULL)
    {
      default_telephony = g_object_new (VALENT_TYPE_TELEPHONY, NULL);

      g_object_add_weak_pointer (G_OBJECT (default_telephony),
                                 (gpointer)&default_telephony);
    }

  return default_telephony;
}

static JsonNode *
valent_telephony_serialize_modem (GDBusProxy *proxy)
{
  g_autoptr (JsonBuilder) builder = NULL;
  GVariant *value;
  unsigned int access_technologies;
  uint32_t signal_quality;
  gboolean signal_recent;
  int32_t state;
  const char *telephony_type;
  int64_t signal_strength = -1;

  g_assert (G_IS_DBUS_PROXY (proxy));

  /* Extract the relevant properties */
  value = g_dbus_proxy_get_cached_property (proxy, "AccessTechnologies");
  g_variant_get (value, "u", &access_technologies);
  g_clear_pointer (&value, g_variant_unref);

  value = g_dbus_proxy_get_cached_property (proxy, "SignalQuality");
  g_variant_get (value, "(ub)", &signal_quality, &signal_recent);
  g_clear_pointer (&value, g_variant_unref);

  value = g_dbus_proxy_get_cached_property (proxy, "State");
  g_variant_get (value, "i", &state);
  g_clear_pointer (&value, g_variant_unref);

  /* Convert to KDE Connect values (`networkType`, `signalStrength`) */
  telephony_type = get_telephony_type_string (access_technologies);

  if (state >= MM_MODEM_STATE_ENABLED)
    signal_strength = (int64_t)(signal_quality / 20);

  /* Serialize to a JsonNode */
  builder = json_builder_new ();
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "networkType");
  json_builder_add_string_value (builder, telephony_type);
  json_builder_set_member_name (builder, "signalStrength");
  json_builder_add_int_value (builder, signal_strength);
  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

/**
 * valent_telephony_get_signal_strengths:
 * @telephony: a #ValentTelephony
 *
 * Get a serialized dictionary of the known modems' status.
 *
 * Returns: (transfer full) (nullable): a #JsonNode
 *
 * Since: 1.0
 */
JsonNode *
valent_telephony_get_signal_strengths (ValentTelephony *telephony)
{
  g_autoptr (JsonBuilder) builder = NULL;
  GHashTableIter iter;
  gpointer key, value;
  unsigned int m = 0;

  g_return_val_if_fail (VALENT_IS_TELEPHONY (telephony), NULL);

  builder = json_builder_new ();
  json_builder_begin_object (builder);

  g_hash_table_iter_init (&iter, telephony->modems);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      g_autoptr (JsonNode) modem = NULL;
      g_autofree char *id = NULL;

      id = g_strdup_printf ("%u", m++);
      modem = valent_telephony_serialize_modem (G_DBUS_PROXY (value));

      json_builder_set_member_name (builder, id);
      json_builder_add_value (builder, g_steal_pointer (&modem));
    }

  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}

