// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mpris-remote"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-media.h>

#include "valent-mpris-remote.h"


struct _ValentMprisRemote
{
  ValentMediaPlayer     parent_instance;

  GDBusConnection      *connection;
  unsigned int          application_id;
  GDBusInterfaceVTable  application_vtable;
  unsigned int          player_id;
  GDBusInterfaceVTable  player_vtable;
  unsigned int          name_id;

  unsigned int          idle_id;
  GHashTable           *player_properties;
  GHashTable           *cache;
  char                 *bus_name;

  /* org.mpris.MediaPlayer2 */
  char                 *identity;
  unsigned int          fullscreen : 1;
  unsigned int          can_fullscreen : 1;
  unsigned int          can_quit : 1;
  unsigned int          can_raise : 1;
  unsigned int          has_tracklist : 1;

  /* org.mpris.MediaPlayer2.Player */
  ValentMediaActions    flags;
  ValentMediaState      state;
  char                 *loop_status;
  GVariant             *metadata;
  char                 *playback_status;
  gint64                position;
  double                volume;
};

static void valent_mpris_remote_flush (ValentMprisRemote *self);

G_DEFINE_TYPE (ValentMprisRemote, valent_mpris_remote, VALENT_TYPE_MEDIA_PLAYER)


enum {
  PLAYER_METHOD,
  SET_PROPERTY,
  N_SIGNALS
};

static guint signals [N_SIGNALS] = { 0, };


/*
 * DBus Interfaces
 */
static const char dbus_xml[] =
  "<node>"
  "  <interface name='org.mpris.MediaPlayer2'>"
  "    <method name='Raise'/>"
  "    <method name='Quit'/>"
  "    <property name='CanQuit' type='b' access='read'/>"
  "    <property name='Fullscreen' type='b' access='readwrite'/>"
  "    <property name='CanSetFullscreen' type='b' access='read'/>"
  "    <property name='CanRaise' type='b' access='read'/>"
  "    <property name='HasTrackList' type='b' access='read'/>"
  "    <property name='Identity' type='s' access='read'/>"
  "    <property name='DesktopEntry' type='s' access='read'/>"
  "    <property name='SupportedUriSchemes' type='as' access='read'/>"
  "    <property name='SupportedMimeTypes' type='as' access='read'/>"
  "  </interface>"
  "  <interface name='org.mpris.MediaPlayer2.Player'>"
  "    <method name='Next'/>"
  "    <method name='Previous'/>"
  "    <method name='Pause'/>"
  "    <method name='PlayPause'/>"
  "    <method name='Stop'/>"
  "    <method name='Play'/>"
  "    <method name='Seek'>"
  "      <arg direction='in' type='x' name='Offset'/>"
  "    </method>"
  "    <method name='SetPosition'>"
  "      <arg direction='in' type='o' name='TrackId'/>"
  "      <arg direction='in' type='x' name='Position'/>"
  "    </method>"
  "    <method name='OpenUri'>"
  "      <arg direction='in' type='s' name='Uri'/>"
  "    </method>"
  "    <property name='PlaybackStatus' type='s' access='read'/>"
  "    <property name='LoopStatus' type='s' access='readwrite'/>"
  "    <property name='Rate' type='d' access='readwrite'/>"
  "    <property name='Shuffle' type='b' access='readwrite'/>"
  "    <property name='Metadata' type='a{sv}' access='read'/>"
  "    <property name='Volume' type='d' access='readwrite'/>"
  "    <property name='Position' type='x' access='read'/>"
  "    <property name='MinimumRate' type='d' access='read'/>"
  "    <property name='MaximumRate' type='d' access='read'/>"
  "    <property name='CanGoNext' type='b' access='read'/>"
  "    <property name='CanGoPrevious' type='b' access='read'/>"
  "    <property name='CanPlay' type='b' access='read'/>"
  "    <property name='CanPause' type='b' access='read'/>"
  "    <property name='CanSeek' type='b' access='read'/>"
  "    <property name='CanControl' type='b' access='read'/>"
  "    <signal name='Seeked'>"
  "      <arg name='Position' type='x'/>"
  "    </signal>"
  "  </interface>"
  "</node>";

static GDBusNodeInfo *node_info = NULL;


static inline void
init_node_info (void)
{
  if G_LIKELY (node_info != NULL)
    return;

  node_info = g_dbus_node_info_new_for_xml (dbus_xml, NULL);
  g_dbus_interface_info_cache_build (node_info->interfaces[0]);
  g_dbus_interface_info_cache_build (node_info->interfaces[1]);
}

static gboolean
idle_cb (gpointer data)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (data);

  if (G_IS_DBUS_CONNECTION (self->connection))
    valent_mpris_remote_flush (self);

  return G_SOURCE_REMOVE;
}

static void
emit_property_changed (ValentMprisRemote *self,
                       const char        *property_name,
                       GVariant          *property_value)
{
  g_hash_table_replace (self->cache,
                        g_strdup (property_name),
                        g_variant_ref_sink (property_value));

  g_hash_table_replace (self->player_properties,
                        g_strdup (property_name),
                        g_variant_ref_sink (property_value));

  if (self->idle_id == 0)
    self->idle_id = g_idle_add (idle_cb, self);
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
  g_dbus_method_invocation_return_error (invocation,
                                         G_DBUS_ERROR,
                                         G_DBUS_ERROR_NOT_SUPPORTED,
                                         "%s is not supported",
                                         method_name);
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
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "CanQuit") == 0)
    {
      value = g_variant_new_boolean (self->can_quit);
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "Fullscreen") == 0)
    {
      value = g_variant_new_boolean (self->fullscreen);
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "CanSetFullscreen") == 0)
    {
      value = g_variant_new_boolean (self->can_fullscreen);
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "CanRaise") == 0)
    {
      value = g_variant_new_boolean (self->can_raise);
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "HasTrackList") == 0)
    {
      value = g_variant_new_boolean (self->has_tracklist);
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "DesktopEntry") == 0)
    {
      value = g_variant_new_string (APPLICATION_ID".desktop");
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "SupportedUriSchemes") == 0)
    {
      value = g_variant_new_strv (NULL, 0);
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "SupportedMimeTypes") == 0)
    {
      value = g_variant_new_strv (NULL, 0);
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

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
      gboolean fullscreen = g_variant_get_boolean (value);

      if (self->fullscreen != fullscreen)
        {
          self->fullscreen = fullscreen;
          g_object_notify (G_OBJECT (self), "fullscreen");
        }
    }
  else
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_UNKNOWN_PROPERTY,
                   "Unknown property \"%s\"", property_name);
      return FALSE;
    }

  emit_property_changed (self, property_name, value);
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
                 signals [PLAYER_METHOD], 0,
                 method_name, parameters);
  g_dbus_method_invocation_return_value (invocation, g_variant_new ("()"));
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
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "CanGoNext") == 0)
    {
      value = g_variant_new_boolean ((self->flags & VALENT_MEDIA_ACTION_NEXT) != 0);
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "CanGoPrevious") == 0)
    {
      value = g_variant_new_boolean ((self->flags & VALENT_MEDIA_ACTION_PREVIOUS) != 0);
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "CanPlay") == 0)
    {
      value = g_variant_new_boolean ((self->flags & VALENT_MEDIA_ACTION_PLAY) != 0);
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "CanPause") == 0)
    {
      value = g_variant_new_boolean ((self->flags & VALENT_MEDIA_ACTION_PAUSE) != 0);
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "CanSeek") == 0)
    {
      value = g_variant_new_boolean ((self->flags & VALENT_MEDIA_ACTION_SEEK) != 0);
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "Metadata") == 0)
    {
      if (self->metadata != NULL)
        return g_variant_ref (self->metadata);

      return NULL;
    }

  if (g_strcmp0 (property_name, "Position") == 0)
    return g_variant_new_double (self->position);

  if (g_strcmp0 (property_name, "Volume") == 0)
    {
      value = g_variant_new_double (self->volume);
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }


  /* Uncommon properties */
  if (g_strcmp0 (property_name, "LoopStatus") == 0)
    {
      value = g_variant_new_string (self->loop_status);
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "PlaybackStatus") == 0)
    {
      value = g_variant_new_string (self->playback_status);
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

      return g_variant_ref_sink (value);
    }

  if (g_strcmp0 (property_name, "Shuffle") == 0)
    {
      value = g_variant_new_boolean ((self->state & VALENT_MEDIA_STATE_SHUFFLE) != 0);
      g_hash_table_replace (self->cache, g_strdup (property_name), g_variant_ref_sink (value));

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

      if ((self->state & VALENT_MEDIA_STATE_REPEAT) != 0 &&
          g_strcmp0 (loop_status, "Track") == 0)
        return TRUE;

      if ((self->state & VALENT_MEDIA_STATE_REPEAT_ALL) != 0 &&
          g_strcmp0 (loop_status, "Playlist") == 0)
        return TRUE;

      if (g_strcmp0 (loop_status, "None") == 0)
        return TRUE;
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
  else
    {
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_UNKNOWN_PROPERTY,
                   "Unknown property \"%s\"", property_name);
      return FALSE;
    }

  g_signal_emit (G_OBJECT (self), signals [SET_PROPERTY], 0, property_name, value);

  return TRUE;
}

/*
 * ValentMediaPayer
 */
static const char *
valent_mpris_remote_get_name (ValentMediaPlayer *player)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (player);

  return self->identity;
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
  GVariant *properties;
  GVariantBuilder changed_props;
  GVariantBuilder invalidated_props;
  GHashTableIter iter;
  GVariant *value;
  char *prop_name;

  g_variant_builder_init (&changed_props, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_init (&invalidated_props, G_VARIANT_TYPE_STRING_ARRAY);

  g_hash_table_iter_init (&iter, self->player_properties);

  while (g_hash_table_iter_next (&iter, (void**) &prop_name, (void**) &value))
    {
      if (value)
        g_variant_builder_add (&changed_props, "{sv}", prop_name, value);
      else
        g_variant_builder_add (&invalidated_props, "s", prop_name);
    }

  properties = g_variant_new ("(s@a{sv}@as)",
                              "org.mpris.MediaPlayer2.Player", // FIXME
                              g_variant_builder_end (&changed_props),
                              g_variant_builder_end (&invalidated_props));

  g_dbus_connection_emit_signal (self->connection,
                                 NULL, /* bus name */
                                 "/org/mpris/MediaPlayer2",
                                 "org.freedesktop.DBus.Properties",
                                 "PropertiesChanged",
                                 properties,
                                 NULL /* error */);

  g_hash_table_remove_all (self->player_properties);
  g_clear_handle_id (&self->idle_id, g_source_remove);
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
                                           node_info->interfaces[0],
                                           &self->application_vtable,
                                           self, NULL,
                                           error);

      if (self->application_id == 0)
        return FALSE;
    }

  /* Register org.mpris.MediaPlayer2.Player interface */
  if (self->player_id == 0)
    {
      self->player_id =
        g_dbus_connection_register_object (self->connection,
                                           "/org/mpris/MediaPlayer2",
                                           node_info->interfaces[1],
                                           &self->player_vtable,
                                           self, NULL,
                                           error);

      if (self->player_id == 0)
        return FALSE;
    }

  /* Own a well-known name on the connection */
  if (self->name_id == 0)
    {
      self->name_id =
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
static GVariant *
valent_mpris_remote_get_metadata (ValentMediaPlayer *player)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (player);

  return self->metadata;
}

/*
 * GObject
 */
static void
valent_mpris_remote_dispose (GObject *object)
{
  ValentMprisRemote *self = VALENT_MPRIS_REMOTE (object);

  g_clear_handle_id (&self->idle_id, g_source_remove);
  g_clear_handle_id (&self->name_id, g_bus_unown_name);

  if (self->application_id > 0)
    {
      g_dbus_connection_unregister_object (self->connection, self->application_id);
      self->application_id = 0;
    }

  if (self->player_id > 0)
    {
      g_dbus_connection_unregister_object (self->connection, self->player_id);
      self->player_id = 0;
    }

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
  g_clear_pointer (&self->playback_status, g_free);
  g_clear_pointer (&self->metadata, g_variant_unref);

  g_clear_pointer (&self->cache, g_hash_table_unref);
  g_clear_pointer (&self->player_properties, g_hash_table_unref);

  G_OBJECT_CLASS (valent_mpris_remote_parent_class)->finalize (object);
}

static void
valent_mpris_remote_class_init (ValentMprisRemoteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentMediaPlayerClass *player_class = VALENT_MEDIA_PLAYER_CLASS (klass);

  object_class->dispose = valent_mpris_remote_dispose;
  object_class->finalize = valent_mpris_remote_finalize;

  player_class->get_name = valent_mpris_remote_get_name;
  player_class->get_metadata = valent_mpris_remote_get_metadata;

  /**
   * ValentMprisRemote::player-method:
   * @player: a #ValentMprisRemote
   * @method_name: the method name
   * @method_args: the method arguments
   *
   * #ValentMprisRemote::player-method is emitted when a method is called by a
   * consumer of the exported interface.
   */
  signals [PLAYER_METHOD] =
    g_signal_new ("player-method",
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

  init_node_info ();
}

static void
valent_mpris_remote_init (ValentMprisRemote *self)
{
  GVariant *rate;
  
  valent_mpris_remote_set_name (self, "Media Player");

  self->loop_status = g_strdup ("None");
  self->playback_status = g_strdup ("Stopped");
  self->volume = 1.0;

  self->application_vtable.method_call = application_method_call;
  self->application_vtable.get_property = application_get_property;
  self->application_vtable.set_property = application_set_property;

  self->player_vtable.method_call = player_method_call;
  self->player_vtable.get_property = player_get_property;
  self->player_vtable.set_property = player_set_property;

  self->cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                       g_free, (GDestroyNotify)g_variant_unref);
  self->player_properties = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                   g_free, (GDestroyNotify)g_variant_unref);

  /* Static values */
  rate = g_variant_new_double (1.0);
  g_hash_table_insert (self->cache, g_strdup ("Rate"), g_variant_ref_sink (rate));
  g_hash_table_insert (self->cache, g_strdup ("MaximumRate"), g_variant_ref_sink (rate));
  g_hash_table_insert (self->cache, g_strdup ("MinimumRate"), g_variant_ref_sink (rate));
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
  init_node_info ();

  return g_object_new (VALENT_TYPE_MPRIS_REMOTE,
                       NULL);
}

static void
new_connection_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  g_autoptr (GError) error = NULL;
  ValentMprisRemote *self;

  self = g_task_get_source_object (task);
  self->connection = g_dbus_connection_new_for_address_finish (result, &error);

  if (self->connection == NULL)
    return g_task_return_error (task, g_steal_pointer (&error));

  if (!valent_mpris_remote_register (self, &error))
    return g_task_return_error (task, g_steal_pointer (&error));

  g_task_return_boolean (task, TRUE);
}

/**
 * valent_media_player_export:
 * @remote: a #ValentMprisRemote
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Export the test media player on the session bus.
 */
void
valent_mpris_remote_export (ValentMprisRemote   *remote,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *address = NULL;

  g_return_if_fail (VALENT_IS_MPRIS_REMOTE (remote));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (remote, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mpris_remote_export);

  if (remote->name_id > 0)
    return g_task_return_boolean (task, TRUE);

  /* We already have a connection */
  if (remote->connection != NULL)
    {
      if (!valent_mpris_remote_register (remote, &error))
        return g_task_return_error (task, g_steal_pointer (&error));

      return g_task_return_boolean (task, TRUE);
    }

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
                                     (GAsyncReadyCallback)new_connection_cb,
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

  g_clear_handle_id (&remote->name_id, g_bus_unown_name);
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
  g_autofree char *guid = NULL;

  g_return_if_fail (VALENT_IS_MPRIS_REMOTE (remote));
  g_return_if_fail (identity != NULL);

  g_clear_pointer (&remote->identity, g_free);
  remote->identity = g_strdup (identity);

  g_clear_pointer (&remote->bus_name, g_free);
  guid = g_dbus_generate_guid();
  remote->bus_name = g_strdup_printf ("org.mpris.MediaPlayer2.Valent_%s", guid);
}

/**
 * valent_media_player_set_bus_name:
 * @remote: a #ValentMprisRemote
 * @name: a well-known name
 *
 * Set the well-known name of the player when exported on DBus to @name.
 */
void
valent_mpris_remote_set_bus_name (ValentMprisRemote *remote,
                                  const char        *name)
{
  g_return_if_fail (VALENT_IS_MPRIS_REMOTE (remote));
  g_return_if_fail (g_dbus_is_name (name));

  g_clear_pointer (&remote->bus_name, g_free);
  remote->bus_name = g_strdup (name);
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
    g_warning ("Emitting \"Seeked\": %s", error->message);
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
  ValentMediaActions mask;

  g_return_if_fail (VALENT_IS_MPRIS_REMOTE (remote));

  mask = remote->flags ^ flags;
  remote->flags = flags;

  if (mask & VALENT_MEDIA_ACTION_NEXT)
    emit_property_changed (remote, "CanGoNext",
                           g_variant_new_boolean (flags & VALENT_MEDIA_ACTION_NEXT));

  if (mask & VALENT_MEDIA_ACTION_PREVIOUS)
    emit_property_changed (remote, "CanGoPrevious",
                           g_variant_new_boolean (flags & VALENT_MEDIA_ACTION_PREVIOUS));

  if (mask & VALENT_MEDIA_ACTION_PAUSE)
    emit_property_changed (remote, "CanPause",
                           g_variant_new_boolean (flags & VALENT_MEDIA_ACTION_PAUSE));

  if (mask & VALENT_MEDIA_ACTION_PLAY)
    emit_property_changed (remote, "CanPlay",
                           g_variant_new_boolean (flags & VALENT_MEDIA_ACTION_PLAY));

  if (mask & VALENT_MEDIA_ACTION_SEEK)
    emit_property_changed (remote, "CanSeek",
                           g_variant_new_boolean (flags & VALENT_MEDIA_ACTION_SEEK));


  /* --- */
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
  emit_property_changed (remote, "Metadata", value);
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

  if (g_strcmp0 (remote->playback_status, status) == 0)
    return;

  g_clear_pointer (&remote->playback_status, g_free);
  remote->playback_status = g_strdup (status);
  emit_property_changed (remote, "PlaybackStatus", g_variant_new_string (status));
}

/**
 * valent_mpris_remote_update_rate:
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
  emit_property_changed (remote, "Volume", g_variant_new_double (volume));
}

