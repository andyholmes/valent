// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mpris-adapter"

#include "config.h"

#include <valent.h>

#include "valent-mpris-impl.h"
#include "valent-mpris-player.h"
#include "valent-mpris-utils.h"

#include "valent-mpris-adapter.h"


struct _ValentMPRISAdapter
{
  ValentMediaAdapter  parent_instance;

  GDBusConnection    *connection;
  unsigned int        name_owner_changed_id;
  GHashTable         *players;
  GHashTable         *exports;
};

static void   g_async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentMPRISAdapter, valent_mpris_adapter, VALENT_TYPE_MEDIA_ADAPTER,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, g_async_initable_iface_init))


static unsigned int n_exports = 0;


static void
g_async_initable_new_async_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  ValentMPRISAdapter *self = VALENT_MPRIS_ADAPTER (user_data);
  GAsyncInitable *initable = G_ASYNC_INITABLE (object);
  g_autoptr (GObject) player = NULL;
  g_autofree char *name = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_MPRIS_ADAPTER (self));
  g_assert (G_IS_ASYNC_INITABLE (initable));

  if ((player = g_async_initable_new_finish (initable, result, &error)) == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  g_object_get (player, "bus-name", &name, NULL);

  if (g_hash_table_contains (self->players, name))
    return;

  g_hash_table_replace (self->players,
                        g_steal_pointer (&name),
                        g_object_ref (player));

  valent_media_adapter_player_added (VALENT_MEDIA_ADAPTER (self),
                                     VALENT_MEDIA_PLAYER (player));
}

static void
on_name_owner_changed (GDBusConnection *connection,
                       const char      *sender_name,
                       const char      *object_path,
                       const char      *interface_name,
                       const char      *signal_name,
                       GVariant        *parameters,
                       gpointer         user_data)
{
  ValentMPRISAdapter *self = VALENT_MPRIS_ADAPTER (user_data);
  const char *name;
  const char *old_owner;
  const char *new_owner;
  gboolean known;

  g_variant_get (parameters, "(&s&s&s)", &name, &old_owner, &new_owner);

  /* This is the D-Bus name we export on */
  if G_UNLIKELY (g_str_has_prefix (name, VALENT_MPRIS_DBUS_NAME))
    return;

  known = g_hash_table_contains (self->players, name);

  if (*new_owner != '\0' && !known)
    {
      g_autoptr (GCancellable) destroy = NULL;

      /* Cancel initialization if the adapter is destroyed */
      destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
      g_async_initable_new_async (VALENT_TYPE_MPRIS_PLAYER,
                                  G_PRIORITY_DEFAULT,
                                  destroy,
                                  g_async_initable_new_async_cb,
                                  self,
                                  "source",   self,
                                  "bus-name", name,
                                  NULL);
    }
  else if (*old_owner != '\0' && known)
    {
      ValentMediaAdapter *adapter = VALENT_MEDIA_ADAPTER (self);
      gpointer key, value;

      if (g_hash_table_steal_extended (self->players, name, &key, &value))
        {
          valent_media_adapter_player_removed (adapter, value);
          g_free (key);
          g_object_unref (value);
        }
    }
}

/*
 * GAsyncInitable
 */
static void
list_names_cb (GDBusConnection *connection,
               GAsyncResult    *result,
               gpointer         user_data)
{
  ValentMPRISAdapter *self;
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GVariant) reply = NULL;

  self = g_task_get_source_object (task);

  /* If successful, add any currently exported players */
  reply = g_dbus_connection_call_finish (connection, result, NULL);

  if (reply != NULL)
    {
      g_autoptr (GCancellable) destroy = NULL;
      g_autoptr (GVariant) names = NULL;
      GVariantIter iter;
      const char *name;

      destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
      names = g_variant_get_child_value (reply, 0);
      g_variant_iter_init (&iter, names);

      while (g_variant_iter_next (&iter, "&s", &name))
        {
          if G_LIKELY (!g_str_has_prefix (name, "org.mpris.MediaPlayer2"))
            continue;

          /* This is the D-Bus name we export on */
          if G_UNLIKELY (g_str_has_prefix (name, VALENT_MPRIS_DBUS_NAME))
            continue;

          g_async_initable_new_async (VALENT_TYPE_MPRIS_PLAYER,
                                      G_PRIORITY_DEFAULT,
                                      destroy,
                                      g_async_initable_new_async_cb,
                                      self,
                                      "bus-name", name,
                                      NULL);
        }
    }

  if (g_task_return_error_if_cancelled (task))
    return;

  /* Regardless of the result of `ListNames()`, the connection is valid */
  self->name_owner_changed_id =
    g_dbus_connection_signal_subscribe (connection,
                                        "org.freedesktop.DBus",
                                        "org.freedesktop.DBus",
                                        "NameOwnerChanged",
                                        "/org/freedesktop/DBus",
                                        "org.mpris.MediaPlayer2",
                                        G_DBUS_SIGNAL_FLAGS_MATCH_ARG0_NAMESPACE,
                                        on_name_owner_changed,
                                        self, NULL);


  /* Report the adapter as active */
  valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                         VALENT_PLUGIN_STATE_ACTIVE,
                                         NULL);
  g_task_return_boolean (task, TRUE);
}

static void
valent_mpris_adapter_init_async (GAsyncInitable      *initable,
                                 int                  io_priority,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  ValentMPRISAdapter *self = VALENT_MPRIS_ADAPTER (initable);
  g_autoptr (GTask) task = NULL;
  g_autoptr (GCancellable) destroy = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_MPRIS_ADAPTER (self));

  /* Cede the primary position until complete */
  valent_extension_plugin_state_changed (VALENT_EXTENSION (initable),
                                         VALENT_PLUGIN_STATE_INACTIVE,
                                         NULL);

  /* Cancel initialization if the object is destroyed */
  destroy = valent_object_chain_cancellable (VALENT_OBJECT (initable),
                                             cancellable);

  task = g_task_new (initable, destroy, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, valent_mpris_adapter_init_async);

  self->connection = g_bus_get_sync (G_BUS_TYPE_SESSION,
                                     destroy,
                                     &error);

  if (self->connection == NULL)
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ERROR,
                                             error);
      return g_task_return_error (task, g_steal_pointer (&error));
    }

  g_dbus_connection_call (self->connection,
                          "org.freedesktop.DBus",
                          "/org/freedesktop/DBus",
                          "org.freedesktop.DBus",
                          "ListNames",
                          NULL,
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          destroy,
                          (GAsyncReadyCallback)list_names_cb,
                          g_steal_pointer (&task));
}

static void
g_async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = valent_mpris_adapter_init_async;
}

/*
 * ValentMediaAdapter
 */
static void
valent_mpris_impl_export_full_cb (ValentMPRISImpl    *impl,
                                  GAsyncResult       *result,
                                  ValentMPRISAdapter *self)
{
  g_autoptr (GError) error = NULL;

  if (!valent_mpris_impl_export_finish (impl, result, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("%s(): %s", G_STRFUNC, error->message);
}

static void
valent_mpris_adapter_export_player (ValentMediaAdapter *adapter,
                                    ValentMediaPlayer  *player)
{
  ValentMPRISAdapter *self = VALENT_MPRIS_ADAPTER (adapter);
  g_autoptr (ValentMPRISImpl) impl = NULL;
  g_autoptr (GCancellable) destroy = NULL;
  g_autofree char *bus_name = NULL;

  if (g_hash_table_contains (self->exports, player))
    return;

  impl = valent_mpris_impl_new (player);
  g_hash_table_insert (self->exports, player, g_object_ref (impl));

  bus_name = g_strdup_printf ("%s.Player%u",
                              VALENT_MPRIS_DBUS_NAME,
                              n_exports++);
  destroy = valent_object_ref_cancellable (VALENT_OBJECT (adapter));
  valent_mpris_impl_export_full (impl,
                                 bus_name,
                                 destroy,
                                 (GAsyncReadyCallback)valent_mpris_impl_export_full_cb,
                                 self);
}

static void
valent_mpris_adapter_unexport_player (ValentMediaAdapter *adapter,
                                      ValentMediaPlayer  *player)
{
  ValentMPRISAdapter *self = VALENT_MPRIS_ADAPTER (adapter);
  g_autoptr (ValentMPRISImpl) impl = NULL;

  g_assert (VALENT_IS_MPRIS_ADAPTER (self));
  g_assert (VALENT_IS_MEDIA_PLAYER (player));

  if (!g_hash_table_steal_extended (self->exports, player, NULL, (void **)&impl))
    return;

  g_signal_handlers_disconnect_by_data (impl, self);
  valent_mpris_impl_unexport (impl);
}

/*
 * ValentObject
 */
static void
valent_mpris_adapter_destroy (ValentObject *object)
{
  ValentMPRISAdapter *self = VALENT_MPRIS_ADAPTER (object);
  GHashTableIter iter;
  ValentMPRISImpl *impl;

  if (self->name_owner_changed_id > 0)
    {
      g_dbus_connection_signal_unsubscribe (self->connection,
                                            self->name_owner_changed_id);
      self->name_owner_changed_id = 0;
    }

  g_hash_table_iter_init (&iter, self->exports);

  while (g_hash_table_iter_next (&iter, NULL, (void **)&impl))
    {
      g_signal_handlers_disconnect_by_data (impl, self);
      valent_mpris_impl_unexport (impl);
      g_hash_table_iter_remove (&iter);
    }

  VALENT_OBJECT_CLASS (valent_mpris_adapter_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_mpris_adapter_finalize (GObject *object)
{
  ValentMPRISAdapter *self = VALENT_MPRIS_ADAPTER (object);

  g_clear_object (&self->connection);
  g_clear_pointer (&self->players, g_hash_table_unref);
  g_clear_pointer (&self->exports, g_hash_table_unref);

  G_OBJECT_CLASS (valent_mpris_adapter_parent_class)->finalize (object);
}

static void
valent_mpris_adapter_class_init (ValentMPRISAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentMediaAdapterClass *adapter_class = VALENT_MEDIA_ADAPTER_CLASS (klass);

  object_class->finalize = valent_mpris_adapter_finalize;

  vobject_class->destroy = valent_mpris_adapter_destroy;

  adapter_class->export_player = valent_mpris_adapter_export_player;
  adapter_class->unexport_player = valent_mpris_adapter_unexport_player;
}

static void
valent_mpris_adapter_init (ValentMPRISAdapter *self)
{
  self->exports = g_hash_table_new_full (NULL, NULL,
                                         NULL, g_object_unref);
  self->players = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, g_object_unref);
}

