// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-lan-dnssd"

#include "config.h"

#include <gio/gio.h>
#include <valent.h>

#include "valent-lan-dnssd.h"
#include "valent-lan-utils.h"

#define AVAHI_DBUS_NAME             "org.freedesktop.Avahi"
#define AVAHI_SERVER2_PATH          "/"
#define AVAHI_SERVER2_IFACE         "org.freedesktop.Avahi.Server2"
#define AVAHI_ENTRY_GROUP_IFACE     "org.freedesktop.Avahi.EntryGroup"
#define AVAHI_SERVICE_BROWSER_IFACE "org.freedesktop.Avahi.ServiceBrowser"

#define KDECONNECT_UDP_SERVICE_TYPE "_kdeconnect._udp"


/**
 * ValentLanDNSSD:
 *
 * A simple DNS-SD manager.
 *
 * `ValentLanDNSSD` implements [iface@Gio.ListModel], representing discovered
 * services as [class@Gio.SocketAddress] objects. The [type@GLib.MainContext]
 * passed to [method@Valent.LanDNSSD.attach] is the thread and context where
 * [signal@Gio.ListModel::items-changed] is emitted.
 *
 * If the [property@ValentLanDNSSD:identity] property is set to a KDE Connect
 * identity packet (`kdeconnect.identity`), it will export a service with the
 * type `_kdeconnect._udp`.
 */

struct _ValentLanDNSSD
{
  ValentObject      parent_instance;

  GPtrArray        *items;
  JsonNode         *identity;

  char             *name;
  char             *type;
  uint16_t          port;
  GVariant         *txt;

  GMainContext     *context;
  GDBusConnection  *connection;
  GCancellable     *cancellable;
  unsigned int      watcher_id;

  unsigned int      server_state;
  unsigned int      server_state_id;
  char             *entry_group_path;
  unsigned int      entry_group_state;
  unsigned int      entry_group_state_id;
  char             *service_browser_path;
  unsigned int      service_browser_event_id;
};

static void   g_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentLanDNSSD, valent_lan_dnssd, VALENT_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

enum {
  PROP_0,
  PROP_IDENTITY,
  PROP_SERVICE_TYPE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static inline GVariant *
txt_new_str (const char *name,
             const char *value)
{
  char *key = g_strdup_printf ("%s=%s", name, value);
  g_autoptr (GBytes) bytes = g_bytes_new_take (key, strlen (key));

  return g_variant_new_from_bytes (G_VARIANT_TYPE_BYTESTRING, bytes, TRUE);
}

static inline GVariant *
txt_new_uint (const char *name,
              uint32_t    value)
{
  char *key = g_strdup_printf ("%s=%u", name, value);
  g_autoptr (GBytes) bytes = g_bytes_new_take (key, strlen (key));

  return g_variant_new_from_bytes (G_VARIANT_TYPE_BYTESTRING, bytes, TRUE);
}

static inline GWeakRef *
weak_ref_new (gpointer object)
{
  GWeakRef *weak_ref;

  g_assert (object == NULL || G_IS_OBJECT (object));

  weak_ref = g_new0 (GWeakRef, 1);
  g_weak_ref_init (weak_ref, object);

  return g_steal_pointer (&weak_ref);
}

static inline void
weak_ref_free (gpointer data)
{
  GWeakRef *weak_ref = data;

  g_weak_ref_clear (weak_ref);
  g_free (weak_ref);
}

/*
 * GListModel
 */
static gpointer
valent_lan_dnssd_get_item (GListModel   *list,
                           unsigned int  position)
{
  ValentLanDNSSD *self = VALENT_LAN_DNSSD (list);
  gpointer ret = NULL;

  g_assert (VALENT_IS_LAN_DNSSD (self));

  valent_object_lock (VALENT_OBJECT (self));
  if G_LIKELY (position < self->items->len)
    ret = g_object_ref (g_ptr_array_index (self->items, position));
  valent_object_unlock (VALENT_OBJECT (self));

  return g_steal_pointer (&ret);
}

static GType
valent_lan_dnssd_get_item_type (GListModel *list)
{
  return G_TYPE_SOCKET_ADDRESS;
}

static unsigned int
valent_lan_dnssd_get_n_items (GListModel *list)
{
  ValentLanDNSSD *self = VALENT_LAN_DNSSD (list);
  unsigned int ret = 0;

  g_assert (VALENT_IS_LAN_DNSSD (self));

  valent_object_lock (VALENT_OBJECT (self));
  ret = self->items->len;
  valent_object_unlock (VALENT_OBJECT (self));

  return ret;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_lan_dnssd_get_item;
  iface->get_item_type = valent_lan_dnssd_get_item_type;
  iface->get_n_items = valent_lan_dnssd_get_n_items;
}

/*
 * Avahi D-Bus Service
 *
 * See Also:
 *   - https://github.com/lathiat/avahi/blob/master/avahi-daemon/org.freedesktop.Avahi.Server.xml
 *   - https://github.com/lathiat/avahi/blob/master/avahi-daemon/org.freedesktop.Avahi.ServiceBrowser.xml
 *   - https://github.com/lathiat/avahi/blob/master/avahi-daemon/org.freedesktop.Avahi.EntryGroup.xml
 */
enum {
  _AVAHI_SERVER_INVALID,
  _AVAHI_SERVER_REGISTERING,
  _AVAHI_SERVER_RUNNING,
  _AVAHI_SERVER_COLLISION,
  _AVAHI_SERVER_FAILURE,
};

enum {
  _AVAHI_ENTRY_GROUP_UNCOMMITTED,
  _AVAHI_ENTRY_GROUP_REGISTERING,
  _AVAHI_ENTRY_GROUP_ESTABLISHED,
  _AVAHI_ENTRY_GROUP_COLLISION,
  _AVAHI_ENTRY_GROUP_FAILURE,
};

static gboolean   _avahi_client_connect          (ValentLanDNSSD *self);
static gboolean   _avahi_client_disconnect       (ValentLanDNSSD *self);
static gboolean   _avahi_entry_group_new         (ValentLanDNSSD *self);
static gboolean   _avahi_entry_group_add_service (ValentLanDNSSD *self);
static gboolean   _avahi_entry_group_commit      (ValentLanDNSSD *self);
static gboolean   _avahi_entry_group_reset       (ValentLanDNSSD *self);
static gboolean   _avahi_service_browser_prepare (ValentLanDNSSD *self);


static void
_avahi_entry_group_state_changed (GDBusConnection *connection,
                                  const char      *sender_name,
                                  const char      *object_path,
                                  const char      *interface_name,
                                  const char      *signal_name,
                                  GVariant        *parameters,
                                  gpointer         user_data)
{
  g_autoptr (ValentLanDNSSD) self = g_weak_ref_get ((GWeakRef *)user_data);
  const char *error = NULL;

  g_assert (self == NULL || VALENT_IS_LAN_DNSSD (self));
  g_assert (g_str_equal (signal_name, "StateChanged"));

  if (self == NULL || valent_object_in_destruction (VALENT_OBJECT (self)))
    return;

  valent_object_lock (VALENT_OBJECT (self));
  g_variant_get (parameters, "(i&s)", &self->entry_group_state, &error);

  VALENT_NOTE ("[%i] %s", self->entry_group_state, error);

  switch (self->entry_group_state)
    {
    case _AVAHI_ENTRY_GROUP_UNCOMMITTED:
      _avahi_entry_group_commit (self);
      break;

    case _AVAHI_ENTRY_GROUP_REGISTERING:
    case _AVAHI_ENTRY_GROUP_ESTABLISHED:
      break;

    case _AVAHI_ENTRY_GROUP_COLLISION:
      g_warning ("%s(): DNS-SD service name \"%s\" already registered",
                 G_STRFUNC, self->name);
      break;

    case _AVAHI_ENTRY_GROUP_FAILURE:
      g_warning ("%s(): DNS-SD failure: %s", G_STRFUNC, error);
      break;
    }
  valent_object_unlock (VALENT_OBJECT (self));
}

static gboolean
_avahi_entry_group_new (ValentLanDNSSD *self)
{
  g_assert (VALENT_IS_LAN_DNSSD (self));

  valent_object_lock (VALENT_OBJECT (self));
  if (self->entry_group_path == NULL)
    {
      g_autoptr (GVariant) reply = NULL;
      g_autoptr (GError) error = NULL;

      reply = g_dbus_connection_call_sync (self->connection,
                                           AVAHI_DBUS_NAME,
                                           AVAHI_SERVER2_PATH,
                                           AVAHI_SERVER2_IFACE,
                                           "EntryGroupNew",
                                           NULL,
                                           G_VARIANT_TYPE ("(o)"),
                                           G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                           -1,
                                           self->cancellable,
                                           &error);

      if (reply == NULL)
        {
          if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning ("%s(): %s", G_STRFUNC, error->message);

          goto unlock_exit;
        }

      g_variant_get (reply, "(o)", &self->entry_group_path);
      g_clear_pointer (&reply, g_variant_unref);

      reply = g_dbus_connection_call_sync (self->connection,
                                           AVAHI_DBUS_NAME,
                                           self->entry_group_path,
                                           AVAHI_ENTRY_GROUP_IFACE,
                                           "GetState",
                                           NULL,
                                           NULL,
                                           G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                           -1,
                                           self->cancellable,
                                           &error);

      if (reply == NULL)
        {
          if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning ("%s(): %s", G_STRFUNC, error->message);

          goto unlock_exit;
        }

      g_variant_get (reply, "(i)", &self->entry_group_state);
      self->entry_group_state_id =
        g_dbus_connection_signal_subscribe (self->connection,
                                            AVAHI_DBUS_NAME,
                                            AVAHI_ENTRY_GROUP_IFACE,
                                            "StateChanged",
                                            self->entry_group_path,
                                            NULL,
                                            G_DBUS_SIGNAL_FLAGS_NONE,
                                            _avahi_entry_group_state_changed,
                                            weak_ref_new (self),
                                            weak_ref_free);

      /* If the initial state is "uncommitted" call `AddService()` then
       * `Commit()`, since `StateChanged` won't be emitted in that case.
       */
      if (self->entry_group_state == _AVAHI_ENTRY_GROUP_UNCOMMITTED)
        {
          g_main_context_invoke_full (self->context,
                                      G_PRIORITY_DEFAULT,
                                      G_SOURCE_FUNC (_avahi_entry_group_add_service),
                                      g_object_ref (self),
                                      g_object_unref);
        }
    }

unlock_exit:
  valent_object_unlock (VALENT_OBJECT (self));

  return G_SOURCE_REMOVE;
}

static void
_avahi_entry_group_add_service_cb (GDBusConnection *connection,
                                   GAsyncResult    *result,
                                   ValentLanDNSSD  *self)
{
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);

  if (reply == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  _avahi_entry_group_commit (self);
}

static gboolean
_avahi_entry_group_add_service (ValentLanDNSSD *self)
{
  valent_object_lock (VALENT_OBJECT (self));
  if (self->identity == NULL || self->entry_group_path == NULL)
    goto unlock_exit;

  if (self->entry_group_state == _AVAHI_ENTRY_GROUP_UNCOMMITTED)
    {
      g_dbus_connection_call (self->connection,
                              AVAHI_DBUS_NAME,
                              self->entry_group_path,
                              AVAHI_ENTRY_GROUP_IFACE,
                              "AddService",
                              g_variant_new ("(iiussssq@aay)",
                                             -1, // interface: AVAHI_IF_UNSPEC
                                             -1, // protocol:  AVAHI_PROTO_UNSPEC
                                             64, // flags:     AVAHI_PUBLISH_UPDATE
                                             self->name,
                                             self->type,
                                             "", // domain
                                             "", // host
                                             self->port,
                                             self->txt),
                              NULL,
                              G_DBUS_CALL_FLAGS_NO_AUTO_START,
                              -1,
                              self->cancellable,
                              (GAsyncReadyCallback)_avahi_entry_group_add_service_cb,
                              self);
    }
  else if (self->entry_group_state == _AVAHI_ENTRY_GROUP_REGISTERING ||
           self->entry_group_state == _AVAHI_ENTRY_GROUP_ESTABLISHED)
    {
      g_dbus_connection_call (self->connection,
                              AVAHI_DBUS_NAME,
                              self->entry_group_path,
                              AVAHI_ENTRY_GROUP_IFACE,
                              "UpdateServiceTxt",
                              g_variant_new ("(iiusss@aay)",
                                             -1, // interface: AVAHI_IF_UNSPEC
                                             -1, // protocol:  AVAHI_PROTO_UNSPEC
                                              0, // flags:     AvahiPublishFlags
                                             self->name,
                                             self->type,
                                             "", // domain
                                             self->txt),
                              NULL,
                              G_DBUS_CALL_FLAGS_NO_AUTO_START,
                              -1,
                              self->cancellable,
                              NULL,
                              NULL);
    }

unlock_exit:
  valent_object_unlock (VALENT_OBJECT (self));

  return G_SOURCE_REMOVE;
}

static gboolean
_avahi_entry_group_commit (ValentLanDNSSD *self)
{
  valent_object_lock (VALENT_OBJECT (self));
  if (self->entry_group_path != NULL &&
      self->entry_group_state == _AVAHI_ENTRY_GROUP_UNCOMMITTED)
    {
      g_dbus_connection_call (self->connection,
                              AVAHI_DBUS_NAME,
                              self->entry_group_path,
                              AVAHI_ENTRY_GROUP_IFACE,
                              "Commit",
                              NULL,
                              NULL,
                              G_DBUS_CALL_FLAGS_NO_AUTO_START,
                              -1,
                              self->cancellable,
                              NULL,
                              NULL);
    }
  valent_object_unlock (VALENT_OBJECT (self));

  return G_SOURCE_REMOVE;
}

static gboolean
_avahi_entry_group_reset (ValentLanDNSSD *self)
{
  valent_object_lock (VALENT_OBJECT (self));
  if (self->entry_group_path != NULL &&
      (self->entry_group_state == _AVAHI_ENTRY_GROUP_REGISTERING ||
       self->entry_group_state == _AVAHI_ENTRY_GROUP_ESTABLISHED))
    {
      g_dbus_connection_call (self->connection,
                              AVAHI_DBUS_NAME,
                              self->entry_group_path,
                              AVAHI_ENTRY_GROUP_IFACE,
                              "Reset",
                              NULL,
                              NULL,
                              G_DBUS_CALL_FLAGS_NO_AUTO_START,
                              -1,
                              self->cancellable,
                              NULL,
                              NULL);
    }
  valent_object_unlock (VALENT_OBJECT (self));

  return G_SOURCE_REMOVE;
}

static void
_avahi_resolve_service_cb (GDBusConnection *connection,
                           GAsyncResult    *result,
                           gpointer         user_data)
{
  ValentLanDNSSD *self = VALENT_LAN_DNSSD (user_data);
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  int interface = 0;
  int protocol = 0;
  const char *name = NULL;
  const char *type = NULL;
  const char *domain = NULL;
  const char *host = NULL;
  int aprotocol = 0;
  const char *address = NULL;
  uint16_t port = 0;
  g_autoptr (GVariant) txt = NULL;
  uint32_t flags = 0;

  g_autoptr (GSocketAddress) saddress = NULL;
  unsigned int position = 0;

  reply = g_dbus_connection_call_finish (connection, result, &error);

  if (reply == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  g_variant_get (reply,
                 "(ii&s&s&s&si&sq@aayu)",
                 &interface,
                 &protocol,
                 &name,
                 &type,
                 &domain,
                 &host,
                 &aprotocol,
                 &address,
                 &port,
                 &txt,
                 &flags);

  saddress = g_inet_socket_address_new_from_string (address, port);
  g_return_if_fail (G_IS_SOCKET_ADDRESS (saddress));
  _g_socket_address_set_dnssd_name (saddress, name);

  valent_object_lock (VALENT_OBJECT (self));
  position = self->items->len;
  g_ptr_array_add (self->items, g_steal_pointer (&saddress));
  valent_object_unlock (VALENT_OBJECT (self));

  /* NOTE: `items-changed` is emitted in Avahi's thread-context */
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
}

static void
_avahi_service_browser_event (GDBusConnection *connection,
                              const char      *sender_name,
                              const char      *object_path,
                              const char      *interface_name,
                              const char      *signal_name,
                              GVariant        *parameters,
                              gpointer         user_data)
{
  g_autoptr (ValentLanDNSSD) self = g_weak_ref_get ((GWeakRef *)user_data);
  int interface = 0;
  int protocol = 0;
  const char *name = 0;
  const char *type = 0;
  const char *domain = 0;
  uint32_t flags = 0;

  g_assert (self == NULL || VALENT_IS_LAN_DNSSD (self));
  g_assert (signal_name != NULL && parameters != NULL);

  if (self == NULL || valent_object_in_destruction (VALENT_OBJECT (self)))
    return;

  /* Ignoring "CacheExhausted", "AllForNow" */
  VALENT_NOTE ("%s", signal_name);

  if (g_str_equal (signal_name, "ItemNew"))
    {
      g_variant_get (parameters,
                     "(ii&s&s&su)",
                     &interface,
                     &protocol,
                     &name,
                     &type,
                     &domain,
                     &flags);

      g_dbus_connection_call (connection,
                              AVAHI_DBUS_NAME,
                              AVAHI_SERVER2_PATH,
                              AVAHI_SERVER2_IFACE,
                              "ResolveService",
                              g_variant_new ("(iisssiu)",
                                             interface,
                                             protocol,
                                             name,
                                             type,
                                             domain,
                                             -1, // aprotocol: AVAHI_PROTO_UNSPEC
                                             0), // flags: AvahiLookupFlags
                              G_VARIANT_TYPE ("(iissssisqaayu)"),
                              G_DBUS_CALL_FLAGS_NO_AUTO_START,
                              -1,
                              NULL,
                              (GAsyncReadyCallback)_avahi_resolve_service_cb,
                              self);
    }
  else if (g_str_equal (signal_name, "ItemRemove"))
    {
      g_variant_get (parameters,
                     "(ii&s&s&su)",
                     &interface,
                     &protocol,
                     &name,
                     &type,
                     &domain,
                     &flags);

      for (unsigned int i = 0; i < self->items->len; i++)
        {
          GSocketAddress *saddress = NULL;
          GSocketFamily sprotocol = G_SOCKET_FAMILY_INVALID;
          const char *device_id = NULL;

          saddress = g_ptr_array_index (self->items, i);
          sprotocol = g_socket_address_get_family (saddress);

          /* NOTE: IPv4 = 0, IPv6 = 1, Any = -1 */
          if (protocol != -1)
            {
              if ((protocol == 1 && sprotocol != G_SOCKET_FAMILY_IPV6) ||
                  (protocol == 0 && sprotocol != G_SOCKET_FAMILY_IPV4))
                continue;
            }

          device_id = _g_socket_address_get_dnssd_name (saddress);

          if (!g_str_equal (device_id, name))
            continue;

          valent_object_lock (VALENT_OBJECT (self));
          g_ptr_array_remove_index (self->items, i);
          valent_object_unlock (VALENT_OBJECT (self));

          g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
        }
    }
  else if (g_str_equal (signal_name, "Failure"))
    {
      const char *error = NULL;

      g_variant_get (parameters, "&s", &error);
      g_warning ("%s(): %s", G_STRFUNC, error);
    }
}

static void
_avahi_service_browser_start_cb (GDBusConnection *connection,
                                 GAsyncResult    *result,
                                 ValentLanDNSSD  *self)
{
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);

  if (reply == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      valent_object_lock (VALENT_OBJECT (self));
      if (self->service_browser_event_id != 0)
        {
          g_dbus_connection_signal_unsubscribe (connection,
                                                self->service_browser_event_id);
          self->service_browser_event_id = 0;
        }

      g_clear_pointer (&self->service_browser_path, g_free);
      valent_object_unlock (VALENT_OBJECT (self));
    }
}

static gboolean
_avahi_service_browser_prepare (ValentLanDNSSD *self)
{
  g_assert (VALENT_IS_LAN_DNSSD (self));

  valent_object_lock (VALENT_OBJECT (self));
  if (self->service_browser_path == NULL)
    {
      g_autoptr (GVariant) reply = NULL;
      g_autoptr (GError) error = NULL;

      reply = g_dbus_connection_call_sync (self->connection,
                                           AVAHI_DBUS_NAME,
                                           AVAHI_SERVER2_PATH,
                                           AVAHI_SERVER2_IFACE,
                                           "ServiceBrowserPrepare",
                                           g_variant_new ("(iissu)",
                                                          -1, // interface: AVAHI_IF_UNSPEC
                                                          -1, // protocol: AVAHI_PROTO_UNSPEC
                                                          self->type,
                                                          "", // domain
                                                          0), // flags: AvahiLookupFlags
                                           G_VARIANT_TYPE ("(o)"),
                                           G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                           -1,
                                           self->cancellable,
                                           &error);

      if (reply == NULL)
        {
          if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning ("%s(): %s", G_STRFUNC, error->message);

          goto unlock_exit;
        }

      g_variant_get (reply, "(o)", &self->service_browser_path);

      self->service_browser_event_id =
        g_dbus_connection_signal_subscribe (self->connection,
                                            AVAHI_DBUS_NAME,
                                            AVAHI_SERVICE_BROWSER_IFACE,
                                            NULL, // all signals
                                            self->service_browser_path,
                                            NULL,
                                            G_DBUS_SIGNAL_FLAGS_NONE,
                                            _avahi_service_browser_event,
                                            weak_ref_new (self),
                                            weak_ref_free);

      g_dbus_connection_call (self->connection,
                              AVAHI_DBUS_NAME,
                              self->service_browser_path,
                              AVAHI_SERVICE_BROWSER_IFACE,
                              "Start",
                              NULL,
                              NULL,
                              G_DBUS_CALL_FLAGS_NO_AUTO_START,
                              -1,
                              self->cancellable,
                              (GAsyncReadyCallback)_avahi_service_browser_start_cb,
                              self);
    }

unlock_exit:
  valent_object_unlock (VALENT_OBJECT (self));

  return G_SOURCE_REMOVE;
}

static void
_avahi_server_state_changed (GDBusConnection *connection,
                             const char      *sender_name,
                             const char      *object_path,
                             const char      *interface_name,
                             const char      *signal_name,
                             GVariant        *parameters,
                             gpointer         user_data)
{
  g_autoptr (ValentLanDNSSD) self = g_weak_ref_get ((GWeakRef *)user_data);
  const char *error = NULL;

  g_assert (self == NULL || VALENT_IS_LAN_DNSSD (self));
  g_assert (g_str_equal (signal_name, "StateChanged"));

  if (self == NULL || valent_object_in_destruction (VALENT_OBJECT (self)))
    return;

  valent_object_lock (VALENT_OBJECT (self));
  g_variant_get (parameters, "(i&s)", &self->server_state, &error);

  VALENT_NOTE ("[%i] %s", self->server_state, error);

  switch (self->server_state)
    {
    case _AVAHI_SERVER_INVALID:
    case _AVAHI_SERVER_REGISTERING:
      break;

    case _AVAHI_SERVER_RUNNING:
      g_main_context_invoke_full (self->context,
                                  G_PRIORITY_DEFAULT,
                                  G_SOURCE_FUNC (_avahi_entry_group_new),
                                  g_object_ref (self),
                                  g_object_unref);
      g_main_context_invoke_full (self->context,
                                  G_PRIORITY_DEFAULT,
                                  G_SOURCE_FUNC (_avahi_service_browser_prepare),
                                  g_object_ref (self),
                                  g_object_unref);
      break;

    case _AVAHI_SERVER_COLLISION:
      g_warning ("%s(): DNS-SD server collision: %s", G_STRFUNC, error);
      break;

    case _AVAHI_SERVER_FAILURE:
      g_warning ("%s(): DNS-SD server failure: %s", G_STRFUNC, error);
      break;
    }
  valent_object_unlock (VALENT_OBJECT (self));
}

static void
on_name_appeared (GDBusConnection *connection,
                  const char      *name,
                  const char      *name_owner,
                  ValentLanDNSSD  *self)
{
  g_autoptr (GCancellable) destroy = NULL;
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_LAN_DNSSD (self));

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  reply = g_dbus_connection_call_sync (connection,
                                       AVAHI_DBUS_NAME,
                                       AVAHI_SERVER2_PATH,
                                       AVAHI_SERVER2_IFACE,
                                       "GetState",
                                       NULL,
                                       NULL,
                                       G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                       -1,
                                       destroy,
                                       &error);

  if (reply == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  valent_object_lock (VALENT_OBJECT (self));
  /* Create a new cancellable, chained to the object's cancellable, so that
   * any operations will be cancelled if the object is destroyed.
   */
  self->connection = g_object_ref (connection);
  self->cancellable = g_cancellable_new ();
  g_signal_connect_object (destroy,
                           "cancelled",
                           G_CALLBACK (g_cancellable_cancel),
                           self->cancellable,
                           G_CONNECT_SWAPPED);

  g_variant_get (reply, "(i)", &self->server_state);
  self->server_state_id =
    g_dbus_connection_signal_subscribe (self->connection,
                                        AVAHI_DBUS_NAME,
                                        AVAHI_SERVER2_IFACE,
                                        "StateChanged",
                                        AVAHI_SERVER2_PATH,
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        _avahi_server_state_changed,
                                        weak_ref_new (self),
                                        weak_ref_free);


  /* If the initial state is "running" call `EntryGroupNew()` and
   * `ServiceBrowserPrepare()`, otherwise wait for a `StateChanged` emission.
   */
  if (self->server_state == _AVAHI_SERVER_RUNNING)
    {
      g_main_context_invoke_full (self->context,
                                  G_PRIORITY_DEFAULT,
                                  G_SOURCE_FUNC (_avahi_entry_group_new),
                                  g_object_ref (self),
                                  g_object_unref);
      g_main_context_invoke_full (self->context,
                                  G_PRIORITY_DEFAULT,
                                  G_SOURCE_FUNC (_avahi_service_browser_prepare),
                                  g_object_ref (self),
                                  g_object_unref);
    }
  valent_object_unlock (VALENT_OBJECT (self));
}

static void
on_name_vanished (GDBusConnection *connection,
                  const char      *name,
                  ValentLanDNSSD  *self)
{
  g_assert (VALENT_IS_LAN_DNSSD (self));

  valent_object_lock (VALENT_OBJECT (self));
  g_cancellable_cancel (self->cancellable);

  if (self->connection != NULL)
    {
      if (self->server_state_id != 0)
        {
          g_dbus_connection_signal_unsubscribe (self->connection,
                                                self->server_state_id);
          self->server_state_id = 0;
        }

      if (self->entry_group_state_id != 0)
        {
          g_dbus_connection_signal_unsubscribe (self->connection,
                                                self->entry_group_state_id);
          self->entry_group_state_id = 0;
        }

      if (self->service_browser_event_id != 0)
        {
          g_dbus_connection_signal_unsubscribe (self->connection,
                                                self->service_browser_event_id);
          self->service_browser_event_id = 0;
        }

      self->entry_group_state = _AVAHI_ENTRY_GROUP_UNCOMMITTED;
      self->server_state = _AVAHI_SERVER_INVALID;

      g_clear_pointer (&self->service_browser_path, g_free);
      g_clear_pointer (&self->entry_group_path, g_free);
      g_clear_object (&self->connection);
      g_clear_object (&self->cancellable);
    }
  valent_object_unlock (VALENT_OBJECT (self));
}

static gboolean
_avahi_client_connect (ValentLanDNSSD *self)
{
  valent_object_lock (VALENT_OBJECT (self));
  if (self->watcher_id == 0)
    {
      self->watcher_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                           AVAHI_DBUS_NAME,
                                           G_BUS_NAME_WATCHER_FLAGS_NONE,
                                           (GBusNameAppearedCallback)on_name_appeared,
                                           (GBusNameVanishedCallback)on_name_vanished,
                                           self, NULL);
    }
  valent_object_unlock (VALENT_OBJECT (self));

  return G_SOURCE_REMOVE;
}

static gboolean
_avahi_client_disconnect (ValentLanDNSSD *self)
{
  valent_object_lock (VALENT_OBJECT (self));
  g_cancellable_cancel (self->cancellable);

  if (self->connection != NULL)
    {
      if (self->entry_group_path != NULL)
        {
          g_dbus_connection_call (self->connection,
                                  AVAHI_DBUS_NAME,
                                  self->entry_group_path,
                                  AVAHI_ENTRY_GROUP_IFACE,
                                  "Free",
                                  NULL,
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                  -1,
                                  NULL,
                                  NULL,
                                  NULL);
        }

      if (self->service_browser_path != NULL)
        {
          g_dbus_connection_call (self->connection,
                                  AVAHI_DBUS_NAME,
                                  self->service_browser_path,
                                  AVAHI_SERVICE_BROWSER_IFACE,
                                  "Free",
                                  NULL,
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                  -1,
                                  NULL,
                                  NULL,
                                  NULL);
        }

      g_clear_handle_id (&self->watcher_id, g_bus_unwatch_name);
      on_name_vanished (self->connection, AVAHI_DBUS_NAME, self);
    }
  valent_object_unlock (VALENT_OBJECT (self));

  return G_SOURCE_REMOVE;
}

/*
 * ValentLanDNSSD
 */
static void
valent_lan_dnssd_set_identity (ValentLanDNSSD *self,
                               JsonNode       *identity)
{
  g_autoptr (GPtrArray) txt = NULL;
  const char *id = NULL;
  const char *name = NULL;
  const char *type = NULL;
  gint64 protocol = 0;
  gint64 port = VALENT_LAN_PROTOCOL_PORT;

  g_assert (VALENT_IS_LAN_DNSSD (self));
  g_assert (identity == NULL || VALENT_IS_PACKET (identity));

  if (identity == NULL)
    {
      g_clear_pointer (&self->identity, json_node_unref);

      if (self->entry_group_path != NULL)
        {
          g_main_context_invoke_full (self->context,
                                      G_PRIORITY_DEFAULT,
                                      G_SOURCE_FUNC (_avahi_entry_group_reset),
                                      g_object_ref (self),
                                      g_object_unref);
        }
      return;
    }

  /* Even if the pointers match, assume the contents have changed */
  if (self->identity != identity)
    {
      g_clear_pointer (&self->identity, json_node_unref);
      self->identity = json_node_ref (identity);
    }

  /* Service TXT Record */
  txt = g_ptr_array_new ();

  if (valent_packet_get_string (identity, "deviceId", &id))
    g_ptr_array_add (txt, txt_new_str ("id", id));

  if (valent_packet_get_string (identity, "deviceName", &name))
    g_ptr_array_add (txt, txt_new_str ("name", name));

  if (valent_packet_get_string (identity, "deviceType", &type))
    g_ptr_array_add (txt, txt_new_str ("type", type));

  if (valent_packet_get_int (identity, "protocolVersion", &protocol))
    g_ptr_array_add (txt, txt_new_uint ("protocol", (uint32_t)protocol));

  g_clear_pointer (&self->txt, g_variant_unref);
  self->txt = g_variant_new_array (G_VARIANT_TYPE_BYTESTRING,
                                   (GVariant * const *)txt->pdata,
                                   txt->len);
  g_variant_ref_sink (self->txt);

  /* Service Name and Port */
  g_set_str (&self->name, id);

  if (valent_packet_get_int (identity, "tcpPort", &port))
    self->port = (uint16_t)port;

  if (self->entry_group_path != NULL)
    {
      g_main_context_invoke_full (self->context,
                                  G_PRIORITY_DEFAULT,
                                  G_SOURCE_FUNC (_avahi_entry_group_add_service),
                                  g_object_ref (self),
                                  g_object_unref);
    }
}

static void
valent_lan_dnssd_set_service_type (ValentLanDNSSD *self,
                                   const char     *type)
{
  g_assert (VALENT_IS_LAN_DNSSD (self));
  g_assert (type == NULL || *type != '\0');

  if (type == NULL)
    type = KDECONNECT_UDP_SERVICE_TYPE;

  if (g_set_str (&self->type, type))
    {
      valent_object_notify_by_pspec (VALENT_OBJECT (self),
                                     properties [PROP_SERVICE_TYPE]);
    }
}

/*
 * ValentObject
 */
static void
valent_lan_dnssd_destroy (ValentObject *object)
{
  ValentLanDNSSD *self = VALENT_LAN_DNSSD (object);

  _avahi_client_disconnect (self);

  g_clear_pointer (&self->context, g_main_context_unref);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->type, g_free);
  g_clear_pointer (&self->txt, g_variant_unref);

  VALENT_OBJECT_CLASS (valent_lan_dnssd_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_lan_dnssd_finalize (GObject *object)
{
  ValentLanDNSSD *self = VALENT_LAN_DNSSD (object);

  valent_object_lock (VALENT_OBJECT (object));
  g_clear_pointer (&self->items, g_ptr_array_unref);
  g_clear_pointer (&self->identity, json_node_unref);
  valent_object_unlock (VALENT_OBJECT (object));

  G_OBJECT_CLASS (valent_lan_dnssd_parent_class)->finalize (object);
}

static void
valent_lan_dnssd_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  ValentLanDNSSD *self = VALENT_LAN_DNSSD (object);

  switch (prop_id)
    {
    case PROP_IDENTITY:
      valent_object_lock (VALENT_OBJECT (self));
      g_value_set_boxed (value, self->identity);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    case PROP_SERVICE_TYPE:
      valent_object_lock (VALENT_OBJECT (self));
      g_value_set_string (value, self->type);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_lan_dnssd_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  ValentLanDNSSD *self = VALENT_LAN_DNSSD (object);

  switch (prop_id)
    {
    case PROP_IDENTITY:
      valent_object_lock (VALENT_OBJECT (self));
      valent_lan_dnssd_set_identity (self, g_value_get_boxed (value));
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    case PROP_SERVICE_TYPE:
      valent_object_lock (VALENT_OBJECT (self));
      valent_lan_dnssd_set_service_type (self, g_value_get_string (value));
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_lan_dnssd_class_init (ValentLanDNSSDClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);

  object_class->finalize = valent_lan_dnssd_finalize;
  object_class->get_property = valent_lan_dnssd_get_property;
  object_class->set_property = valent_lan_dnssd_set_property;

  vobject_class->destroy = valent_lan_dnssd_destroy;

  /**
   * ValentLanDNSSD:identity:
   *
   * The KDE Connect packet holding the local identity.
   */
  properties [PROP_IDENTITY] =
    g_param_spec_boxed ("identity", NULL, NULL,
                        JSON_TYPE_NODE,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentLanDNSSD:service-type:
   *
   * The DNS-SD service type to register and observe.
   */
  properties [PROP_SERVICE_TYPE] =
    g_param_spec_string ("service-type", NULL, NULL,
                         KDECONNECT_UDP_SERVICE_TYPE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_lan_dnssd_init (ValentLanDNSSD *self)
{
  valent_object_lock (VALENT_OBJECT (self));
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
  self->type = g_strdup (KDECONNECT_UDP_SERVICE_TYPE);
  valent_object_unlock (VALENT_OBJECT (self));
}

/**
 * valent_lan_dnssd_new:
 * @identity: (nullable): a KDE Connect identity packet
 *
 * Create a DNS-SD adapter for @identity.
 *
 * Returns: (transfer full): a `GListModel`
 */
GListModel *
valent_lan_dnssd_new (JsonNode *identity)
{
  g_return_val_if_fail (identity == NULL || VALENT_IS_PACKET (identity), NULL);

  return g_object_new (VALENT_TYPE_LAN_DNSSD,
                       "identity", identity,
                       NULL);
}

/**
 * valent_lan_dnssd_attach:
 * @self: an `ValentLanDNSSD`
 * @context: (nullable): a `GMainContext`
 *
 * Start the DNS-SD adapter in @context.
 *
 * If @context is %NULL, the service will run in the thread-default main
 * context, as returned by [type@GLib.MainContext.ref_thread_default].
 */
void
valent_lan_dnssd_attach (ValentLanDNSSD  *self,
                         GMainContext    *context)
{
  g_return_if_fail (VALENT_IS_LAN_DNSSD (self));

  valent_object_lock (VALENT_OBJECT (self));
  if (context == NULL)
    {
      g_clear_pointer (&self->context, g_main_context_unref);
      self->context = g_main_context_ref_thread_default ();
    }
  else if (self->context != context)
    {
      g_clear_pointer (&self->context, g_main_context_unref);
      self->context = g_main_context_ref (context);
    }

  g_main_context_invoke_full (self->context,
                              G_PRIORITY_DEFAULT,
                              G_SOURCE_FUNC (_avahi_client_connect),
                              g_object_ref (self),
                              g_object_unref);
  valent_object_unlock (VALENT_OBJECT (self));
}
