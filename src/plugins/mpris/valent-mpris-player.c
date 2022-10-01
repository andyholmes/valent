// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mpris-player"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-media.h>

#include "valent-mpris-player.h"
#include "valent-mpris-utils.h"


struct _ValentMPRISPlayer
{
  ValentMediaPlayer   parent_instance;

  char               *bus_name;
  GDBusProxy         *application;
  GDBusProxy         *player;
  unsigned int        no_position : 1;
  gint64              position;

  ValentMediaActions  flags;
};

static void valent_mpris_player_sync_flags (ValentMPRISPlayer *self);

static void async_initable_iface_init      (GAsyncInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentMPRISPlayer, valent_mpris_player, VALENT_TYPE_MEDIA_PLAYER,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

enum {
  PROP_0,
  PROP_BUS_NAME,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * DBus Property Mapping
 */
typedef struct
{
  const char *dbus;
  const char *name;
} PropMapping;

static const PropMapping player_properties[] = {
  {"CanControl",     "flags"},
  {"CanGoNext",      "flags"},
  {"CanGoPrevious",  "flags"},
  {"CanPause",       "flags"},
  {"CanPlay",        "flags"},
  {"CanSeek",        "flags"},
  {"Metadata",       "metadata"},
  {"LoopStatus",     "repeat"},
  {"PlaybackStatus", "state"},
  {"Position",       "position"},
  {"Shuffle",        "shuffle"},
  {"Volume",         "volume"},
};


/* For convenience, we use our object's ::notify signal to forward each proxy's
 * GDBusProxy::g-properties-changed signal.
 */
static void
on_application_properties_changed (GDBusProxy        *application,
                                   GVariant          *changed_properties,
                                   GStrv              invalidated_properties,
                                   ValentMediaPlayer *player)
{
  GVariantDict dict;

  g_assert (VALENT_IS_MEDIA_PLAYER (player));

  g_variant_dict_init (&dict, changed_properties);

  if (g_variant_dict_contains (&dict, "Identity"))
    {
      g_object_notify (G_OBJECT (player), "name");
      valent_media_player_emit_changed (player);
    }

  g_variant_dict_clear (&dict);
}

static void
on_player_properties_changed (GDBusProxy        *proxy,
                              GVariant          *changed_properties,
                              GStrv              invalidated_properties,
                              ValentMediaPlayer *player)
{
  GVariantDict dict;

  g_assert (VALENT_IS_MPRIS_PLAYER (player));
  g_assert (changed_properties != NULL);

  g_object_freeze_notify (G_OBJECT (player));
  g_variant_dict_init (&dict, changed_properties);

  for (unsigned int i = 0; i < G_N_ELEMENTS (player_properties); i++)
    {
      if (g_variant_dict_contains (&dict, player_properties[i].dbus))
        g_object_notify (G_OBJECT (player), player_properties[i].name);
    }

  g_variant_dict_clear (&dict);
  g_object_thaw_notify (G_OBJECT (player));

  valent_media_player_emit_changed (player);
}

static void
on_player_signal (GDBusProxy        *proxy,
                  const char        *sender_name,
                  const char        *signal_name,
                  GVariant          *parameters,
                  ValentMediaPlayer *player)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);

  g_assert (VALENT_IS_MPRIS_PLAYER (player));
  g_assert (signal_name != NULL);

  if (strcmp (signal_name, "Seeked") == 0)
    {
      gint64 position_us = 0;

      /* Convert microseconds to milliseconds */
      g_variant_get (parameters, "(x)", &position_us);
      self->position = position_us / 1000L;
      g_object_notify (G_OBJECT (player), "position");
    }
}

/*
 * GAsyncInitable
 */
static void
valent_mpris_player_init_player_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentMPRISPlayer *self = g_task_get_source_object (task);
  GError *error = NULL;

  g_assert (VALENT_IS_MPRIS_PLAYER (self));
  g_assert (self->bus_name != NULL);

  self->player = g_dbus_proxy_new_finish (result, &error);

  if (self->player == NULL)
    return g_task_return_error (task, error);

  g_signal_connect_object (self->player,
                           "g-properties-changed",
                           G_CALLBACK (on_player_properties_changed),
                           self, 0);

  g_signal_connect_object (self->player,
                           "g-signal",
                           G_CALLBACK (on_player_signal),
                           self, 0);

  valent_mpris_player_sync_flags (self);

  g_task_return_boolean (task, TRUE);
}

static void
valent_mpris_player_init_application_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentMPRISPlayer *self = g_task_get_source_object (task);
  GCancellable *cancellable = g_task_get_cancellable (task);
  GError *error = NULL;

  g_assert (VALENT_IS_MPRIS_PLAYER (self));
  g_assert (self->bus_name != NULL);
  g_assert (G_IS_TASK (task));

  self->application = g_dbus_proxy_new_finish (result, &error);

  if (self->application == NULL)
    return g_task_return_error (task, error);

  g_signal_connect (self->application,
                    "g-properties-changed",
                    G_CALLBACK (on_application_properties_changed),
                    self);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                            VALENT_MPRIS_PLAYER_INFO,
                            self->bus_name,
                            "/org/mpris/MediaPlayer2",
                            "org.mpris.MediaPlayer2.Player",
                            cancellable,
                            valent_mpris_player_init_player_cb,
                            g_steal_pointer (&task));
}

static void
valent_mpris_player_init_async (GAsyncInitable      *initable,
                                int                  io_priority,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (initable);
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (VALENT_IS_MPRIS_PLAYER (self));
  g_return_if_fail (self->bus_name != NULL);

  task = g_task_new (initable, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mpris_player_init_async);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                            VALENT_MPRIS_APPLICATION_INFO,
                            self->bus_name,
                            "/org/mpris/MediaPlayer2",
                            "org.mpris.MediaPlayer2",
                            cancellable,
                            valent_mpris_player_init_application_cb,
                            g_steal_pointer (&task));
}

static gboolean
valent_mpris_player_init_finish (GAsyncInitable  *initable,
                                 GAsyncResult    *result,
                                 GError         **error)
{
  g_return_val_if_fail (g_task_is_valid (result, initable), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = valent_mpris_player_init_async;
  iface->init_finish = valent_mpris_player_init_finish;
}

static void
valent_mpris_player_sync_flags (ValentMPRISPlayer *self)
{
  g_autoptr (GVariant) value = NULL;

  // TODO: Controllable
  value = g_dbus_proxy_get_cached_property (self->player, "CanControl");

  if (value && !g_variant_get_boolean (value))
    self->flags = VALENT_MEDIA_ACTION_NONE;

  g_clear_pointer (&value, g_variant_unref);

  // Next
  value = g_dbus_proxy_get_cached_property (self->player, "CanGoNext");

  if (value && g_variant_get_boolean (value))
    self->flags |= VALENT_MEDIA_ACTION_NEXT;
  else
    self->flags &= ~VALENT_MEDIA_ACTION_NEXT;

  g_clear_pointer (&value, g_variant_unref);

  // Previous
  value = g_dbus_proxy_get_cached_property (self->player, "CanGoPrevious");

  if (value && g_variant_get_boolean (value))
    self->flags |= VALENT_MEDIA_ACTION_PREVIOUS;
  else
    self->flags &= ~VALENT_MEDIA_ACTION_PREVIOUS;

  g_clear_pointer (&value, g_variant_unref);

  // Pause
  value = g_dbus_proxy_get_cached_property (self->player, "CanPause");

  if (value && g_variant_get_boolean (value))
    self->flags |= VALENT_MEDIA_ACTION_PAUSE;
  else
    self->flags &= ~VALENT_MEDIA_ACTION_PAUSE;

  g_clear_pointer (&value, g_variant_unref);

  // Play
  value = g_dbus_proxy_get_cached_property (self->player, "CanPlay");

  if (value && g_variant_get_boolean (value))
    self->flags |= VALENT_MEDIA_ACTION_PLAY;
  else
    self->flags &= ~VALENT_MEDIA_ACTION_PLAY;

  g_clear_pointer (&value, g_variant_unref);

  // Seek
  value = g_dbus_proxy_get_cached_property (self->player, "CanSeek");

  if (value && g_variant_get_boolean (value))
    self->flags |= VALENT_MEDIA_ACTION_SEEK;
  else
    self->flags &= ~VALENT_MEDIA_ACTION_SEEK;

  g_clear_pointer (&value, g_variant_unref);
}

/*
 * ValentMediaPlayer
 */
static ValentMediaActions
valent_mpris_player_get_flags (ValentMediaPlayer *player)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);

  return self->flags;
}

static GVariant *
valent_mpris_player_get_metadata (ValentMediaPlayer *player)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);

  return g_dbus_proxy_get_cached_property (self->player, "Metadata");
}

static const char *
valent_mpris_player_get_name (ValentMediaPlayer *player)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);
  g_autoptr (GVariant) value = NULL;

  value = g_dbus_proxy_get_cached_property (self->application, "Identity");

  if G_UNLIKELY (value == NULL)
    return "MPRIS Player";

  return g_variant_get_string (value, NULL);
}

static gint64
valent_mpris_player_get_position (ValentMediaPlayer *player)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);
  g_autoptr (GError) error = NULL;
  g_autoptr (GVariant) result = NULL;
  g_autoptr (GVariant) value = NULL;

  /* Avoid repeated calls for players that don't support this property,
   * particularly web browsers like Mozilla Firefox. */
  if (self->no_position)
    return self->position;

  result = g_dbus_proxy_call_sync (self->player,
                                   "org.freedesktop.DBus.Properties.Get",
                                   g_variant_new ("(ss)",
                                                  "org.mpris.MediaPlayer2.Player",
                                                  "Position"),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   1,
                                   NULL,
                                   &error);

  if (result == NULL)
    {
      if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED))
        self->no_position = TRUE;
      else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT))
        VALENT_TODO ("Unexpected error: G_IO_ERROR_TIMED_OUT");
      else
        g_message ("%s(): %s", G_STRFUNC, error->message);

      return self->position;
    }

  /* Convert microseconds to milliseconds */
  g_variant_get (result, "(v)", &value);
  self->position = g_variant_get_int64 (value) / 1000L;

  return self->position;
}

static void
valent_mpris_player_set_position (ValentMediaPlayer *player,
                                  gint64             position)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);

  /* Avoid repeated calls for players that don't support this property,
   * particularly web browsers like Mozilla Firefox. */
  if (self->no_position)
    return;

  /* Convert milliseconds to microseconds */
  g_dbus_proxy_call (self->player,
                     "SetPosition",
                     g_variant_new ("(ox)", "/", position * 1000L),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     NULL,
                     NULL);
}

static ValentMediaRepeat
valent_mpris_player_get_repeat (ValentMediaPlayer *player)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);
  g_autoptr (GVariant) value = NULL;
  const char *loop_status = NULL;

  value = g_dbus_proxy_get_cached_property (self->player, "LoopStatus");

  if G_UNLIKELY (value == NULL)
    return VALENT_MEDIA_REPEAT_NONE;

  loop_status = g_variant_get_string (value, NULL);

  return valent_mpris_repeat_from_string (loop_status);
}

static void
valent_mpris_player_set_repeat (ValentMediaPlayer *player,
                                ValentMediaRepeat  repeat)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);
  const char *loop_status = valent_mpris_repeat_to_string (repeat);

  g_dbus_proxy_call (self->player,
                     "org.freedesktop.DBus.Properties.Set",
                     g_variant_new ("(ssv)",
                                    "org.mpris.MediaPlayer2.Player",
                                    "LoopStatus",
                                    g_variant_new_string (loop_status)),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     NULL,
                     NULL);
}

static ValentMediaState
valent_mpris_player_get_state (ValentMediaPlayer *player)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);
  g_autoptr (GVariant) value = NULL;
  const char *playback_status = NULL;

  value = g_dbus_proxy_get_cached_property (self->player, "PlaybackStatus");

  if G_UNLIKELY (value == NULL)
    return VALENT_MEDIA_STATE_STOPPED;

  playback_status = g_variant_get_string (value, NULL);

  return valent_mpris_state_from_string (playback_status);
}

static gboolean
valent_mpris_player_get_shuffle (ValentMediaPlayer *player)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);
  g_autoptr (GVariant) value = NULL;

  value = g_dbus_proxy_get_cached_property (self->player, "Shuffle");

  if G_UNLIKELY (value == NULL)
    return FALSE;

  return g_variant_get_boolean (value);
}

static void
valent_mpris_player_set_shuffle (ValentMediaPlayer *player,
                                 gboolean           shuffle)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);

  g_dbus_proxy_call (self->player,
                     "org.freedesktop.DBus.Properties.Set",
                     g_variant_new ("(ssv)",
                                    "org.mpris.MediaPlayer2.Player",
                                    "Shuffle",
                                    g_variant_new_boolean (shuffle)),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     NULL,
                     NULL);
}

static double
valent_mpris_player_get_volume (ValentMediaPlayer *player)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);
  g_autoptr (GVariant) value = NULL;

  value = g_dbus_proxy_get_cached_property (self->player, "Volume");

  if G_UNLIKELY (value == NULL)
    return 1.0;

  return g_variant_get_double (value);
}

static void
valent_mpris_player_set_volume (ValentMediaPlayer *player,
                                double             volume)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);

  g_dbus_proxy_call (self->player,
                     "org.freedesktop.DBus.Properties.Set",
                     g_variant_new ("(ssv)",
                                    "org.mpris.MediaPlayer2.Player",
                                    "Volume",
                                    g_variant_new_double (volume)),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     NULL,
                     NULL);
}

static void
valent_mpris_player_next (ValentMediaPlayer *player)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);

  g_dbus_proxy_call (self->player,
                     "Next",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     NULL,
                     NULL);
}

static void
valent_mpris_player_pause (ValentMediaPlayer *player)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);

  g_dbus_proxy_call (self->player,
                     "Pause",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     NULL,
                     NULL);
}

static void
valent_mpris_player_play (ValentMediaPlayer *player)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);

  g_dbus_proxy_call (self->player,
                     "Play",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     NULL,
                     NULL);
}

static void
valent_mpris_player_play_pause (ValentMediaPlayer *player)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);

  g_dbus_proxy_call (self->player,
                     "PlayPause",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     NULL,
                     NULL);
}

static void
valent_mpris_player_previous (ValentMediaPlayer *player)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);

  g_dbus_proxy_call (self->player,
                     "Previous",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     NULL,
                     NULL);
}

static void
valent_mpris_player_seek (ValentMediaPlayer *player,
                          gint64             offset)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);

  /* Convert milliseconds to microseconds */
  g_dbus_proxy_call (self->player,
                     "Seek",
                     g_variant_new ("(x)", offset * 1000L),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     NULL,
                     NULL);
}

static void
valent_mpris_player_stop (ValentMediaPlayer *player)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (player);

  g_dbus_proxy_call (self->player,
                     "Stop",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     NULL,
                     NULL);
}

/*
 * GObject
 */
static void
valent_mpris_player_finalize (GObject *object)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (object);

  g_clear_pointer (&self->bus_name, g_free);
  g_clear_object (&self->player);
  g_clear_object (&self->application);

  G_OBJECT_CLASS (valent_mpris_player_parent_class)->finalize (object);
}

static void
valent_mpris_player_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (object);

  switch (prop_id)
    {
    case PROP_BUS_NAME:
      g_value_set_string (value, self->bus_name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mpris_player_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (object);

  switch (prop_id)
    {
    case PROP_BUS_NAME:
      self->bus_name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mpris_player_notify (GObject    *object,
                            GParamSpec *pspec)
{
  ValentMPRISPlayer *self = VALENT_MPRIS_PLAYER (object);
  const char *name = g_param_spec_get_name (pspec);

  if (g_str_equal (name, "flags"))
    valent_mpris_player_sync_flags (self);

  if (G_OBJECT_CLASS (valent_mpris_player_parent_class)->notify)
    G_OBJECT_CLASS (valent_mpris_player_parent_class)->notify (object, pspec);
}

static void
valent_mpris_player_class_init (ValentMPRISPlayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentMediaPlayerClass *player_class = VALENT_MEDIA_PLAYER_CLASS (klass);

  object_class->finalize = valent_mpris_player_finalize;
  object_class->get_property = valent_mpris_player_get_property;
  object_class->set_property = valent_mpris_player_set_property;
  object_class->notify = valent_mpris_player_notify;

  player_class->get_flags = valent_mpris_player_get_flags;
  player_class->get_metadata = valent_mpris_player_get_metadata;
  player_class->get_name = valent_mpris_player_get_name;
  player_class->get_position = valent_mpris_player_get_position;
  player_class->set_position = valent_mpris_player_set_position;
  player_class->get_repeat = valent_mpris_player_get_repeat;
  player_class->set_repeat = valent_mpris_player_set_repeat;
  player_class->get_shuffle = valent_mpris_player_get_shuffle;
  player_class->set_shuffle = valent_mpris_player_set_shuffle;
  player_class->get_state = valent_mpris_player_get_state;
  player_class->get_volume = valent_mpris_player_get_volume;
  player_class->set_volume = valent_mpris_player_set_volume;
  player_class->next = valent_mpris_player_next;
  player_class->pause = valent_mpris_player_pause;
  player_class->play = valent_mpris_player_play;
  player_class->play_pause = valent_mpris_player_play_pause;
  player_class->previous = valent_mpris_player_previous;
  player_class->seek = valent_mpris_player_seek;
  player_class->stop = valent_mpris_player_stop;

  /**
   * ValentMPRISPlayer:bus-name:
   *
   * The well-known or unique name that the player is on.
   */
  properties [PROP_BUS_NAME] =
    g_param_spec_string ("bus-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_mpris_player_init (ValentMPRISPlayer *media_player)
{
}

/**
 * valent_mpris_player_new:
 * @name: a well-known or unique bus name
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Create a new MPRISv2 client.
 *
 * Returns: (transfer full): a #ValentMPRISPlayer
 */
void
valent_mpris_player_new (const char          *bus_name,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  g_async_initable_new_async (VALENT_TYPE_MPRIS_PLAYER,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              callback,
                              user_data,
                              "bus-name", bus_name,
                              NULL);
}

/**
 * valent_mpris_player_new_finish:
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started ny [func@Valent.MPRISPlayer.new].
 *
 * Returns: (transfer full) (nullable): a #ValentMPRISPlayer
 */
ValentMPRISPlayer *
valent_mpris_player_new_finish (GAsyncResult  *result,
                                GError       **error)
{
  GObject *ret;
  g_autoptr (GObject) source_object = NULL;

  source_object = g_async_result_get_source_object (result);
  ret = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object),
                                     result,
                                     error);

  if (ret != NULL)
    return VALENT_MPRIS_PLAYER (ret);

  return NULL;
}

