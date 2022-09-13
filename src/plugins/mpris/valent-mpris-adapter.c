// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mpris-adapter"

#include "config.h"

#include <libvalent-core.h>
#include <libvalent-media.h>

#include "valent-mpris-adapter.h"
#include "valent-mpris-player.h"
#include "valent-mpris-utils.h"


struct _ValentMPRISAdapter
{
  ValentMediaAdapter  parent_instance;

  GDBusConnection    *connection;
  unsigned int        name_owner_changed_id;
  GHashTable         *players;
};

G_DEFINE_TYPE (ValentMPRISAdapter, valent_mpris_adapter, VALENT_TYPE_MEDIA_ADAPTER)


/*
 * Emitted when the properties of a #MPRISPlayer change.
 */
static void
add_player (ValentMPRISAdapter *self,
            ValentMPRISPlayer  *player)
{
  g_autofree char *name = NULL;

  g_object_get (player, "bus-name", &name, NULL);
  g_hash_table_insert (self->players,
                       g_steal_pointer (&name),
                       g_object_ref (player));

  valent_media_adapter_emit_player_added (VALENT_MEDIA_ADAPTER (self),
                                          VALENT_MEDIA_PLAYER (player));
}

static void
remove_player (ValentMPRISAdapter *self,
               const char         *name)
{
  ValentMediaAdapter *adapter = VALENT_MEDIA_ADAPTER (self);
  gpointer key, value;

  g_assert (VALENT_IS_MEDIA_ADAPTER (self));

  if (g_hash_table_steal_extended (self->players, name, &key, &value))
    {
      valent_media_adapter_emit_player_removed (adapter, value);
      g_free (key);
      g_object_unref (value);
    }
}

static void
valent_mpris_player_new_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  ValentMPRISAdapter *self = VALENT_MPRIS_ADAPTER (user_data);
  g_autoptr (ValentMPRISPlayer) player = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_MPRIS_ADAPTER (self));

  if ((player = valent_mpris_player_new_finish (result, &error)) != NULL)
    add_player (self, player);
  else
    g_warning ("Adding MPRIS player: %s", error->message);
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
  gboolean known;
  const char *name;
  const char *old_owner;
  const char *new_owner;

  g_variant_get (parameters, "(&s&s&s)", &name, &old_owner, &new_owner);

  /* This is the DBus name we export on */
  if G_UNLIKELY (g_str_equal (name, VALENT_MPRIS_DBUS_NAME))
    return;

  known = g_hash_table_contains (self->players, name);

  /* An unknown player was added */
  if (strlen (new_owner) > 0 && !known)
    valent_mpris_player_new (name, NULL, valent_mpris_player_new_cb, self);

  /* An known player has exited */
  else if (strlen (old_owner) > 0 && known)
    remove_player (self, name);
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
          /* This is the DBus name we export on */
          if G_UNLIKELY (g_str_equal (name, VALENT_MPRIS_DBUS_NAME))
            continue;

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
valent_mpris_adapter_load_async (ValentMediaAdapter *adapter,
                                         GCancellable              *cancellable,
                                         GAsyncReadyCallback        callback,
                                         gpointer                   user_data)
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

/*
 * GObject
 */
static void
valent_mpris_adapter_finalize (GObject *object)
{
  ValentMPRISAdapter *self = VALENT_MPRIS_ADAPTER (object);

  if (self->name_owner_changed_id > 0)
    {
      g_dbus_connection_signal_unsubscribe (self->connection,
                                            self->name_owner_changed_id);
      self->name_owner_changed_id = 0;
    }

  g_clear_object (&self->connection);
  g_clear_pointer (&self->players, g_hash_table_unref);

  G_OBJECT_CLASS (valent_mpris_adapter_parent_class)->finalize (object);
}

static void
valent_mpris_adapter_class_init (ValentMPRISAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentMediaAdapterClass *adapter_class = VALENT_MEDIA_ADAPTER_CLASS (klass);

  object_class->finalize = valent_mpris_adapter_finalize;

  adapter_class->load_async = valent_mpris_adapter_load_async;
}

static void
valent_mpris_adapter_init (ValentMPRISAdapter *self)
{
  self->players = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, g_object_unref);
}

