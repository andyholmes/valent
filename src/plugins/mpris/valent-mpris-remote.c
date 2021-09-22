// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mpris-remote"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-media.h>

#include "valent-mpris-common.h"
#include "valent-mpris-remote.h"


struct _ValentMprisRemote
{
  ValentMediaPlayer     parent_instance;

  GDBusConnection      *connection;
  char                 *bus_name;
  unsigned int          bus_name_id;

  unsigned int          flush_id;
  GHashTable           *cache;
  GHashTable           *player_buffer;

  /* org.mpris.MediaPlayer2 */
  unsigned int          application_id;
  GDBusInterfaceVTable  application_vtable;
  char                 *identity;
  unsigned int          fullscreen : 1;
  unsigned int          can_fullscreen : 1;
  unsigned int          can_quit : 1;
  unsigned int          can_raise : 1;
  unsigned int          has_tracklist : 1;

  /* org.mpris.MediaPlayer2.Player */
  unsigned int          player_id;
  GDBusInterfaceVTable  player_vtable;
  ValentMediaActions    flags;
  ValentMediaState      state;
  char                 *loop_status;
  GVariant             *metadata;
  gint64                position;
  double                volume;
};

static void     valent_mpris_remote_flush     (ValentMprisRemote  *self);
static gboolean valent_mpris_remote_register  (ValentMprisRemote  *self,
                                               GError            **error);
static void     valent_mpris_remote_set_value (ValentMprisRemote  *self,
                                               const char         *name,
                                               GVariant           *value);

G_DEFINE_TYPE (ValentMprisRemote, valent_mpris_remote, VALENT_TYPE_MEDIA_PLAYER)


enum {
  METHOD_CALL,
  SET_PROPERTY,
  N_SIGNALS
};

static guint signals [N_SIGNALS] = { 0, };


/*
 * Auto-Export
 */
static ValentMprisRemote *mpris_active = NULL;
static GDBusConnection *mpris_connection = NULL;
static GHashTable *mpris_exports = NULL;


static inline void
valent_mpris_remote_auto_export_init (void)
{
  static gsize guard = 0;

  if (g_once_init_enter (&guard))
    {
      mpris_connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
      mpris_exports = g_hash_table_new (NULL, NULL);

      g_once_init_leave (&guard, 1);
    }
}

static inline void
valent_media_remote_auto_export_check (ValentMprisRemote *remote)
{
  unsigned int n_exports = g_hash_table_size (mpris_exports);

  /* Nothing to do */
  if (n_exports == 0)
    return;

  /* Ensure we have a remote */
  if (remote == NULL)
    {
      GHashTableIter iter;

      g_hash_table_iter_init (&iter, mpris_exports);
      g_hash_table_iter_next (&iter, (void **)&remote, NULL);
    }

  /* If the exported remote stopped, maybe export a different one */
  if (!valent_media_player_is_playing (VALENT_MEDIA_PLAYER (remote)))
    {
      GHashTableIter iter;
      gpointer player;

      /* It wasn't the exported remote that stopped */
      if (mpris_active && mpris_active != remote)
        return;

      /* Look for a replacement */
      g_hash_table_iter_init (&iter, mpris_exports);

      while (g_hash_table_iter_next (&iter, &player, NULL))
        {
          if (valent_media_player_is_playing (player))
            {
              remote = player;
              break;
            }
        }
    }

  /* Nothing to do */
  if (mpris_active == remote)
    return;

  /* Temporarily untrack the remote to really unexport */
  if (mpris_active)
    {
      ValentMprisRemote *unexport = mpris_active;

      g_hash_table_remove (mpris_exports, unexport);

      valent_mpris_remote_flush (mpris_active);
      g_clear_pointer (&mpris_active, valent_mpris_remote_unexport);

      g_hash_table_add (mpris_exports, unexport);
    }

  /* Looping on failure is risky, so just wait for the next state change */
  if (remote)
    {
      g_autoptr (GError) error = NULL;

      if (valent_mpris_remote_register (remote, &error))
        mpris_active = remote;
      else
        g_warning ("%s: %s", G_STRFUNC, error->message);
    }
}

static inline gboolean
valent_mpris_remote_auto_export (ValentMprisRemote *remote)
{
  if (g_strcmp0 (remote->bus_name, VALENT_MPRIS_DBUS_NAME) != 0)
    return FALSE;

  if (!g_hash_table_add (mpris_exports, remote))
    return FALSE;

  /* Set the connection and watch for changes */
  g_set_object (&remote->connection, mpris_connection);
  g_signal_connect (remote,
                    "notify::state",
                    G_CALLBACK (valent_media_remote_auto_export_check),
                    mpris_connection);
  valent_media_remote_auto_export_check (remote);

  return TRUE;
}

static inline gboolean
valent_mpris_remote_auto_unexport (ValentMprisRemote *remote)
{
  if (g_strcmp0 (remote->bus_name, VALENT_MPRIS_DBUS_NAME) != 0)
    return FALSE;

  if (!g_hash_table_remove (mpris_exports, remote))
    return FALSE;

  /* Unexport, stop watching for changes and drop the connection */
  if (mpris_active == remote)
    {
      valent_mpris_remote_flush (mpris_active);
      g_clear_pointer (&mpris_active, valent_mpris_remote_unexport);

      valent_media_remote_auto_export_check (NULL);
    }

  g_signal_handlers_disconnect_by_data (remote, mpris_connection);
  g_clear_object (&remote->connection);

  return TRUE;
}


/*
 * org.mpris.MediaPlayer2 VTable
 */
static void
application_method_call (GDBusConnection       *connection,
                         const char            *sender,
                         const char            *object_path,
                         const char            *interface_name,
                         const char            *method_name,
                         GVariant              *parameters,
                         GDBusMethodInvocation *invocation,
                         gpointer               user_data)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (user_data);

  g_signal_emit (G_OBJECT (self),
                 signals [METHOD_CALL], 0,
                 method_name, parameters);
  g_dbus_method_invocation_return_value (invocation, NULL);
}

static GVariant *
application_get_property (GDBusConnection  *connection,
                          const char       *sender,
                          const char       *object_path,
                          const char       *interface_name,
                          const char       *property_name,
                          GError          **error,
                          gpointer          user_data)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (user_data);
  GVariant *value;

  if ((value = g_hash_table_lookup (self->cache, property_name)) != NULL)
    return g_variant_ref_sink (value);

  if (g_strcmp0 (property_name, "Identity") == 0)
    {
      value = g_variant_new_string (self->identity);
      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "CanQuit") == 0)
    {
      value = g_variant_new_boolean (self->can_quit);
      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "Fullscreen") == 0)
    {
      value = g_variant_new_boolean (self->fullscreen);
      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "CanSetFullscreen") == 0)
    {
      value = g_variant_new_boolean (self->can_fullscreen);
      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "CanRaise") == 0)
    {
      value = g_variant_new_boolean (self->can_raise);
      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "HasTrackList") == 0)
    {
      value = g_variant_new_boolean (self->has_tracklist);
      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "DesktopEntry") == 0)
    {
      value = g_variant_new_string (APPLICATION_ID".desktop");
      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "SupportedUriSchemes") == 0)
    {
      value = g_variant_new_strv (NULL, 0);
      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "SupportedMimeTypes") == 0)
    {
      value = g_variant_new_strv (NULL, 0);
      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  g_set_error (error,
               G_DBUS_ERROR,
               G_DBUS_ERROR_UNKNOWN_PROPERTY,
               "Unknown property \"%s\"", property_name);

  return NULL;
}

static gboolean
application_set_property (GDBusConnection  *connection,
                          const char       *sender,
                          const char       *object_path,
                          const char       *interface_name,
                          const char       *property_name,
                          GVariant         *value,
                          GError          **error,
                          gpointer          user_data)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (user_data);

  if (g_strcmp0 (property_name, "Fullscreen") == 0)
    {
      if (self->fullscreen == g_variant_get_boolean (value))
        return TRUE;
    }

  g_signal_emit (G_OBJECT (self), signals [SET_PROPERTY], 0, property_name, value);

  return TRUE;
}

/*
 * org.mpris.MediaPlayer2.Player VTable
 */
static void
player_method_call (GDBusConnection       *connection,
                    const char            *sender,
                    const char            *object_path,
                    const char            *interface_name,
                    const char            *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (user_data);

  g_signal_emit (G_OBJECT (self),
                 signals [METHOD_CALL], 0,
                 method_name, parameters);
  g_dbus_method_invocation_return_value (invocation, NULL);
}

static GVariant *
player_get_property (GDBusConnection  *connection,
                     const char       *sender,
                     const char       *object_path,
                     const char       *interface_name,
                     const char       *property_name,
                     GError          **error,
                     gpointer          user_data)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (user_data);
  GVariant *value;

  /* Check cache */
  if ((value = g_hash_table_lookup (self->cache, property_name)) != NULL)
    return g_variant_ref (value);

  /* Common properties */
  if (g_strcmp0 (property_name, "CanControl") == 0)
    {
      value = g_variant_new_boolean (TRUE);
      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "CanGoNext") == 0)
    {
      value = g_variant_new_boolean ((self->flags & VALENT_MEDIA_ACTION_NEXT) != 0);
      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "CanGoPrevious") == 0)
    {
      value = g_variant_new_boolean ((self->flags & VALENT_MEDIA_ACTION_PREVIOUS) != 0);
      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "CanPlay") == 0)
    {
      value = g_variant_new_boolean ((self->flags & VALENT_MEDIA_ACTION_PLAY) != 0);
      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "CanPause") == 0)
    {
      value = g_variant_new_boolean ((self->flags & VALENT_MEDIA_ACTION_PAUSE) != 0);
      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "CanSeek") == 0)
    {
      value = g_variant_new_boolean ((self->flags & VALENT_MEDIA_ACTION_SEEK) != 0);
      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "Metadata") == 0)
    {
      if (self->metadata != NULL)
        return g_variant_ref (self->metadata);

      return NULL;
    }

  if (g_strcmp0 (property_name, "Position") == 0)
    return g_variant_new_int64 (self->position);

  if (g_strcmp0 (property_name, "Volume") == 0)
    {
      value = g_variant_new_double (self->volume);
      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }


  /* Uncommon properties */
  if (g_strcmp0 (property_name, "LoopStatus") == 0)
    {
      if (self->state & VALENT_MEDIA_STATE_REPEAT_ALL)
        value = g_variant_new_string ("Playlist");
      else if (self->state & VALENT_MEDIA_STATE_REPEAT)
        value = g_variant_new_string ("Track");
      else
        value = g_variant_new_string ("None");

      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "PlaybackStatus") == 0)
    {
      if ((self->state & VALENT_MEDIA_STATE_PAUSED) != 0)
        value = g_variant_new_string ("Paused");
      if ((self->state & VALENT_MEDIA_STATE_PLAYING) != 0)
        value = g_variant_new_string ("Playing");
      else
        value = g_variant_new_string ("Stopped");

      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "Shuffle") == 0)
    {
      value = g_variant_new_boolean ((self->state & VALENT_MEDIA_STATE_SHUFFLE) != 0);
      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  g_set_error (error,
               G_DBUS_ERROR,
               G_DBUS_ERROR_UNKNOWN_PROPERTY,
               "Unknown property \"%s\"", property_name);

  return NULL;
}

static gboolean
player_set_property (GDBusConnection  *connection,
                     const char       *sender,
                     const char       *object_path,
                     const char       *interface_name,
                     const char       *property_name,
                     GVariant         *value,
                     GError          **error,
                     gpointer          user_data)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (user_data);

  if (g_strcmp0 (property_name, "LoopStatus") == 0)
    {
      const char *loop_status = g_variant_get_string (value, NULL);

      if ((self->state & VALENT_MEDIA_STATE_REPEAT) != 0)
        {
          if (g_strcmp0 (loop_status, "Track") == 0)
            return TRUE;
        }
      else if ((self->state & VALENT_MEDIA_STATE_REPEAT_ALL) != 0)
        {
          if (g_strcmp0 (loop_status, "Playlist") == 0)
            return TRUE;
        }
      else
        {
          if (g_strcmp0 (loop_status, "None") == 0)
            return TRUE;
        }
    }

  else if (g_strcmp0 (property_name, "Rate") == 0)
    {
      return TRUE;
    }

  else if (g_strcmp0 (property_name, "Shuffle") == 0)
    {
      gboolean shuffle = g_variant_get_boolean (value);

      if (((self->state & VALENT_MEDIA_STATE_SHUFFLE) != 0) == shuffle)
        return TRUE;
    }

  else if (g_strcmp0 (property_name, "Volume") == 0)
    {
      double volume = g_variant_get_double (value);

      if (self->volume == volume)
        return TRUE;
    }

  g_signal_emit (G_OBJECT (self), signals [SET_PROPERTY], 0, property_name, value);

  return TRUE;
}

/**
 * valent_mpris_remote_flush:
 * @self: a #ValentMprisRemote
 *
 * Flush player properties.
 */
static void
valent_mpris_remote_flush (ValentMprisRemote *self)
{
  g_autoptr (GError) error = NULL;
  GVariant *properties;
  GVariantBuilder changed_props;
  GVariantBuilder invalidated_props;
  GHashTableIter iter;
  GVariant *value;
  char *prop_name;

  if (self->bus_name_id > 0)
    {
      g_variant_builder_init (&changed_props, G_VARIANT_TYPE_VARDICT);
      g_variant_builder_init (&invalidated_props, G_VARIANT_TYPE_STRING_ARRAY);

      g_hash_table_iter_init (&iter, self->player_buffer);

      while (g_hash_table_iter_next (&iter, (void**)&prop_name, (void**)&value))
        {
          if (value)
            g_variant_builder_add (&changed_props, "{sv}", prop_name, value);
          else
            g_variant_builder_add (&invalidated_props, "s", prop_name);

          g_hash_table_iter_remove (&iter);
        }

      properties = g_variant_new ("(s@a{sv}@as)",
                                  "org.mpris.MediaPlayer2.Player",
                                  g_variant_builder_end (&changed_props),
                                  g_variant_builder_end (&invalidated_props));

      g_dbus_connection_emit_signal (self->connection,
                                     NULL,
                                     "/org/mpris/MediaPlayer2",
                                     "org.freedesktop.DBus.Properties",
                                     "PropertiesChanged",
                                     properties,
                                     &error);

      if (error != NULL)
        g_warning ("%s: %s", G_STRFUNC, error->message);
    }

  g_clear_handle_id (&self->flush_id, g_source_remove);
}

static gboolean
valent_mpris_remote_flush_idle (gpointer data)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (data);

  valent_mpris_remote_flush (self);

  return G_SOURCE_REMOVE;
}

static void
valent_mpris_remote_set_value (ValentMprisRemote *self,
                               const char        *name,
                               GVariant          *value)
{
  g_hash_table_replace (self->cache,
                        g_strdup (name),
                        g_variant_ref_sink (value));

  g_hash_table_replace (self->player_buffer,
                        g_strdup (name),
                        g_variant_ref_sink (value));

  if (self->flush_id == 0)
    self->flush_id = g_idle_add (valent_mpris_remote_flush_idle, self);
}

static gboolean
valent_mpris_remote_register (ValentMprisRemote  *self,
                              GError            **error)
{
  g_assert (VALENT_IS_MPRIS_REMOTE (self));
  g_assert (error == NULL || *error == NULL);
  g_assert (G_IS_DBUS_CONNECTION (self->connection));

  /* Register org.mpris.MediaPlayer2 interface */
  if (self->application_id == 0)
    {
      self->application_id =
        g_dbus_connection_register_object (self->connection,
                                           "/org/mpris/MediaPlayer2",
                                           valent_mpris_get_application_iface (),
                                           &self->application_vtable,
                                           self, NULL,
                                           error);

      if (self->application_id == 0)
        {
          valent_mpris_remote_unexport (self);
          return FALSE;
        }
    }

  /* Register org.mpris.MediaPlayer2.Player interface */
  if (self->player_id == 0)
    {
      self->player_id =
        g_dbus_connection_register_object (self->connection,
                                           "/org/mpris/MediaPlayer2",
                                           valent_mpris_get_player_iface (),
                                           &self->player_vtable,
                                           self, NULL,
                                           error);

      if (self->player_id == 0)
        {
          valent_mpris_remote_unexport (self);
          return FALSE;
        }
    }

  /* Own a well-known name on the connection */
  if (self->bus_name_id == 0)
    {
      self->bus_name_id =
        g_bus_own_name_on_connection (self->connection,
                                      self->bus_name,
                                      G_BUS_NAME_OWNER_FLAGS_NONE,
                                      NULL, // NameAcquired
                                      NULL, // NameLost
                                      NULL,
                                      NULL);
    }

  return TRUE;
}


/*
 * ValentMediaPlayer
 */
static ValentMediaActions
valent_mpris_remote_get_flags (ValentMediaPlayer *player)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (player);

  return self->flags;
}

static GVariant *
valent_mpris_remote_get_metadata (ValentMediaPlayer *player)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (player);

  if (self->metadata)
    return g_variant_ref (self->metadata);

  return NULL;
}

static const char *
valent_mpris_remote_get_name (ValentMediaPlayer *player)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (player);

  return self->identity;
}

static gint64
valent_mpris_remote_get_position (ValentMediaPlayer *player)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (player);

  return self->position;
}

static ValentMediaState
valent_mpris_remote_get_state (ValentMediaPlayer *player)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (player);

  return self->state;
}

static double
valent_mpris_remote_get_volume (ValentMediaPlayer *player)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (player);

  return self->volume;
}

static void
valent_mpris_remote_set_volume (ValentMediaPlayer *player,
                                double             volume)
{
  g_autoptr (GVariant) value = NULL;

  value = g_variant_ref_sink (g_variant_new ("(d)", volume));
  g_signal_emit (G_OBJECT (player), signals [SET_PROPERTY], 0, "Volume", value);
}

static void
valent_mpris_remote_next (ValentMediaPlayer *player)
{
  g_signal_emit (G_OBJECT (player), signals [METHOD_CALL], 0, "Next", NULL);
}

static void
valent_mpris_remote_open_uri (ValentMediaPlayer *player,
                              const char        *uri)
{
  g_autoptr (GVariant) value = NULL;

  value = g_variant_ref_sink (g_variant_new ("(s)", uri));
  g_signal_emit (G_OBJECT (player), signals [METHOD_CALL], 0, "OpenUri", value);
}

static void
valent_mpris_remote_pause (ValentMediaPlayer *player)
{
  g_signal_emit (G_OBJECT (player), signals [METHOD_CALL], 0, "Pause", NULL);
}

static void
valent_mpris_remote_play (ValentMediaPlayer *player)
{
  g_signal_emit (G_OBJECT (player), signals [METHOD_CALL], 0, "Play", NULL);
}

static void
valent_mpris_remote_previous (ValentMediaPlayer *player)
{
  g_signal_emit (G_OBJECT (player), signals [METHOD_CALL], 0, "Previous", NULL);
}

static void
valent_mpris_remote_seek (ValentMediaPlayer *player,
                          gint64             offset)
{
  g_autoptr (GVariant) value = NULL;

  value = g_variant_ref_sink (g_variant_new ("(x)", offset));
  g_signal_emit (G_OBJECT (player), signals [METHOD_CALL], 0, "Seek", value);
}

static void
valent_mpris_remote_stop (ValentMediaPlayer *player)
{
  g_signal_emit (G_OBJECT (player), signals [METHOD_CALL], 0, "Stop", NULL);
}


/*
 * GObject
 */
static void
valent_mpris_remote_dispose (GObject *object)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (object);

  valent_mpris_remote_unexport (self);

  G_OBJECT_CLASS (valent_mpris_remote_parent_class)->dispose (object);
}

static void
valent_mpris_remote_finalize (GObject *object)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (object);

  g_clear_pointer (&self->bus_name, g_free);
  g_clear_object (&self->connection);

  g_clear_pointer (&self->identity, g_free);
  g_clear_pointer (&self->loop_status, g_free);
  g_clear_pointer (&self->metadata, g_variant_unref);

  g_clear_pointer (&self->cache, g_hash_table_unref);
  g_clear_pointer (&self->player_buffer, g_hash_table_unref);

  G_OBJECT_CLASS (valent_mpris_remote_parent_class)->finalize (object);
}

static void
valent_mpris_remote_class_init (ValentMprisRemoteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentMediaPlayerClass *player_class = VALENT_MEDIA_PLAYER_CLASS (klass);

  object_class->dispose = valent_mpris_remote_dispose;
  object_class->finalize = valent_mpris_remote_finalize;

  player_class->get_flags = valent_mpris_remote_get_flags;
  player_class->get_metadata = valent_mpris_remote_get_metadata;
  player_class->get_name = valent_mpris_remote_get_name;
  player_class->get_position = valent_mpris_remote_get_position;
  player_class->get_state = valent_mpris_remote_get_state;
  player_class->get_volume = valent_mpris_remote_get_volume;
  player_class->set_volume = valent_mpris_remote_set_volume;

  player_class->next = valent_mpris_remote_next;
  player_class->open_uri = valent_mpris_remote_open_uri;
  player_class->pause = valent_mpris_remote_pause;
  player_class->play = valent_mpris_remote_play;
  player_class->previous = valent_mpris_remote_previous;
  player_class->seek = valent_mpris_remote_seek;
  player_class->stop = valent_mpris_remote_stop;

  /**
   * ValentMprisRemote::method-call:
   * @player: a #ValentMprisRemote
   * @method_name: the method name
   * @method_args: the method arguments
   *
   * #ValentMprisRemote::method-call is emitted when a method is called by a
   * consumer of the exported interface.
   */
  signals [METHOD_CALL] =
    g_signal_new ("method-call",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_VARIANT);

  /**
   * ValentMprisRemote::set-property:
   * @player: a #ValentMprisRemote
   * @property_name: the property name
   * @property_value: the property value
   *
   * #ValentMprisRemote::set-property is emitted when a property is set by a
   * consumer of the exported interface.
   */
  signals [SET_PROPERTY] =
    g_signal_new ("set-property",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_VARIANT);

  valent_mpris_remote_auto_export_init ();
}

static void
valent_mpris_remote_init (ValentMprisRemote *self)
{
  GVariant *rate;
  
  self->identity = g_strdup ("Media Player");
  self->bus_name = g_strdup (VALENT_MPRIS_DBUS_NAME);

  self->loop_status = g_strdup ("None");
  self->volume = 1.0;
  self->state = VALENT_MEDIA_STATE_STOPPED;

  self->application_vtable.method_call = application_method_call;
  self->application_vtable.get_property = application_get_property;
  self->application_vtable.set_property = application_set_property;

  self->player_vtable.method_call = player_method_call;
  self->player_vtable.get_property = player_get_property;
  self->player_vtable.set_property = player_set_property;

  self->cache = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       (GDestroyNotify)g_variant_unref);
  self->player_buffer = g_hash_table_new_full (g_str_hash,
                                               g_str_equal,
                                               g_free,
                                               (GDestroyNotify)g_variant_unref);

  /* Static values */
  rate = g_variant_new_double (1.0);
  g_hash_table_insert (self->cache,
                       g_strdup ("Rate"),
                       g_variant_ref_sink (rate));
  g_hash_table_insert (self->cache,
                       g_strdup ("MaximumRate"),
                       g_variant_ref_sink (rate));
  g_hash_table_insert (self->cache,
                       g_strdup ("MinimumRate"),
                       g_variant_ref_sink (rate));
}

/**
 * valent_media_player_new:
 *
 * Get the #ValentMprisRemote instance.
 *
 * Returns: (transfer none) (nullable): a #ValentMprisRemote
 */
ValentMprisRemote *
valent_mpris_remote_new (void)
{
  return g_object_new (VALENT_TYPE_MPRIS_REMOTE, NULL);
}

/**
 * valent_media_player_export:
 * @remote: a #ValentMprisRemote
 *
 * Add @remote to the pool of auto-exported remotes.
 *
 * Whenever a remote is exported on the bus name %VALENT_MPRIS_DBUS_NAME it is
 * placed in a pool. The remote most recently in a play state gets exported.
 */
void
valent_mpris_remote_export (ValentMprisRemote *remote)
{
  g_autoptr (GError) error = NULL;

  g_return_if_fail (VALENT_IS_MPRIS_REMOTE (remote));

  /* Auto export */
  if (valent_mpris_remote_auto_export (remote))
    return;

  /* We already have a connection */
  if (remote->connection != NULL)
    {
      if (!valent_mpris_remote_register (remote, &error))
        g_warning ("%s: %s", G_STRFUNC, error->message);
    }
}

static void
export_full_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentMprisRemote *self = g_task_get_source_object (task);
  GError *error = NULL;

  self->connection = g_dbus_connection_new_for_address_finish (result, &error);

  if (self->connection == NULL)
    return g_task_return_error (task, error);

  if (!valent_mpris_remote_register (self, &error))
    return g_task_return_error (task, error);

  g_task_return_boolean (task, TRUE);
}

/**
 * valent_media_player_export_full:
 * @remote: a #ValentMprisRemote
 * @bus_name: the well-known name to own
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Export the test media player on the session bus.
 */
void
valent_mpris_remote_export_full (ValentMprisRemote   *remote,
                                 const char          *bus_name,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autofree char *address = NULL;
  GError *error = NULL;

  g_return_if_fail (VALENT_IS_MPRIS_REMOTE (remote));
  g_return_if_fail (g_dbus_is_name (bus_name));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (remote, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mpris_remote_export_full);

  /* Set the new bus name */
  g_clear_pointer (&remote->bus_name, g_free);
  remote->bus_name = g_strdup (bus_name);

  /* Set up a dedicated connection */
  address = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION,
                                             cancellable,
                                             &error);

  if (address == NULL)
    return g_task_return_error (task, error);

  g_dbus_connection_new_for_address (address,
                                     G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                     G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                     NULL,
                                     cancellable,
                                     (GAsyncReadyCallback)export_full_cb,
                                     g_steal_pointer (&task));
}

/**
 * valent_mpris_remote_export_finish:
 * @remote: a #ValentMprisRemote
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Finish an operation started by valent_mpris_remote_export().
 *
 * Returns: %TRUE or %FALSE with @error set
 */
gboolean
valent_mpris_remote_export_finish (ValentMprisRemote  *remote,
                                   GAsyncResult       *result,
                                   GError            **error)
{
  g_return_val_if_fail (VALENT_IS_MPRIS_REMOTE (remote), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, remote), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * valent_media_player_unexport:
 * @remote: a #ValentMprisRemote
 *
 * Unexport the player on the session bus.
 */
void
valent_mpris_remote_unexport (ValentMprisRemote *remote)
{
  g_return_if_fail (VALENT_IS_MPRIS_REMOTE (remote));

  /* Auto unexport */
  if (valent_mpris_remote_auto_unexport (remote))
    return;

  /* Unexport in reverse order */
  g_clear_handle_id (&remote->flush_id, g_source_remove);
  g_clear_handle_id (&remote->bus_name_id, g_bus_unown_name);

  if (remote->player_id > 0)
    {
      g_dbus_connection_unregister_object (remote->connection,
                                           remote->player_id);
      remote->player_id = 0;
    }

  if (remote->application_id > 0)
    {
      g_dbus_connection_unregister_object (remote->connection,
                                           remote->application_id);
      remote->application_id = 0;
    }
}

/**
 * valent_media_player_set_name:
 * @remote: a #ValentMprisRemote
 * @identity: a name
 *
 * Set the user-visible name of the player to @identity.
 */
void
valent_mpris_remote_set_name (ValentMprisRemote *remote,
                              const char        *identity)
{
  g_return_if_fail (VALENT_IS_MPRIS_REMOTE (remote));
  g_return_if_fail (identity != NULL);

  if (g_strcmp0 (remote->identity, identity) == 0)
    return;

  g_clear_pointer (&remote->identity, g_free);
  remote->identity = g_strdup (identity);
}

/**
 * valent_mpris_remote_emit_seeked:
 * @self: a #ValentMprisImpl
 * @position: new position
 *
 * Emits the "Seeked" signal.
 */
void
valent_mpris_remote_emit_seeked (ValentMprisRemote *self,
                                 gint64             position)
{
  g_autoptr (GError) error = NULL;
  gboolean ret;

  ret = g_dbus_connection_emit_signal (self->connection,
                                       NULL,
                                       "/org/mpris/MediaPlayer2",
                                       "org.mpris.MediaPlayer2.Player",
                                       "Seeked",
                                       g_variant_new ("(x)", position),
                                       &error);

  if (!ret)
    g_warning ("%s: %s", G_STRFUNC, error->message);
}

/**
 * valent_media_player_update:
 * @remote: a #ValentMprisRemote
 * @flags: a #ValentMediaActions bitmask
 * @metadata: (nullable): the track metadata
 * @playback_status: (nullable): the playback status (`Stopped`, `Paused`, `Playing`)
 * @position: the playback position
 * @volume: the playback volume
 *
 * A convenience method for updating the internal state of the most commom
 * properties.
 *
 * If @metadata is a floating reference, it will be consumed.
 */
void
valent_mpris_remote_update_player (ValentMprisRemote  *remote,
                                   ValentMediaActions  flags,
                                   GVariant           *metadata,
                                   const char         *playback_status,
                                   gint64              position,
                                   double              volume)
{
  g_assert (VALENT_IS_MPRIS_REMOTE (remote));

  valent_mpris_remote_update_flags (remote, flags);

  if (metadata != NULL)
    valent_mpris_remote_update_metadata (remote, metadata);

  if (playback_status != NULL)
    valent_mpris_remote_update_playback_status (remote, playback_status);

  valent_mpris_remote_update_position (remote, position);
  valent_mpris_remote_update_volume (remote, volume);
}

/**
 * valent_mpris_remote_update_art:
 * @remote: a #ValentMprisRemote
 * @file: a #GFile
 *
 * Update the `mpris:artUrl` metadata field from @file.
 */
void
valent_mpris_remote_update_art (ValentMprisRemote *remote,
                                GFile             *file)
{
  GVariantDict dict;
  GVariant *metadata;
  g_autofree char *uri = NULL;

  g_assert (VALENT_IS_MPRIS_REMOTE (remote));
  g_assert (G_IS_FILE (file));

  uri = g_file_get_uri (file);

  g_variant_dict_init (&dict, remote->metadata);
  g_variant_dict_insert (&dict, "mpris:artUrl", "s", uri);
  metadata = g_variant_dict_end (&dict);

  valent_mpris_remote_update_metadata (remote, metadata);
}

/**
 * valent_mpris_remote_update_flags:
 * @remote: a #ValentMprisRemote
 * @flags: a #ValentMediaActions
 *
 * Set the #ValentMediaPlayer:flags property.
 */
void
valent_mpris_remote_update_flags (ValentMprisRemote  *remote,
                                  ValentMediaActions  flags)
{
  ValentMediaActions mask = remote->flags ^ flags;
  GVariant *value;
  gboolean enabled;

  g_assert (VALENT_IS_MPRIS_REMOTE (remote));

  if (mask == 0)
    return;

  if ((mask & VALENT_MEDIA_ACTION_NEXT) != 0)
    {
      enabled = (flags & VALENT_MEDIA_ACTION_NEXT) != 0;
      value = g_variant_new_boolean (enabled);
      valent_mpris_remote_set_value (remote, "CanGoNext", value);
    }

  if ((mask & VALENT_MEDIA_ACTION_PAUSE) != 0)
    {
      enabled = (flags & VALENT_MEDIA_ACTION_PAUSE) != 0;
      value = g_variant_new_boolean (enabled);
      valent_mpris_remote_set_value (remote, "CanPause", value);
    }

  if ((mask & VALENT_MEDIA_ACTION_PLAY) != 0)
    {
      enabled = (flags & VALENT_MEDIA_ACTION_PLAY) != 0;
      value = g_variant_new_boolean (enabled);
      valent_mpris_remote_set_value (remote, "CanPlay", value);
    }

  if ((mask & VALENT_MEDIA_ACTION_PREVIOUS) != 0)
    {
      enabled = (flags & VALENT_MEDIA_ACTION_PREVIOUS) != 0;
      value = g_variant_new_boolean (enabled);
      valent_mpris_remote_set_value (remote, "CanGoPrevious", value);
    }

  if ((mask & VALENT_MEDIA_ACTION_SEEK) != 0)
    {
      enabled = (flags & VALENT_MEDIA_ACTION_SEEK) != 0;
      value = g_variant_new_boolean (enabled);
      valent_mpris_remote_set_value (remote, "CanSeek", value);
    }

  remote->flags = flags;
  g_object_notify (G_OBJECT (remote), "flags");
}

/**
 * valent_mpris_remote_update_metadata:
 * @remote: a #ValentMprisRemote
 * @value: the metadata
 *
 * Set the `Metadata` property.
 */
void
valent_mpris_remote_update_metadata (ValentMprisRemote *remote,
                                     GVariant          *value)
{
  g_assert (VALENT_IS_MPRIS_REMOTE (remote));

  if (remote->metadata == NULL && value == NULL)
    return;

  g_clear_pointer (&remote->metadata, g_variant_unref);
  remote->metadata = g_variant_ref_sink (value);
  g_object_notify (G_OBJECT (remote), "metadata");
  valent_mpris_remote_set_value (remote, "Metadata", value);
}

/**
 * valent_mpris_remote_update_playback_status:
 * @remote: a #ValentMprisRemote
 * @status: the playback status
 *
 * Set the `PlaybackStatus`.
 */
void
valent_mpris_remote_update_playback_status (ValentMprisRemote *remote,
                                            const char        *status)
{
  g_assert (VALENT_IS_MPRIS_REMOTE (remote));
  g_assert (status != NULL);

  if (g_str_equal (status, "Paused"))
    {
      if ((remote->state & VALENT_MEDIA_STATE_PAUSED) != 0)
        return;

      remote->state &= ~VALENT_MEDIA_STATE_PLAYING;
      remote->state |= VALENT_MEDIA_STATE_PAUSED;
    }

  else if (g_str_equal (status, "Playing"))
    {
      if ((remote->state & VALENT_MEDIA_STATE_PLAYING) != 0)
        return;

      remote->state &= ~VALENT_MEDIA_STATE_PAUSED;
      remote->state |= VALENT_MEDIA_STATE_PLAYING;
    }

  else if (g_str_equal (status, "Stopped"))
    {
      if ((remote->state & VALENT_MEDIA_STATE_PAUSED) == 0 &&
          (remote->state & VALENT_MEDIA_STATE_PLAYING) == 0)
        return;

      remote->state &= ~VALENT_MEDIA_STATE_PAUSED;
      remote->state &= ~VALENT_MEDIA_STATE_PLAYING;
    }

  g_object_notify (G_OBJECT (remote), "state");
  valent_mpris_remote_set_value (remote,
                                 "PlaybackStatus",
                                 g_variant_new_string (status));
}

/**
 * valent_mpris_remote_update_position:
 * @remote: a #ValentMprisRemote
 * @rate: the playback rate
 *
 * Set the `Position` property.
 */
void
valent_mpris_remote_update_position (ValentMprisRemote *remote,
                                     gint64             position)
{
  g_assert (VALENT_IS_MPRIS_REMOTE (remote));

  remote->position = position;
}

/**
 * valent_mpris_remote_update_volume:
 * @remote: a #ValentMprisRemote
 * @volume: a level
 *
 * Set the `Volume` property.
 */
void
valent_mpris_remote_update_volume (ValentMprisRemote *remote,
                                   double             volume)
{
  g_assert (VALENT_IS_MPRIS_REMOTE (remote));

  if (remote->volume == volume)
    return;

  remote->volume = volume;
  g_object_notify (G_OBJECT (remote), "volume");
  valent_mpris_remote_set_value (remote, "Volume", g_variant_new_double (volume));
}

