// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mpris-impl"

#include "config.h"

#include <gio/gio.h>
#include <valent.h>

#include "valent-mpris-impl.h"
#include "valent-mpris-utils.h"


struct _ValentMPRISImpl
{
  GObject               parent_instance;

  ValentMediaPlayer    *player;
  GDBusConnection      *connection;
  char                 *bus_name;
  unsigned int          bus_name_id;

  GHashTable           *cache;
  GHashTable           *pending;
  unsigned int          flush_id;

  /* org.mpris.MediaPlayer2 */
  unsigned int          application_id;
  GDBusInterfaceVTable  application_vtable;

  /* org.mpris.MediaPlayer2.Player */
  unsigned int          player_id;
  GDBusInterfaceVTable  player_vtable;
};

G_DEFINE_FINAL_TYPE (ValentMPRISImpl, valent_mpris_impl, G_TYPE_OBJECT)


enum {
  PROP_0,
  PROP_PLAYER,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * org.mpris.MediaPlayer2
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
  g_assert (VALENT_IS_MPRIS_IMPL (user_data));
  g_assert (method_name != NULL);

  if (g_str_equal (method_name, "Raise"))
    {
      GApplication *application = g_application_get_default ();

      if (application != NULL)
        {
          g_action_group_activate_action (G_ACTION_GROUP (application),
                                          "media-remote",
                                          NULL);
        }
    }

  /* Silently ignore method calls */
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
  ValentMPRISImpl *self = VALENT_MPRIS_IMPL (user_data);
  GVariant *value = NULL;

  g_assert (VALENT_IS_MPRIS_IMPL (self));
  g_assert (property_name != NULL);

  if ((value = g_hash_table_lookup (self->cache, property_name)) != NULL)
    return g_variant_ref_sink (value);

  if (g_str_equal (property_name, "Identity"))
    value = g_variant_new_string (valent_media_player_get_name (self->player));
  else if (g_str_equal (property_name, "CanQuit"))
    value = g_variant_new_boolean (FALSE);
  else if (g_str_equal (property_name, "CanRaise"))
    value = g_variant_new_boolean (TRUE);
  else if (g_str_equal (property_name, "CanSetFullscreen"))
    value = g_variant_new_boolean (FALSE);
  else if (g_str_equal (property_name, "DesktopEntry"))
    value = g_variant_new_string (APPLICATION_ID".desktop");
  else if (g_str_equal (property_name, "Fullscreen") ||
           g_str_equal (property_name, "HasTrackList"))
    value = g_variant_new_boolean (FALSE);
  else if (g_str_equal (property_name, "SupportedMimeTypes") ||
           g_str_equal (property_name, "SupportedUriSchemes"))
    value = g_variant_new_strv (NULL, 0);

  if (value != NULL)
    {
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

/* LCOV_EXCL_START */
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
  /* Silently ignore property setters */
  return TRUE;
}
/* LCOV_EXCL_STOP */

/*
 * org.mpris.MediaPlayer2.Player
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
  ValentMPRISImpl *self = VALENT_MPRIS_IMPL (user_data);

  g_assert (VALENT_IS_MPRIS_IMPL (self));
  g_assert (method_name != NULL);

  if (g_str_equal (method_name, "Next"))
    {
      valent_media_player_next (self->player);
    }
  else if (g_str_equal (method_name, "Pause"))
    {
      valent_media_player_pause (self->player);
    }
  else if (g_str_equal (method_name, "Play"))
    {
      valent_media_player_play (self->player);
    }
  else if (g_str_equal (method_name, "PlayPause"))
    {
      valent_mpris_play_pause (self->player);
    }
  else if (g_str_equal (method_name, "Previous"))
    {
      valent_media_player_previous (self->player);
    }
  else if (g_str_equal (method_name, "Seek"))
    {
      int64_t offset_us;

      /* Convert microseconds to seconds */
      g_variant_get (parameters, "(x)", &offset_us);
      valent_media_player_seek (self->player, offset_us / G_TIME_SPAN_SECOND);
    }
  else if (g_str_equal (method_name, "SetPosition"))
    {
      int64_t position_us;

      /* Convert microseconds to seconds */
      g_variant_get (parameters, "(&ox)", NULL, &position_us);
      valent_media_player_set_position (self->player, position_us / G_TIME_SPAN_SECOND);
    }
  else if (g_str_equal (method_name, "Stop"))
    {
      valent_media_player_stop (self->player);
    }
  else if (g_str_equal (method_name, "OpenUri"))
    {
      /* Silently ignore method calls */
    }
  else
    {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR,
                                             G_DBUS_ERROR_UNKNOWN_METHOD,
                                             "Unknown method \"%s\"",
                                             method_name);
      return;
    }

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
  ValentMPRISImpl *self = VALENT_MPRIS_IMPL (user_data);
  ValentMediaActions flags = VALENT_MEDIA_ACTION_NONE;
  GVariant *value;

  g_assert (VALENT_IS_MPRIS_IMPL (self));
  g_assert (property_name != NULL);

  /* Check cache */
  if ((value = g_hash_table_lookup (self->cache, property_name)) != NULL)
    return g_variant_ref (value);

  /* The `Position` is not cached, because `PropertiesChanged` is not emitted */
  if (g_str_equal (property_name, "Position"))
    {
      double position = valent_media_player_get_position (self->player);

      /* Convert seconds to microseconds */
      return g_variant_new_int64 (position * G_TIME_SPAN_SECOND);
    }

  /* Load properties */
  if (*property_name == 'C')
    flags = valent_media_player_get_flags (self->player);

  if (g_str_equal (property_name, "CanControl"))
    {
      value = g_variant_new_boolean (flags != 0);
    }
  else if (g_str_equal (property_name, "CanGoNext"))
    {
      value = g_variant_new_boolean ((flags & VALENT_MEDIA_ACTION_NEXT) != 0);
    }
  else if (g_str_equal (property_name, "CanGoPrevious"))
    {
      value = g_variant_new_boolean ((flags & VALENT_MEDIA_ACTION_PREVIOUS) != 0);
    }
  else if (g_str_equal (property_name, "CanPlay"))
    {
      value = g_variant_new_boolean ((flags & VALENT_MEDIA_ACTION_PLAY) != 0);
    }
  else if (g_str_equal (property_name, "CanPause"))
    {
      value = g_variant_new_boolean ((flags & VALENT_MEDIA_ACTION_PAUSE) != 0);
    }
  else if (g_str_equal (property_name, "CanSeek"))
    {
      value = g_variant_new_boolean ((flags & VALENT_MEDIA_ACTION_SEEK) != 0);
    }
  else if (g_str_equal (property_name, "Metadata"))
    {
      value = valent_media_player_get_metadata (self->player);
    }
  else if (g_str_equal (property_name, "LoopStatus"))
    {
      ValentMediaRepeat repeat = valent_media_player_get_repeat (self->player);

      value = g_variant_new_string (valent_mpris_repeat_to_string (repeat));
    }
  else if (g_str_equal (property_name, "Shuffle"))
    {
      gboolean shuffle = valent_media_player_get_shuffle (self->player);

      value = g_variant_new_boolean (shuffle);
    }
  else if (g_str_equal (property_name, "PlaybackStatus"))
    {
      ValentMediaState state = valent_media_player_get_state (self->player);

      value = g_variant_new_string (valent_mpris_state_to_string (state));
    }
  else if (g_str_equal (property_name, "Volume"))
    {
      double volume = valent_media_player_get_volume (self->player);

      value = g_variant_new_double (volume);
    }
  else if (g_str_equal (property_name, "Rate") ||
           g_str_equal (property_name, "MaximumRate") ||
           g_str_equal (property_name, "MinimumRate"))
    {
      value = g_variant_new_double (1.0);
    }

  if (value != NULL)
    {
      g_hash_table_replace (self->cache,
                            g_strdup (property_name),
                            g_variant_take_ref (value));

      return g_variant_ref (value);
    }

  g_set_error (error,
               G_DBUS_ERROR,
               G_DBUS_ERROR_UNKNOWN_PROPERTY,
               "Unknown property \"%s\"",
               property_name);

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
  ValentMPRISImpl *self = VALENT_MPRIS_IMPL (user_data);

  g_assert (VALENT_IS_MPRIS_IMPL (self));
  g_assert (property_name != NULL);

  if (g_str_equal (property_name, "LoopStatus"))
    {
      const char *loop_status = g_variant_get_string (value, NULL);
      ValentMediaRepeat repeat = valent_mpris_repeat_from_string (loop_status);

      valent_media_player_set_repeat (self->player, repeat);
      return TRUE;
    }

  if (g_str_equal (property_name, "Shuffle"))
    {
      gboolean shuffle = g_variant_get_boolean (value);

      valent_media_player_set_shuffle (self->player, shuffle);
      return TRUE;
    }

  if (g_str_equal (property_name, "Volume"))
    {
      double volume = g_variant_get_double (value);

      valent_media_player_set_volume (self->player, volume);
      return TRUE;
    }

  return TRUE;
}

static gboolean
valent_mpris_impl_flush (gpointer data)
{
  ValentMPRISImpl *self = VALENT_MPRIS_IMPL (data);
  g_autoptr (GError) error = NULL;
  GVariant *parameters;
  GVariantBuilder changed_props;
  GVariantBuilder invalidated_props;
  GHashTableIter iter;
  GVariant *value;
  char *name;

  g_assert (VALENT_IS_MPRIS_IMPL (self));

  if (self->connection != NULL)
    {
      g_variant_builder_init (&changed_props, G_VARIANT_TYPE_VARDICT);
      g_variant_builder_init (&invalidated_props, G_VARIANT_TYPE_STRING_ARRAY);

      g_hash_table_iter_init (&iter, self->pending);

      while (g_hash_table_iter_next (&iter, (void**)&name, (void**)&value))
        {
          if (value)
            g_variant_builder_add (&changed_props, "{sv}", name, value);
          else
            g_variant_builder_add (&invalidated_props, "s", name);

          g_hash_table_iter_remove (&iter);
        }

      parameters = g_variant_new ("(s@a{sv}@as)",
                                  "org.mpris.MediaPlayer2.Player",
                                  g_variant_builder_end (&changed_props),
                                  g_variant_builder_end (&invalidated_props));

      g_dbus_connection_emit_signal (self->connection,
                                     NULL,
                                     "/org/mpris/MediaPlayer2",
                                     "org.freedesktop.DBus.Properties",
                                     "PropertiesChanged",
                                     parameters,
                                     &error);

      if (error != NULL)
        g_warning ("%s(): %s", G_STRFUNC, error->message);
    }

  g_clear_handle_id (&self->flush_id, g_source_remove);

  return G_SOURCE_REMOVE;
}

static void
valent_mpris_impl_set_value (ValentMPRISImpl *self,
                             const char      *name,
                             GVariant        *value)
{
  g_assert (VALENT_IS_MPRIS_IMPL (self));
  g_assert (name != NULL && *name != '\0');
  g_assert (value != NULL);

  g_hash_table_replace (self->cache,
                        g_strdup (name),
                        g_variant_ref_sink (value));
  g_hash_table_replace (self->pending,
                        g_strdup (name),
                        g_variant_ref_sink (value));

  if (self->flush_id == 0)
    self->flush_id = g_idle_add (valent_mpris_impl_flush, self);
}

static void
valent_mpris_impl_propagate_seeked (ValentMPRISImpl *self,
                                    int64_t          position)
{
  g_autoptr (GError) error = NULL;
  gboolean ret;

  if (self->connection == NULL)
    return;

  ret = g_dbus_connection_emit_signal (self->connection,
                                       NULL,
                                       "/org/mpris/MediaPlayer2",
                                       "org.mpris.MediaPlayer2.Player",
                                       "Seeked",
                                       g_variant_new ("(x)", position),
                                       &error);

  if (!ret)
    g_warning ("%s(): %s", G_STRFUNC, error->message);
}

static void
valent_mpris_impl_propagate_notify (ValentMediaPlayer *player,
                                    GParamSpec        *pspec,
                                    ValentMPRISImpl   *self)
{
  const char *name = g_param_spec_get_name (pspec);
  GVariant *value = NULL;

  if (g_str_equal (name, "flags"))
    {
      ValentMediaActions flags = valent_media_player_get_flags (self->player);

      value = g_variant_new_boolean (flags != 0);
      valent_mpris_impl_set_value (self, "CanControl", value);
      value = g_variant_new_boolean ((flags & VALENT_MEDIA_ACTION_NEXT) != 0);
      valent_mpris_impl_set_value (self, "CanGoNext", value);
      value = g_variant_new_boolean ((flags & VALENT_MEDIA_ACTION_PAUSE) != 0);
      valent_mpris_impl_set_value (self, "CanPause", value);
      value = g_variant_new_boolean ((flags & VALENT_MEDIA_ACTION_PLAY) != 0);
      valent_mpris_impl_set_value (self, "CanPlay", value);
      value = g_variant_new_boolean ((flags & VALENT_MEDIA_ACTION_PREVIOUS) != 0);
      valent_mpris_impl_set_value (self, "CanGoPrevious", value);
      value = g_variant_new_boolean ((flags & VALENT_MEDIA_ACTION_SEEK) != 0);
      valent_mpris_impl_set_value (self, "CanSeek", value);
    }
  else if (g_str_equal (name, "metadata"))
    {
      value = valent_media_player_get_metadata (self->player);
      valent_mpris_impl_set_value (self, "Metadata", value);
      g_variant_unref (value);
    }
  else if (g_str_equal (name, "name"))
    {
      const char *identity = valent_media_player_get_name (self->player);

      value = g_variant_new_string (identity);
      g_hash_table_replace (self->cache,
                            g_strdup ("Identity"),
                            g_variant_ref_sink (value));
    }
  else if (g_str_equal (name, "position"))
    {
      double position = valent_media_player_get_position (self->player);

      /* Convert seconds to microseconds */
      value = g_variant_new_int64 (position * G_TIME_SPAN_SECOND);
      g_hash_table_replace (self->cache,
                            g_strdup ("Position"),
                            g_variant_ref_sink (value));

      /* Convert seconds to microseconds */
      valent_mpris_impl_propagate_seeked (self, position * G_TIME_SPAN_SECOND);
    }
  else if (g_str_equal (name, "repeat"))
    {
      ValentMediaRepeat repeat = valent_media_player_get_repeat (self->player);

      value = g_variant_new_string (valent_mpris_repeat_to_string (repeat));
      valent_mpris_impl_set_value (self, "LoopStatus", value);
    }
  else if (g_str_equal (name, "shuffle"))
    {
      gboolean shuffle = valent_media_player_get_shuffle (self->player);

      value = g_variant_new_boolean (shuffle);
      valent_mpris_impl_set_value (self, "Shuffle", value);
    }
  else if (g_str_equal (name, "state"))
    {
      ValentMediaState state = valent_media_player_get_state (self->player);

      value = g_variant_new_string (valent_mpris_state_to_string (state));
      valent_mpris_impl_set_value (self, "PlaybackStatus", value);
    }
  else if (g_str_equal (name, "volume"))
    {
      double volume = valent_media_player_get_volume (self->player);

      value = g_variant_new_double (volume);
      valent_mpris_impl_set_value (self, "Volume", value);
    }
}

/*
 * GObject
 */
static void
valent_mpris_impl_constructed (GObject *object)
{
  ValentMPRISImpl *self = VALENT_MPRIS_IMPL (object);

  g_assert (VALENT_IS_MEDIA_PLAYER (self->player));

  g_signal_connect_object (self->player,
                           "notify",
                           G_CALLBACK (valent_mpris_impl_propagate_notify),
                           self, 0);

  G_OBJECT_CLASS (valent_mpris_impl_parent_class)->constructed (object);
}

static void
valent_mpris_impl_dispose (GObject *object)
{
  ValentMPRISImpl *self = VALENT_MPRIS_IMPL (object);

  g_signal_handlers_disconnect_by_data (self->player, self);
  valent_mpris_impl_unexport (self);

  G_OBJECT_CLASS (valent_mpris_impl_parent_class)->dispose (object);
}

static void
valent_mpris_impl_finalize (GObject *object)
{
  ValentMPRISImpl *self = VALENT_MPRIS_IMPL (object);

  g_clear_pointer (&self->bus_name, g_free);
  g_clear_object (&self->connection);
  g_clear_object (&self->player);

  g_clear_pointer (&self->cache, g_hash_table_unref);
  g_clear_pointer (&self->pending, g_hash_table_unref);

  G_OBJECT_CLASS (valent_mpris_impl_parent_class)->finalize (object);
}

static void
valent_mpris_impl_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ValentMPRISImpl *self = VALENT_MPRIS_IMPL (object);

  switch (prop_id)
    {
    case PROP_PLAYER:
      g_value_set_object (value, self->player);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mpris_impl_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ValentMPRISImpl *self = VALENT_MPRIS_IMPL (object);

  switch (prop_id)
    {
    case PROP_PLAYER:
      self->player = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mpris_impl_class_init (ValentMPRISImplClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_mpris_impl_constructed;
  object_class->dispose = valent_mpris_impl_dispose;
  object_class->finalize = valent_mpris_impl_finalize;
  object_class->get_property = valent_mpris_impl_get_property;
  object_class->set_property = valent_mpris_impl_set_property;

  /**
   * ValentMprisDevice:player:
   *
   * The [class@Valent.MediaPlayer] being exported.
   */
  properties [PROP_PLAYER] =
    g_param_spec_object ("player", NULL, NULL,
                         VALENT_TYPE_MEDIA_PLAYER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_mpris_impl_init (ValentMPRISImpl *self)
{
  self->application_vtable.method_call = application_method_call;
  self->application_vtable.get_property = application_get_property;
  self->application_vtable.set_property = application_set_property;

  self->player_vtable.method_call = player_method_call;
  self->player_vtable.get_property = player_get_property;
  self->player_vtable.set_property = player_set_property;

  self->bus_name = g_strdup (VALENT_MPRIS_DBUS_NAME);
  self->cache = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       (GDestroyNotify)g_variant_unref);
  self->pending = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         (GDestroyNotify)g_variant_unref);
}

/**
 * valent_mpris_impl_new:
 * @player: a #ValentMediaPlayer
 *
 * Get the #ValentMPRISImpl instance.
 *
 * Returns: (transfer full) (nullable): a #ValentMPRISImpl
 */
ValentMPRISImpl *
valent_mpris_impl_new (ValentMediaPlayer *player)
{
  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER (player), NULL);

  return g_object_new (VALENT_TYPE_MPRIS_IMPL,
                       "player", player,
                       NULL);
}

/**
 * valent_media_player_impl_export:
 * @impl: a #ValentMPRISImpl
 * @connection: a #GDBusConnection
 * @error:
 *
 * Impl @impl on @connection.
 */
gboolean
valent_mpris_impl_export (ValentMPRISImpl  *impl,
                          GDBusConnection  *connection,
                          GError          **error)
{
  g_return_val_if_fail (VALENT_IS_MPRIS_IMPL (impl), FALSE);
  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (impl->connection == connection)
    return TRUE;

  /* Unexport from any existing connection */
  valent_mpris_impl_unexport (impl);
  impl->connection = g_object_ref (connection);

  /* Register org.mpris.MediaPlayer2 interface */
  if (impl->application_id == 0)
    {
      impl->application_id =
        g_dbus_connection_register_object (impl->connection,
                                           "/org/mpris/MediaPlayer2",
                                           VALENT_MPRIS_APPLICATION_INFO,
                                           &impl->application_vtable,
                                           impl, NULL,
                                           error);

      if (impl->application_id == 0)
        {
          valent_mpris_impl_unexport (impl);
          return FALSE;
        }
    }

  /* Register org.mpris.MediaPlayer2.Player interface */
  if (impl->player_id == 0)
    {
      impl->player_id =
        g_dbus_connection_register_object (impl->connection,
                                           "/org/mpris/MediaPlayer2",
                                           VALENT_MPRIS_PLAYER_INFO,
                                           &impl->player_vtable,
                                           impl, NULL,
                                           error);

      if (impl->player_id == 0)
        {
          valent_mpris_impl_unexport (impl);
          return FALSE;
        }
    }

  /* Own a well-known name on the connection */
  if (impl->bus_name_id == 0)
    {
      impl->bus_name_id =
        g_bus_own_name_on_connection (impl->connection,
                                      impl->bus_name,
                                      G_BUS_NAME_OWNER_FLAGS_NONE,
                                      NULL, // NameAcquired
                                      NULL, // NameLost
                                      NULL,
                                      NULL);
    }

  return TRUE;
}

static void
valent_mpris_impl_export_full_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentMPRISImpl *self = g_task_get_source_object (task);
  g_autoptr (GDBusConnection) connection = NULL;
  g_autoptr (GError) error = NULL;

  connection = g_dbus_connection_new_for_address_finish (result, &error);

  if (connection == NULL)
    return g_task_return_error (task, g_steal_pointer (&error));

  if (!valent_mpris_impl_export (self, connection, &error))
    return g_task_return_error (task, g_steal_pointer (&error));

  g_task_return_boolean (task, TRUE);
}

/**
 * valent_mpris_impl_export_full:
 * @impl: a #ValentMPRISImpl
 * @bus_name: the well-known name to own
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Impl the test media player on the session bus.
 */
void
valent_mpris_impl_export_full (ValentMPRISImpl     *impl,
                               const char          *bus_name,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autofree char *address = NULL;
  g_autoptr (GError) error = NULL;

  g_return_if_fail (VALENT_IS_MPRIS_IMPL (impl));
  g_return_if_fail (g_dbus_is_name (bus_name));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (impl, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mpris_impl_export_full);

  /* Set the new bus name */
  g_set_str (&impl->bus_name, bus_name);

  /* Set up a dedicated connection */
  address = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION,
                                             cancellable,
                                             &error);

  if (address == NULL)
    return g_task_return_error (task, g_steal_pointer (&error));

  g_dbus_connection_new_for_address (address,
                                     G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                     G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                     NULL,
                                     cancellable,
                                     (GAsyncReadyCallback)valent_mpris_impl_export_full_cb,
                                     g_steal_pointer (&task));
}

/**
 * valent_mpris_impl_export_finish:
 * @impl: a #ValentMPRISImpl
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Finish an operation started by valent_mpris_impl_export_full().
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 */
gboolean
valent_mpris_impl_export_finish (ValentMPRISImpl  *impl,
                                 GAsyncResult     *result,
                                 GError          **error)
{
  g_return_val_if_fail (VALENT_IS_MPRIS_IMPL (impl), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, impl), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * valent_mpris_player_impl_unexport:
 * @impl: a #ValentMPRISImpl
 *
 * Unexport the player.
 */
void
valent_mpris_impl_unexport (ValentMPRISImpl *impl)
{
  g_return_if_fail (VALENT_IS_MPRIS_IMPL (impl));

  g_clear_handle_id (&impl->flush_id, g_source_remove);
  g_clear_handle_id (&impl->bus_name_id, g_bus_unown_name);

  if (impl->player_id > 0)
    {
      g_dbus_connection_unregister_object (impl->connection,
                                           impl->player_id);
      impl->player_id = 0;
    }

  if (impl->application_id > 0)
    {
      g_dbus_connection_unregister_object (impl->connection,
                                           impl->application_id);
      impl->application_id = 0;
    }

  g_clear_object (&impl->connection);
}

