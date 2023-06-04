// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mpris-adapter"

#include "config.h"

#include <libportal/portal.h>
#include <valent.h>

#include "valent-mpris-adapter.h"
#include "valent-mpris-impl.h"
#include "valent-mpris-player.h"
#include "valent-mpris-utils.h"


struct _ValentMPRISAdapter
{
  ValentMediaAdapter  parent_instance;

  GDBusConnection    *connection;
  unsigned int        name_owner_changed_id;
  GHashTable         *players;

  /* Exports */
  ValentMPRISImpl    *active_export;
  GHashTable         *exports;
};

static void   g_async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentMPRISAdapter, valent_mpris_adapter, VALENT_TYPE_MEDIA_ADAPTER,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, g_async_initable_iface_init))


static unsigned int n_exports = 0;

static void
on_player_state_changed (ValentMPRISAdapter *self,
                         GParamSpec         *pspec,
                         ValentMediaPlayer  *player)
{
  ValentMPRISImpl *export = NULL;

  g_assert (VALENT_IS_MPRIS_ADAPTER (self));
  g_assert (player == NULL || VALENT_IS_MEDIA_PLAYER (player));

  if (g_hash_table_size (self->exports) == 0)
    return;

  /* There is at least one export, but @player may be %NULL */
  if (player == NULL)
    {
      GHashTableIter iter;

      g_hash_table_iter_init (&iter, self->exports);
      g_hash_table_iter_next (&iter, (void **)&player, (void **)&export);
    }
  else
    {
      export = g_hash_table_lookup (self->exports, player);

      /* Bail: it wasn't the exported player that stopped */
      if (self->active_export && self->active_export != export)
        return;
    }

  /* If the player has stopped, look for another one that is active */
  if (valent_media_player_get_state (player) != VALENT_MEDIA_STATE_PLAYING)
    {
      GHashTableIter iter;
      gpointer key, value;

      g_hash_table_iter_init (&iter, self->exports);

      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          if (valent_media_player_get_state (key) != VALENT_MEDIA_STATE_PLAYING)
            continue;

          player = key;
          export = value;
          break;
        }
    }

  g_assert (VALENT_IS_MPRIS_IMPL (export));

  /* Unexport any current player and replace it with the active player */
  if (self->active_export != export)
    {
      g_autoptr (GError) error = NULL;

      g_clear_pointer (&self->active_export, valent_mpris_impl_unexport);

      if (valent_mpris_impl_export (export, self->connection, &error))
        {
          self->active_export = export;

          g_object_freeze_notify (G_OBJECT (player));
          g_object_notify (G_OBJECT (player), "flags");
          g_object_notify (G_OBJECT (player), "metadata");
          g_object_notify (G_OBJECT (player), "name");
          g_object_notify (G_OBJECT (player), "repeat");
          g_object_notify (G_OBJECT (player), "shuffle");
          g_object_notify (G_OBJECT (player), "state");
          g_object_notify (G_OBJECT (player), "volume");
          g_object_thaw_notify (G_OBJECT (player));
        }
      else
        {
          g_warning ("%s(): %s", G_STRFUNC, error->message);
        }
    }
}

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
  destroy = valent_object_attach_cancellable (VALENT_OBJECT (initable),
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
export_full_cb (ValentMPRISImpl    *impl,
                GAsyncResult       *result,
                ValentMPRISAdapter *self)
{
  g_autoptr (GError) error = NULL;

  if (!valent_mpris_impl_export_finish (impl, result, &error))
    g_warning ("%s(): %s", G_STRFUNC, error->message);
}

static void
valent_mpris_adapter_export (ValentMediaAdapter *adapter,
                             ValentMediaPlayer  *player)
{
  ValentMPRISAdapter *self = VALENT_MPRIS_ADAPTER (adapter);
  g_autoptr (ValentMPRISImpl) impl = NULL;

  if (g_hash_table_contains (self->exports, player))
    return;

  impl = valent_mpris_impl_new (player);
  g_hash_table_replace (self->exports, player, g_object_ref (impl));

  /* If running in a sandbox, assume only one D-Bus name may be owned and watch
   * the player to see if it should be prioritized for export */
  if (xdp_portal_running_under_sandbox ())
    {
      g_signal_connect_object (player,
                               "notify::state",
                               G_CALLBACK (on_player_state_changed),
                               self,
                               G_CONNECT_SWAPPED);
      on_player_state_changed (self, NULL, player);
    }
  else
    {
      g_autoptr (GCancellable) destroy = NULL;
      g_autofree char *bus_name = NULL;

      /* Cancel export if the object is destroyed */
      destroy = valent_object_ref_cancellable (VALENT_OBJECT (adapter));

      bus_name = g_strdup_printf ("%s.Player%u",
                                  VALENT_MPRIS_DBUS_NAME,
                                  n_exports++);
      valent_mpris_impl_export_full (impl,
                                     bus_name,
                                     destroy,
                                     (GAsyncReadyCallback)export_full_cb,
                                     self);
    }
}

static void
valent_mpris_adapter_unexport (ValentMediaAdapter *adapter,
                               ValentMediaPlayer  *player)
{
  ValentMPRISAdapter *self = VALENT_MPRIS_ADAPTER (adapter);
  g_autoptr (ValentMPRISImpl) impl = NULL;

  g_assert (VALENT_IS_MPRIS_ADAPTER (self));
  g_assert (VALENT_IS_MEDIA_PLAYER (player));

  if (!g_hash_table_steal_extended (self->exports, player, NULL, (void **)&impl))
    return;

  /* If running in a sandbox, stop watching the player and ensure export
   * priority is relinquished */
  if (xdp_portal_running_under_sandbox ())
    {
      g_signal_handlers_disconnect_by_data (impl, self);

      if (self->active_export == impl)
        {
          g_clear_pointer (&self->active_export, valent_mpris_impl_unexport);
          on_player_state_changed (self, NULL, NULL);
        }
    }
  else
    {
      g_signal_handlers_disconnect_by_data (impl, self);
      valent_mpris_impl_unexport (impl);
    }
}

/*
 * GObject
 */
static void
valent_mpris_adapter_dispose (GObject *object)
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

  G_OBJECT_CLASS (valent_mpris_adapter_parent_class)->dispose (object);
}

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
  ValentMediaAdapterClass *adapter_class = VALENT_MEDIA_ADAPTER_CLASS (klass);

  object_class->dispose = valent_mpris_adapter_dispose;
  object_class->finalize = valent_mpris_adapter_finalize;

  adapter_class->export_player = valent_mpris_adapter_export;
  adapter_class->unexport_player = valent_mpris_adapter_unexport;
}

static void
valent_mpris_adapter_init (ValentMPRISAdapter *self)
{
  self->exports = g_hash_table_new_full (NULL, NULL,
                                         NULL, g_object_unref);
  self->players = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, g_object_unref);
}

