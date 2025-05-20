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

/*< private >
 * ValentAvahiAddressEnumerator:
 *
 * A [class@Gio.SocketAddressEnumerator] implementation that uses Avahi to
 * resolve candidates.
 */
#define VALENT_TYPE_AVAHI_ADDRESS_ENUMERATOR (valent_avahi_address_enumerator_get_type())
G_DECLARE_FINAL_TYPE (ValentAvahiAddressEnumerator, valent_avahi_address_enumerator, VALENT, AVAHI_ADDRESS_ENUMERATOR, GSocketAddressEnumerator)

struct _ValentAvahiAddressEnumerator
{
  GSocketAddressEnumerator  parent_instance;

  GDBusConnection          *connection;
  GPtrArray                *items;
  unsigned int              position;
};

G_DEFINE_FINAL_TYPE (ValentAvahiAddressEnumerator, valent_avahi_address_enumerator, G_TYPE_SOCKET_ADDRESS_ENUMERATOR)

static void
valent_avahi_address_enumerator_next_cb (GDBusConnection *connection,
                                         GAsyncResult    *result,
                                         gpointer         user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GSocketAddress) ret = NULL;
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
  GError *error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  if (reply == NULL)
    {
      g_dbus_error_strip_remote_error (error);
      g_task_return_error (task, g_steal_pointer (&error));
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

  ret = g_inet_socket_address_new_from_string (address, port);
  if (ret == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to create socket address for %s:%u",
                               address, port);
      return;
    }

  g_task_return_pointer (task, g_steal_pointer (&ret), g_object_unref);
}

static void
valent_avahi_address_enumerator_next_async (GSocketAddressEnumerator *enumerator,
                                            GCancellable             *cancellable,
                                            GAsyncReadyCallback       callback,
                                            gpointer                  user_data)
{
  ValentAvahiAddressEnumerator *self = VALENT_AVAHI_ADDRESS_ENUMERATOR (enumerator);
  g_autoptr (GTask) task = NULL;
  GVariant *parameters = NULL;
  int interface = 0;
  int protocol = 0;
  const char *name = 0;
  const char *type = 0;
  const char *domain = 0;
  uint32_t flags = 0;

  g_assert (VALENT_IS_AVAHI_ADDRESS_ENUMERATOR (self));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_avahi_address_enumerator_next_async);

  if (self->position >= self->items->len)
    {
      g_task_return_pointer (task, NULL, NULL);
      return;
    }

  /* These are the parameters from an ItemNew emission
   */
  parameters = g_ptr_array_index (self->items, self->position++);
  g_variant_get (parameters,
                 "(ii&s&s&su)",
                 &interface,
                 &protocol,
                 &name,
                 &type,
                 &domain,
                 &flags);

  g_dbus_connection_call (self->connection,
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
                          cancellable,
                          (GAsyncReadyCallback)valent_avahi_address_enumerator_next_cb,
                          g_object_ref (task));
}

static void
valent_avahi_address_enumerator_finalize (GObject *object)
{
  ValentAvahiAddressEnumerator *self = VALENT_AVAHI_ADDRESS_ENUMERATOR (object);

  g_clear_object (&self->connection);
  g_clear_pointer (&self->items, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_avahi_address_enumerator_parent_class)->finalize (object);
}

static void
valent_avahi_address_enumerator_class_init (ValentAvahiAddressEnumeratorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GSocketAddressEnumeratorClass *enumerator_class = G_SOCKET_ADDRESS_ENUMERATOR_CLASS (klass);

  object_class->finalize = valent_avahi_address_enumerator_finalize;

  enumerator_class->next_async = valent_avahi_address_enumerator_next_async;
}

static void
valent_avahi_address_enumerator_init (ValentAvahiAddressEnumerator *self)
{
}

/*< private >
 * ValentAvahiConnectable:
 *
 * A [iface@Gio.SocketConnectable] implementation that aggregates the
 * candidates for a service discovered by Avahi.
 */
#define VALENT_TYPE_AVAHI_CONNECTABLE (valent_avahi_connectable_get_type())
G_DECLARE_FINAL_TYPE (ValentAvahiConnectable, valent_avahi_connectable, VALENT, AVAHI_CONNECTABLE, GObject)

struct _ValentAvahiConnectable
{
  GObject          parent_instance;

  GDBusConnection *connection;
  GPtrArray       *items;
  char            *service_name;
};

static void   g_socket_connectable_iface_init (GSocketConnectableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentAvahiConnectable, valent_avahi_connectable, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_SOCKET_CONNECTABLE, g_socket_connectable_iface_init))

static GSocketAddressEnumerator *
valent_avahi_connectable_enumerate (GSocketConnectable *connectable)
{
  ValentAvahiConnectable *self = VALENT_AVAHI_CONNECTABLE (connectable);
  ValentAvahiAddressEnumerator *enumerator = NULL;

  g_assert (VALENT_IS_AVAHI_CONNECTABLE (self));

  enumerator = g_object_new (VALENT_TYPE_AVAHI_ADDRESS_ENUMERATOR, NULL);
  enumerator->connection = g_object_ref (self->connection);
  enumerator->items = g_ptr_array_ref (self->items);

  return G_SOCKET_ADDRESS_ENUMERATOR (enumerator);
}

static char *
valent_avahi_connectable_to_string (GSocketConnectable *connectable)
{
  ValentAvahiConnectable *self = VALENT_AVAHI_CONNECTABLE (connectable);

  g_assert (VALENT_IS_AVAHI_CONNECTABLE (self));

  return g_strdup (self->service_name);
}

static void
g_socket_connectable_iface_init (GSocketConnectableIface *iface)
{
  iface->enumerate = valent_avahi_connectable_enumerate;
  iface->to_string = valent_avahi_connectable_to_string;
}

static void
valent_avahi_connectable_finalize (GObject *object)
{
  ValentAvahiConnectable *self = VALENT_AVAHI_CONNECTABLE (object);

  g_clear_object (&self->connection);
  g_clear_pointer (&self->items, g_ptr_array_unref);
  g_clear_pointer (&self->service_name, g_free);

  G_OBJECT_CLASS (valent_avahi_connectable_parent_class)->finalize (object);
}

static void
valent_avahi_connectable_class_init (ValentAvahiConnectableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_avahi_connectable_finalize;
}

static void
valent_avahi_connectable_init (ValentAvahiConnectable *self)
{
  self->items = g_ptr_array_new_with_free_func ((GDestroyNotify)g_variant_unref);
}

/**
 * ValentLanDNSSD:
 *
 * A simple DNS-SD manager.
 *
 * `ValentLanDNSSD` implements [iface@Gio.ListModel], representing discovered
 * services as [class@Gio.SocketConnectable] objects.
 *
 * If the [property@ValentLanDNSSD:identity] property is set to a KDE Connect
 * identity packet (`kdeconnect.identity`), it will export a service with the
 * type `_kdeconnect._udp`.
 */

struct _ValentLanDNSSD
{
  ValentObject      parent_instance;

  JsonNode         *identity;
  char             *service_type;

  char             *name;
  uint16_t          port;
  GVariant         *txt;

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

  /* list model */
  GPtrArray        *items;
  GHashTable       *pending;
  unsigned int      pending_id;
};

static void   g_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentLanDNSSD, valent_lan_dnssd, VALENT_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

typedef enum {
  PROP_IDENTITY = 1,
  PROP_SERVICE_TYPE,
} ValentLanDNSSDProperty;

static GParamSpec *properties[PROP_SERVICE_TYPE + 1] = { NULL, };


static inline GVariant *
txt_new_str (const char *name,
             const char *value)
{
  g_autofree char *key = g_strdup_printf ("%s=%s", name, value);

  return g_variant_new_bytestring (key);
}

static inline GVariant *
txt_new_uint (const char *name,
              uint32_t    value)
{
  g_autofree char *key = g_strdup_printf ("%s=%u", name, value);

  return g_variant_new_bytestring (key);
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

  g_assert (VALENT_IS_LAN_DNSSD (self));

  if G_UNLIKELY (position >= self->items->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (self->items, position));
}

static GType
valent_lan_dnssd_get_item_type (GListModel *list)
{
  return G_TYPE_SOCKET_CONNECTABLE;
}

static unsigned int
valent_lan_dnssd_get_n_items (GListModel *list)
{
  ValentLanDNSSD *self = VALENT_LAN_DNSSD (list);
  unsigned int ret = 0;

  g_assert (VALENT_IS_LAN_DNSSD (self));

  ret = self->items->len;

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

static void   _avahi_client_connect          (ValentLanDNSSD *self);
static void   _avahi_client_disconnect       (ValentLanDNSSD *self);
static void   _avahi_entry_group_new         (ValentLanDNSSD *self);
static void   _avahi_entry_group_add_service (ValentLanDNSSD *self);
static void   _avahi_entry_group_commit      (ValentLanDNSSD *self);
static void   _avahi_entry_group_reset       (ValentLanDNSSD *self);
static void   _avahi_service_browser_prepare (ValentLanDNSSD *self);


/*
 * Entry Group
 *
 * These functions export a DNS-SD service based on the content of
 * [property@ValentLanDNSSD:identity].
 */
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
}

static void
_avahi_entry_group_get_state_cb (GDBusConnection *connection,
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

  if (self->entry_group_path != NULL)
    {
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
          _avahi_entry_group_add_service (self);
        }
    }
}

static void
_avahi_entry_group_new_cb (GDBusConnection *connection,
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

  if (self->entry_group_path == NULL)
    {
      g_variant_get (reply, "(o)", &self->entry_group_path);
      g_dbus_connection_call (self->connection,
                              AVAHI_DBUS_NAME,
                              self->entry_group_path,
                              AVAHI_ENTRY_GROUP_IFACE,
                              "GetState",
                              NULL,
                              G_VARIANT_TYPE ("(i)"),
                              G_DBUS_CALL_FLAGS_NO_AUTO_START,
                              -1,
                              self->cancellable,
                              (GAsyncReadyCallback)_avahi_entry_group_get_state_cb,
                              self);
    }
}

static void
_avahi_entry_group_new (ValentLanDNSSD *self)
{
  g_assert (VALENT_IS_LAN_DNSSD (self));

  if (self->entry_group_path == NULL)
    {
      g_dbus_connection_call (self->connection,
                              AVAHI_DBUS_NAME,
                              AVAHI_SERVER2_PATH,
                              AVAHI_SERVER2_IFACE,
                              "EntryGroupNew",
                              NULL,
                              G_VARIANT_TYPE ("(o)"),
                              G_DBUS_CALL_FLAGS_NO_AUTO_START,
                              -1,
                              self->cancellable,
                              (GAsyncReadyCallback)_avahi_entry_group_new_cb,
                              self);
    }
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

static void
_avahi_entry_group_add_service (ValentLanDNSSD *self)
{
  if (self->identity == NULL || self->entry_group_path == NULL)
    return;

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
                                             self->service_type,
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
                                             self->service_type,
                                             "", // domain
                                             self->txt),
                              NULL,
                              G_DBUS_CALL_FLAGS_NO_AUTO_START,
                              -1,
                              self->cancellable,
                              NULL,
                              NULL);
    }
}

static void
_avahi_entry_group_commit (ValentLanDNSSD *self)
{
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
}

static void
_avahi_entry_group_reset (ValentLanDNSSD *self)
{
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
}

/*
 * Service Browser
 *
 * These functions aggregate DNS-SD services into [iface@Gio.SocketConnectable]
 * objects for the [iface@Gio.ListModel] implementation.
 */
static inline gboolean
find_service_func (gconstpointer a,
                   gconstpointer b)
{
  ValentAvahiConnectable *connectable = (ValentAvahiConnectable *)a;

  return g_strcmp0 (connectable->service_name, (const char *)b) == 0;
}

static gboolean
pending_cb (gpointer data)
{
  ValentLanDNSSD *self = VALENT_LAN_DNSSD (data);
  GHashTableIter iter;
  const char *name;
  ValentAvahiConnectable *connectable;
  unsigned int position = 0;
  unsigned int added = 0;

  position = self->items->len;

  g_hash_table_iter_init (&iter, self->pending);
  while (g_hash_table_iter_next (&iter, (void **)&name, (void **)&connectable))
    {
      unsigned int pos = 0;

      if (g_ptr_array_find_with_equal_func (self->items,
                                            name,
                                            find_service_func,
                                            &pos))
        {
          ValentAvahiConnectable *current = g_ptr_array_index (self->items, pos);
          g_ptr_array_extend_and_steal (current->items,
                                        g_steal_pointer (&connectable->items));
        }
      else
        {
          g_ptr_array_add (self->items, g_object_ref (connectable));
          added += 1;
        }

      g_hash_table_iter_remove (&iter);
    }

  if (added > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), position, 0, added);

  self->pending_id = 0;
  return G_SOURCE_REMOVE;
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
      ValentAvahiConnectable *connectable = NULL;

      g_variant_get (parameters,
                     "(ii&s&s&su)",
                     &interface,
                     &protocol,
                     &name,
                     &type,
                     &domain,
                     &flags);

      /* Ignore announcements with an invalid service name (i.e. device ID)
       */
      if (!valent_device_validate_id (name))
        {
          g_warning ("%s(): invalid device ID \"%s\"", G_STRFUNC, name);
          return;
        }

      connectable = g_hash_table_lookup (self->pending, name);
      if (connectable == NULL)
        {
          connectable = g_object_new (VALENT_TYPE_AVAHI_CONNECTABLE, NULL);
          connectable->connection = g_object_ref (connection);
          connectable->service_name = g_strdup (name);
          g_hash_table_replace (self->pending, g_strdup (name), connectable);

          if (self->pending_id == 0)
            self->pending_id = g_idle_add (pending_cb, self);
        }

      g_ptr_array_add (connectable->items, g_variant_ref_sink (parameters));
    }
  else if (g_str_equal (signal_name, "ItemRemove"))
    {
      unsigned int position = 0;

      g_variant_get (parameters,
                     "(ii&s&s&su)",
                     &interface,
                     &protocol,
                     &name,
                     &type,
                     &domain,
                     &flags);

      if (g_ptr_array_find_with_equal_func (self->items,
                                            name,
                                            find_service_func,
                                            &position))
        {
          ValentAvahiConnectable *connectable = NULL;

          connectable = g_ptr_array_index (self->items, position);
          for (unsigned int i = 0; i < connectable->items->len; i++)
            {
              GVariant *params = g_ptr_array_index (connectable->items, i);
              int32_t interface_ = 0;
              int32_t protocol_ = 0;
              const char *name_ = 0;
              const char *type_ = 0;
              const char *domain_ = 0;
              uint32_t flags_ = 0;

              g_variant_get (params,
                             "(ii&s&s&su)",
                             &interface_,
                             &protocol_,
                             &name_,
                             &type_,
                             &domain_,
                             &flags_);

              if (interface == interface_ &&
                  protocol == protocol_ &&
                  g_str_equal (domain, domain_))
                {
                  g_ptr_array_remove_index (connectable->items, i);
                  break;
                }
            }

          if (connectable->items->len == 0)
            {
              g_ptr_array_remove_index (self->items, position);
              g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
            }
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

      if (self->service_browser_event_id != 0)
        {
          g_dbus_connection_signal_unsubscribe (connection,
                                                self->service_browser_event_id);
          self->service_browser_event_id = 0;
        }

      g_clear_pointer (&self->service_browser_path, g_free);
    }
}

static void
_avahi_service_browser_prepare_cb (GDBusConnection *connection,
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

static void
_avahi_service_browser_prepare (ValentLanDNSSD *self)
{
  g_assert (VALENT_IS_LAN_DNSSD (self));

  if (self->service_browser_path == NULL)
    {
      g_dbus_connection_call (self->connection,
                              AVAHI_DBUS_NAME,
                              AVAHI_SERVER2_PATH,
                              AVAHI_SERVER2_IFACE,
                              "ServiceBrowserPrepare",
                              g_variant_new ("(iissu)",
                                             -1, // interface: AVAHI_IF_UNSPEC
                                             -1, // protocol: AVAHI_PROTO_UNSPEC
                                             self->service_type,
                                             "", // domain
                                             0), // flags: AvahiLookupFlags
                              G_VARIANT_TYPE ("(o)"),
                              G_DBUS_CALL_FLAGS_NO_AUTO_START,
                              -1,
                              self->cancellable,
                              (GAsyncReadyCallback)_avahi_service_browser_prepare_cb,
                              self);
    }
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

  g_variant_get (parameters, "(i&s)", &self->server_state, &error);

  VALENT_NOTE ("[%i] %s", self->server_state, error);

  switch (self->server_state)
    {
    case _AVAHI_SERVER_INVALID:
    case _AVAHI_SERVER_REGISTERING:
      break;

    case _AVAHI_SERVER_RUNNING:
      _avahi_entry_group_new (self);
      _avahi_service_browser_prepare (self);
      break;

    case _AVAHI_SERVER_COLLISION:
      g_warning ("%s(): DNS-SD server collision: %s", G_STRFUNC, error);
      break;

    case _AVAHI_SERVER_FAILURE:
      g_warning ("%s(): DNS-SD server failure: %s", G_STRFUNC, error);
      break;
    }
}

static void
_avahi_server_get_state_cb (GDBusConnection *connection,
                            GAsyncResult    *result,
                            ValentLanDNSSD  *self)
{
  g_autoptr (GVariant) reply = NULL;
  g_autoptr (GCancellable) destroy = NULL;
  g_autoptr (GError) error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  if (reply == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

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
      _avahi_entry_group_new (self);
      _avahi_service_browser_prepare (self);
    }
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

  /* Create a new cancellable, chained to the object's cancellable, so that
   * any operations will be cancelled if the object is destroyed.
   */
  self->connection = g_object_ref (connection);
  self->cancellable = g_cancellable_new ();
  g_signal_connect_data (self,
                         "destroy",
                         G_CALLBACK (g_cancellable_cancel),
                         g_object_ref (self->cancellable),
                         (GClosureNotify)(void (*) (void))g_object_unref,
                         G_CONNECT_SWAPPED);
  g_dbus_connection_call (self->connection,
                          AVAHI_DBUS_NAME,
                          AVAHI_SERVER2_PATH,
                          AVAHI_SERVER2_IFACE,
                          "GetState",
                          NULL,
                          G_VARIANT_TYPE ("(i)"),
                          G_DBUS_CALL_FLAGS_NO_AUTO_START,
                          -1,
                          self->cancellable,
                          (GAsyncReadyCallback)_avahi_server_get_state_cb,
                          self);
}

static void
on_name_vanished (GDBusConnection *connection,
                  const char      *name,
                  ValentLanDNSSD  *self)
{
  g_assert (VALENT_IS_LAN_DNSSD (self));

  if (self->cancellable != NULL)
    {
      g_signal_handlers_disconnect_by_data (self, self->cancellable);
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

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

      g_clear_handle_id (&self->pending_id, g_source_remove);
      g_clear_pointer (&self->service_browser_path, g_free);
      g_clear_pointer (&self->entry_group_path, g_free);
      g_clear_object (&self->connection);
    }
}

static void
_avahi_client_connect (ValentLanDNSSD *self)
{
  if (self->watcher_id == 0)
    {
      self->watcher_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                           AVAHI_DBUS_NAME,
                                           G_BUS_NAME_WATCHER_FLAGS_NONE,
                                           (GBusNameAppearedCallback)on_name_appeared,
                                           (GBusNameVanishedCallback)on_name_vanished,
                                           self, NULL);
    }
}

static void
_avahi_client_disconnect (ValentLanDNSSD *self)
{
  if (self->cancellable != NULL)
    {
      g_signal_handlers_disconnect_by_data (self, self->cancellable);
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

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
      _avahi_entry_group_reset (self);
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

  /* Service Name and Port
   */
  g_set_str (&self->name, id);

  if (valent_packet_get_int (identity, "tcpPort", &port))
    self->port = (uint16_t)port;

  _avahi_entry_group_add_service (self);
}

/*
 * ValentObject
 */
static void
valent_lan_dnssd_destroy (ValentObject *object)
{
  ValentLanDNSSD *self = VALENT_LAN_DNSSD (object);

  valent_lan_dnssd_stop (self);

  VALENT_OBJECT_CLASS (valent_lan_dnssd_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_lan_dnssd_finalize (GObject *object)
{
  ValentLanDNSSD *self = VALENT_LAN_DNSSD (object);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->txt, g_variant_unref);
  g_clear_pointer (&self->identity, json_node_unref);
  g_clear_pointer (&self->service_type, g_free);
  g_clear_pointer (&self->items, g_ptr_array_unref);
  g_clear_pointer (&self->pending, g_hash_table_unref);
  g_clear_handle_id (&self->pending_id, g_source_remove);

  G_OBJECT_CLASS (valent_lan_dnssd_parent_class)->finalize (object);
}

static void
valent_lan_dnssd_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  ValentLanDNSSD *self = VALENT_LAN_DNSSD (object);

  switch ((ValentLanDNSSDProperty)prop_id)
    {
    case PROP_IDENTITY:
      g_value_set_boxed (value, self->identity);
      break;

    case PROP_SERVICE_TYPE:
      g_value_set_string (value, self->service_type);
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

  switch ((ValentLanDNSSDProperty)prop_id)
    {
    case PROP_IDENTITY:
      valent_lan_dnssd_set_identity (self, g_value_get_boxed (value));
      break;

    case PROP_SERVICE_TYPE:
      self->service_type = g_value_dup_string (value);
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

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_lan_dnssd_init (ValentLanDNSSD *self)
{
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
  self->pending = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         g_object_unref);
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
 * valent_lan_dnssd_start:
 * @self: an `ValentLanDNSSD`
 *
 * Start the DNS-SD adapter.
 */
void
valent_lan_dnssd_start (ValentLanDNSSD *self)
{
  g_return_if_fail (VALENT_IS_LAN_DNSSD (self));

  _avahi_client_connect (self);
}

/**
 * valent_lan_dnssd_stop:
 * @self: an `ValentLanDNSSD`
 *
 * Stop the DNS-SD adapter.
 */
void
valent_lan_dnssd_stop (ValentLanDNSSD *self)
{
  g_return_if_fail (VALENT_IS_LAN_DNSSD (self));

  _avahi_client_disconnect (self);
}
