// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-telephony-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-media.h>
#include <libvalent-mixer.h>

#include "valent-telephony-plugin.h"


struct _ValentTelephonyPlugin
{
  PeasExtensionBase  parent_instance;

  ValentDevice      *device;
  GSettings         *settings;

  gpointer           prev_input;
  gpointer           prev_output;
  gboolean           prev_media_paused;
};

static void valent_device_plugin_iface_init (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentTelephonyPlugin, valent_telephony_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};


/*
 * Stream Helpers
 */
typedef struct
{
  GWeakRef object;
  double   volume;
  guint    muted : 1;
} StreamState;

static StreamState *
stream_state_new (ValentMixerStream *stream)
{
  StreamState *state;

  state = g_new0 (StreamState, 1);
  g_weak_ref_init (&state->object, stream);
  state->volume = valent_mixer_stream_get_level (stream);
  state->muted = valent_mixer_stream_get_muted (stream);

  return state;
}

static void
stream_state_reset (gpointer data)
{
  StreamState *state = data;
  g_autoptr (ValentMixerStream) stream = NULL;

  stream = g_weak_ref_get (&state->object);

  if (stream != NULL)
    {
      valent_mixer_stream_set_level (stream, state->volume);
      valent_mixer_stream_set_muted (stream, state->muted);
    }
}

static void
stream_state_free (gpointer data)
{
  StreamState *state = data;

  g_weak_ref_clear (&state->object);
  state->volume = 0;
  state->muted = 0;
  g_free (state);
}

/**
 * <private>
 * reset_media_state:
 * @self: a #ValentTelephonyPlugin
 *
 * Reset the input/output volume and MPRIS play state as changed by
 * set_media_state().
 */
static void
reset_media_state (ValentTelephonyPlugin *self)
{
  g_assert (VALENT_IS_TELEPHONY_PLUGIN (self));

  /* Output (eg. speakers) */
  if (self->prev_output)
    {
      stream_state_reset (self->prev_output);
      g_clear_pointer (&self->prev_output, stream_state_free);
    }

  /* Input (eg. microphone) */
  if (self->prev_input)
    {
      stream_state_reset (self->prev_input);
      g_clear_pointer (&self->prev_input, stream_state_free);
    }

  if (self->prev_media_paused)
    {
      ValentMedia *media;

      media = valent_media_get_default ();
      valent_media_unpause (media);
      self->prev_media_paused = FALSE;
    }
}

/**
 * <private>
 * set_media_state:
 * @self: a #ValentTelephonyPlugin
 * @event: a telephony event ('ringing' or 'talking')
 *
 * Set the input/output volume and pause MPRIS media players based on user
 * settings.
 */
static void
set_media_state (ValentTelephonyPlugin *self,
                 const char            *event)
{
  ValentMixer *mixer;
  ValentMixerStream *stream;
  g_autofree char *vol_key = NULL;
  g_autofree char *mic_key = NULL;
  gint vol_val, mic_val;
  g_autofree char *pause_key = NULL;
  gboolean pause;

  g_assert (VALENT_IS_TELEPHONY_PLUGIN (self));
  g_assert (event != NULL);

  mixer = valent_mixer_get_default ();

  /* PulseAudio Volume/Microphone */
  vol_key = g_strconcat (event, "-volume", NULL);
  mic_key = g_strconcat (event, "-microphone", NULL);
  vol_val = g_settings_get_int (self->settings, vol_key);
  mic_val = g_settings_get_int (self->settings, mic_key);

  /* Output (eg. speakers) */
  if (vol_val > 0)
    {
      stream = valent_mixer_get_default_output (mixer);

      if (stream != NULL)
        {
          self->prev_output = stream_state_new (stream);
          valent_mixer_stream_set_level (stream, vol_val);
        }
    }
  else if (vol_val > -1)
    {
      stream = valent_mixer_get_default_output (mixer);

      if (stream != NULL)
        {
          self->prev_output = stream_state_new (stream);
          valent_mixer_stream_set_muted (stream, TRUE);
        }
    }

  /* Input (eg. microphone) */
  if (mic_val > 0)
    {
      stream = valent_mixer_get_default_input (mixer);

      if (stream != NULL)
        {
          self->prev_input = stream_state_new (stream);
          valent_mixer_stream_set_level (stream, mic_val);
        }
    }
  else if (mic_val > -1)
    {
      stream = valent_mixer_get_default_input (mixer);

      if (stream != NULL)
        {
          self->prev_input = stream_state_new (stream);
          valent_mixer_stream_set_muted (stream, TRUE);
        }
    }

  /* MPRIS Play State */
  pause_key = g_strconcat (event, "-pause", NULL);
  pause = g_settings_get_boolean (self->settings, pause_key);

  if (pause)
    {
      ValentMedia *media;

      media = valent_media_get_default ();
      valent_media_pause (media);
      self->prev_media_paused = TRUE;
    }
}

/**
 * <private>
 * get_event_icon:
 * @pbody: a #JsonObject body from a telephony packet
 *
 * Return a #GIcon for a telephony notification, created from a base64 encoded
 * avatar if available.
 *
 * Returns: (transfer full): a new #GIcon
 */
static GIcon *
get_event_icon (JsonObject *pbody)
{
  g_assert (pbody != NULL);

  if (json_object_has_member (pbody, "phoneThumbnail"))
    {
      g_autoptr (GdkPixbufLoader) loader = NULL;
      GdkPixbuf *pixbuf = NULL;
      const char *text;
      g_autofree guchar *data = NULL;
      gsize dlen;

      text = json_object_get_string_member (pbody, "phoneThumbnail");
      data = g_base64_decode (text, &dlen);

      /* Load the icon, but ignore errors as they're often partially corrupt */
      loader = gdk_pixbuf_loader_new();

      if (gdk_pixbuf_loader_write (loader, data, dlen, NULL) &&
          gdk_pixbuf_loader_close (loader, NULL))
        pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

      if (pixbuf != NULL)
        return G_ICON (g_object_ref (pixbuf));
    }

  return g_themed_icon_new ("call-start-symbolic");
}

static void
valent_telephony_plugin_handle_telephony (ValentTelephonyPlugin *self,
                                          JsonNode              *packet)
{
  JsonObject *body;
  const char *event;
  const char *sender;
  gboolean is_ringing = FALSE;
  gboolean is_talking = FALSE;
  g_autoptr (GNotification) notif = NULL;
  g_autoptr (GIcon) icon = NULL;
  g_autofree char *id = NULL;

  g_assert (VALENT_IS_TELEPHONY_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  body = valent_packet_get_body (packet);
  event = json_object_get_string_member (body, "event");

  /* Sender*/
  if (json_object_has_member (body, "contactName"))
    sender = json_object_get_string_member (body, "contactName");
  else if (json_object_has_member (body, "phoneNumber"))
    sender = json_object_get_string_member (body, "phoneNumber");
  else
    sender = _("Unknown Contact");

  /* Check the event type */
  if (g_strcmp0 (event, "ringing") == 0)
    is_ringing = TRUE;
  else if (g_strcmp0 (event, "talking") == 0)
    is_talking = TRUE;
  else
    {
      g_warning ("Unknown telephony event: %s", event);
      return;
    }

  /* Inject the event type into the notification ID, to distinguish them */
  id = g_strdup_printf ("%s|%s", event, sender);

  /* This is a cancelled event */
  if (json_object_has_member (body, "isCancel"))
    {
      reset_media_state (self);
      valent_device_hide_notification (self->device, id);
      return;
    }

  /* Adjust volume/pause media */
  set_media_state (self, event);

  /* Notify user */
  notif = g_notification_new (sender);
  icon = get_event_icon (body);
  g_notification_set_icon (notif, icon);

  if (is_ringing)
    {
      g_notification_set_body (notif, _("Incoming call"));
      valent_notification_add_device_button (notif,
                                             self->device,
                                             _("Mute"),
                                             "mute-call",
                                             NULL);
      g_notification_set_priority (notif, G_NOTIFICATION_PRIORITY_URGENT);
    }
  else if (is_talking)
    {
      g_notification_set_body (notif, _("Ongoing call"));
    }

  valent_device_show_notification (self->device, id, notif);
}

static void
valent_telephony_plugin_mute_call (ValentTelephonyPlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_TELEPHONY_PLUGIN (self));

  builder = valent_packet_start("kdeconnect.telephony.request_mute");
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

/*
 * GActions
 */
static void
mute_action (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  ValentTelephonyPlugin *self = VALENT_TELEPHONY_PLUGIN (user_data);

  g_assert (VALENT_IS_TELEPHONY_PLUGIN (self));

  valent_telephony_plugin_mute_call (self);
}

static const GActionEntry actions[] = {
    {"mute-call", mute_action, NULL, NULL, NULL}
};

/*
 * ValentDevicePlugin
 */
static void
valent_telephony_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentTelephonyPlugin *self = VALENT_TELEPHONY_PLUGIN (plugin);
  const char *device_id;

  g_assert (VALENT_IS_TELEPHONY_PLUGIN (self));

  /* Setup GSettings */
  device_id = valent_device_get_id (self->device);
  self->settings = valent_device_plugin_new_settings (device_id, "telephony");

  /* Register GActions */
  valent_device_plugin_register_actions (plugin,
                                         actions,
                                         G_N_ELEMENTS (actions));
}

static void
valent_telephony_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentTelephonyPlugin *self = VALENT_TELEPHONY_PLUGIN (plugin);

  /* Unregister GActions */
  valent_device_plugin_unregister_actions (plugin,
                                           actions,
                                           G_N_ELEMENTS (actions));

  /* Dispose GSettings */
  g_clear_object (&self->settings);
}

static void
valent_telephony_plugin_update_state (ValentDevicePlugin *plugin)
{
  ValentTelephonyPlugin *self = VALENT_TELEPHONY_PLUGIN (plugin);
  gboolean connected;
  gboolean paired;
  gboolean available;

  connected = valent_device_get_connected (self->device);
  paired = valent_device_get_paired (self->device);
  available = (connected && paired);

  /* GActions */
  valent_device_plugin_toggle_actions (plugin,
                                       actions, G_N_ELEMENTS (actions),
                                       available);
}

static void
valent_telephony_plugin_handle_packet (ValentDevicePlugin *plugin,
                                       const char         *type,
                                       JsonNode           *packet)
{
  ValentTelephonyPlugin *self = VALENT_TELEPHONY_PLUGIN (plugin);

  g_assert (VALENT_IS_TELEPHONY_PLUGIN (plugin));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  if (g_strcmp0 (type, "kdeconnect.telephony") == 0)
    valent_telephony_plugin_handle_telephony (self, packet);
  else
    g_assert_not_reached ();
}

static void
valent_device_plugin_iface_init (ValentDevicePluginInterface *iface)
{
  iface->enable = valent_telephony_plugin_enable;
  iface->disable = valent_telephony_plugin_disable;
  iface->handle_packet = valent_telephony_plugin_handle_packet;
  iface->update_state = valent_telephony_plugin_update_state;
}

/*
 * GObject
 */
static void
valent_telephony_plugin_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ValentTelephonyPlugin *self = VALENT_TELEPHONY_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_telephony_plugin_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ValentTelephonyPlugin *self = VALENT_TELEPHONY_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_telephony_plugin_class_init (ValentTelephonyPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_telephony_plugin_get_property;
  object_class->set_property = valent_telephony_plugin_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
}

static void
valent_telephony_plugin_init (ValentTelephonyPlugin *self)
{
}

