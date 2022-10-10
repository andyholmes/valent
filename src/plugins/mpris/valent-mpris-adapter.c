// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mpris-adapter"

#include "config.h"

#include <libvalent-core.h>
#include <libvalent-media.h>

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
  ValentMPRISImpl    *export;
  GHashTable         *exports;
};

G_DEFINE_TYPE (ValentMPRISAdapter, valent_mpris_adapter, VALENT_TYPE_MEDIA_ADAPTER)


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

  /* There is at least one player exported, but @player may be %NULL */
  if ((export = g_hash_table_lookup (self->exports, player)) == NULL)
    {
      GHashTableIter iter;

      g_hash_table_iter_init (&iter, self->exports);
      g_hash_table_iter_next (&iter, (void **)&player, (void **)&export);
    }

  /* If the player has stopped, select a different one for export */
  if (!valent_media_player_is_playing (player))
    {
      GHashTableIter iter;

      /* It wasn't the exported player that stopped */
      if (self->export && self->export != export)
        return;

      g_hash_table_iter_init (&iter, self->exports);

      while (g_hash_table_iter_next (&iter, (void **)&player, (void **)&export))
        {
          if (valent_media_player_is_playing (player))
            break;
        }
    }

  /* Nothing to do */
  if (self->export == export)
    return;

  /* Unexport any current player and replace it with the active player */
  g_clear_pointer (&self->export, valent_mpris_impl_unexport);

  if (export != NULL)
    {
      g_autoptr (GError) error = NULL;

      if (valent_mpris_impl_export (export, self->connection, &error))
        {
          self->export = export;

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
valent_mpris_player_new_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  ValentMPRISAdapter *self = VALENT_MPRIS_ADAPTER (user_data);
  g_autoptr (ValentMPRISPlayer) player = NULL;
  g_autofree char *name = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_MPRIS_ADAPTER (self));

  if ((player = valent_mpris_player_new_finish (result, &error)) == NULL)
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

  if (strlen (new_owner) > 0 && !known)
    {
      valent_mpris_player_new (name,
                               NULL,
                               (GAsyncReadyCallback)valent_mpris_player_new_cb,
                               self);
    }
  else if (strlen (old_owner) > 0 && known)
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

static void
list_names_cb (GDBusConnection *connection,
               GAsyncResult    *result,
               gpointer         user_data)
{
  ValentMPRISAdapter *self;
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GVariant) reply = NULL;

  self = g_task_get_source_object (task);

  /* If we succeed, add any currently exported players */
  reply = g_dbus_connection_call_finish (connection, result, NULL);

  if (reply != NULL)
    {
      g_autoptr (GVariant) names = NULL;
      GVariantIter iter;
      const char *name;

      names = g_variant_get_child_value (reply, 0);
      g_variant_iter_init (&iter, names);

      while (g_variant_iter_next (&iter, "&s", &name))
        {
          /* This is the D-Bus name we export on */
          if G_UNLIKELY (g_str_has_prefix (name, VALENT_MPRIS_DBUS_NAME))
            return;

          if G_LIKELY (!g_str_has_prefix (name, "org.mpris.MediaPlayer2"))
            continue;

          valent_mpris_player_new (name,
                                   g_task_get_cancellable (task),
                                   valent_mpris_player_new_cb,
                                   self);
        }
    }

  if (g_task_return_error_if_cancelled (task))
    return;

  /* Watch for new and removed MPRIS Players */
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

  g_task_return_boolean (task, TRUE);
}

static void
valent_mpris_adapter_load_async (ValentMediaAdapter  *adapter,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  ValentMPRISAdapter *self = VALENT_MPRIS_ADAPTER (adapter);
  g_autoptr (GTask) task = NULL;
  GError *error = NULL;

  g_assert (VALENT_IS_MPRIS_ADAPTER (self));

  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mpris_adapter_load_async);

  self->connection = g_bus_get_sync (G_BUS_TYPE_SESSION,
                                     cancellable,
                                     &error);

  if (self->connection == NULL)
    return g_task_return_error (task, error);

  g_dbus_connection_call (self->connection,
                          "org.freedesktop.DBus",
                          "/org/freedesktop/DBus",
                          "org.freedesktop.DBus",
                          "ListNames",
                          NULL,
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cancellable,
                          (GAsyncReadyCallback)list_names_cb,
                          g_steal_pointer (&task));
}

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

  if (valent_in_flatpak ())
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
      g_autofree char *bus_name = NULL;

      bus_name = g_strdup_printf ("%s.Player%u",
                                  VALENT_MPRIS_DBUS_NAME,
                                  n_exports++);
      valent_mpris_impl_export_full (impl,
                                     bus_name,
                                     NULL, // cancellable,
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

  if (valent_in_flatpak ())
    {
      g_signal_handlers_disconnect_by_data (impl, self);

      if (self->export == impl)
        {
          g_clear_pointer (&self->export, valent_mpris_impl_unexport);
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

  adapter_class->load_async = valent_mpris_adapter_load_async;
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

