// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "vdp-mpris-adapter"

#include "config.h"

#include <gio/gio.h>
#include <valent.h>

#include "vdp-mpris-player.h"

#include "vdp-mpris-adapter.h"

struct _VdpMprisAdapter
{
  ValentMediaAdapter  parent_instance;

  ValentDevice       *device;
  GCancellable       *cancellable;
  GHashTable         *players;
  GHashTable         *transfers;
};

G_DEFINE_FINAL_TYPE (VdpMprisAdapter, vdp_mpris_adapter, VALENT_TYPE_MEDIA_ADAPTER)

static inline void
_valent_object_deref (gpointer data)
{
  if (!valent_object_in_destruction (VALENT_OBJECT (data)))
    valent_object_destroy (VALENT_OBJECT (data));

  g_object_unref (data);
}

static inline void
valent_device_send_packet_cb (ValentDevice *device,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  g_autoptr (GError) error = NULL;

  if (!valent_device_send_packet_finish (device, result, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED))
        g_critical ("%s(): %s", G_STRFUNC, error->message);
      else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_CONNECTED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);
      else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("%s(): %s", G_STRFUNC, error->message);
    }
}

static void
receive_art_cb (ValentTransfer  *transfer,
                GAsyncResult    *result,
                VdpMprisAdapter *self)
{
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GError) error = NULL;
  ValentMediaPlayer *player = NULL;
  const char *name;

  if (!valent_transfer_execute_finish (transfer, result, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  g_object_get (transfer,
                "file",   &file,
                "packet", &packet,
                NULL);

  if (valent_packet_get_string (packet, "player", &name))
    player = g_hash_table_lookup (self->players, name);;

  if (player != NULL)
    vdp_mpris_player_update_art (VDP_MPRIS_PLAYER (player), file);
}

static void
vdp_mpris_adapter_receive_album_art (VdpMprisAdapter *self,
                                     JsonNode        *packet)
{
  ValentContext *context = NULL;
  const char *url;
  g_autofree char *filename = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (ValentTransfer) transfer = NULL;

  if (!valent_packet_get_string (packet, "albumArtUrl", &url))
    {
      g_debug ("%s(): expected \"albumArtUrl\" field holding a string",
               G_STRFUNC);
      return;
    }

  context = valent_device_get_context (self->device);
  filename = g_compute_checksum_for_string (G_CHECKSUM_MD5, url, -1);
  file = valent_context_get_cache_file (context, filename);

  transfer = valent_device_transfer_new (self->device, packet, file);
  valent_transfer_execute (transfer,
                           self->cancellable,
                           (GAsyncReadyCallback)receive_art_cb,
                           self);
}

/*< private >
 * @self: a `VdpMprisAdapter`
 * @player: a
 *
 * Send a request for messages starting at @range_start_timestamp in
 * oldest-to-newest order, for a maximum of @number_to_request.
 */
static void
vdp_mpris_adapter_request_player_list (VdpMprisAdapter *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  valent_packet_init (&builder, "kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "requestPlayerList");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device,
                             packet,
                             self->cancellable,
                             (GAsyncReadyCallback) valent_device_send_packet_cb,
                             NULL);
}

/*< private >
 * @self: a `VdpMprisAdapter`
 * @player: a player name
 *
 * Request an update for for @player.
 */
static void
vdp_mpris_adapter_request_update (VdpMprisAdapter *self,
                                  const char      *player)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VDP_IS_MPRIS_ADAPTER (self));
  g_assert (player != NULL);

  valent_packet_init (&builder, "kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, player);
  json_builder_set_member_name (builder, "requestNowPlaying");
  json_builder_add_boolean_value (builder, TRUE);
  json_builder_set_member_name (builder, "requestVolume");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device,
                             packet,
                             self->cancellable,
                             (GAsyncReadyCallback) valent_device_send_packet_cb,
                             NULL);
}

/*
 * ValentMediaAdapter
 */
static void
on_device_state_changed (ValentDevice    *device,
                         GParamSpec      *pspec,
                         VdpMprisAdapter *self)
{
  ValentDeviceState state = VALENT_DEVICE_STATE_NONE;
  gboolean available;

  state = valent_device_get_state (device);
  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  if (available && self->cancellable == NULL)
    {
      self->cancellable = g_cancellable_new ();
      vdp_mpris_adapter_request_player_list (self);
    }
  else if (!available && self->cancellable != NULL)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
      g_hash_table_remove_all (self->players);
    }
}

/*
 * GObject
 */
static void
vdp_mpris_adapter_constructed (GObject *object)
{
  VdpMprisAdapter *self = VDP_MPRIS_ADAPTER (object);

  G_OBJECT_CLASS (vdp_mpris_adapter_parent_class)->constructed (object);

  self->device = valent_resource_get_source (VALENT_RESOURCE (self));
  g_signal_connect_object (self->device,
                           "notify::state",
                           G_CALLBACK (on_device_state_changed),
                           self,
                           G_CONNECT_DEFAULT);
  on_device_state_changed (self->device, NULL, self);
}

static void
vdp_mpris_adapter_finalize (GObject *object)
{
  VdpMprisAdapter *self = VDP_MPRIS_ADAPTER (object);

  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->players, g_hash_table_unref);
  g_clear_pointer (&self->transfers, g_hash_table_unref);

  G_OBJECT_CLASS (vdp_mpris_adapter_parent_class)->finalize (object);
}

static void
vdp_mpris_adapter_class_init (VdpMprisAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = vdp_mpris_adapter_constructed;
  object_class->finalize = vdp_mpris_adapter_finalize;
}

static void
vdp_mpris_adapter_init (VdpMprisAdapter *self)
{
  self->players = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         _valent_object_deref);
  self->transfers = g_hash_table_new_full (g_str_hash,
                                           g_str_equal,
                                           g_free,
                                           g_object_unref);
}

/**
 * vdp_mpris_adapter_new:
 * @device: a `ValentDevice`
 *
 * Create a new `VdpMprisAdapter`.
 *
 * Returns: (transfer full): a new message store
 */
ValentMediaAdapter *
vdp_mpris_adapter_new (ValentDevice *device)
{
  g_autoptr (ValentContext) context = NULL;
  g_autofree char *iri = NULL;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  context = valent_context_new (valent_device_get_context (device),
                                "plugin",
                                "mpris");
  iri = tracker_sparql_get_uuid_urn ();
  return g_object_new (VDP_TYPE_MPRIS_ADAPTER,
                       "iri",     iri,
                       "context", context,
                       "source",  device,
                       "title",   valent_device_get_name (device),
                       NULL);
}

static void
vdp_mpris_adapter_handle_player_list (VdpMprisAdapter *self,
                                      JsonArray       *player_list)
{
  GHashTableIter iter;
  VdpMprisPlayer *player;
  const char *name;
  unsigned int n_remote = 0;
  g_autofree const char **remote_names = NULL;

  g_assert (VDP_IS_MPRIS_ADAPTER (self));
  g_assert (player_list != NULL);

#ifndef __clang_analyzer__
  /* Collect the remote player names */
  n_remote = json_array_get_length (player_list);
  remote_names = g_new0 (const char *, n_remote + 1);

  for (unsigned int i = 0; i < n_remote; i++)
    remote_names[i] = json_array_get_string_element (player_list, i);

  /* Remove old players */
  g_hash_table_iter_init (&iter, self->players);
  while (g_hash_table_iter_next (&iter, (void **)&name, (void **)&player))
    {
      if (!g_strv_contains (remote_names, name))
        g_hash_table_iter_remove (&iter);
    }

  /* Add new players */
  for (unsigned int i = 0; remote_names[i] != NULL; i++)
    {
      name = remote_names[i];
      if (g_hash_table_contains (self->players, name))
        continue;

      player = vdp_mpris_player_new (self->device);
      vdp_mpris_player_update_name (player, name);

      valent_media_adapter_player_added (VALENT_MEDIA_ADAPTER (self),
                                         VALENT_MEDIA_PLAYER (player));
      valent_media_export_player (valent_media_get_default (),
                                  VALENT_MEDIA_PLAYER (player));
      g_hash_table_insert (self->players,
                           g_strdup (name),
                           g_steal_pointer (&player));

      vdp_mpris_adapter_request_update (self, name);
    }
#endif /* __clang_analyzer__ */
}

static void
vdp_mpris_adapter_handle_player_update (VdpMprisAdapter *self,
                                        JsonNode        *packet)
{
  ValentMediaPlayer *player = NULL;
  const char *name;

  /* Get the remote */
  if (valent_packet_get_string (packet, "player", &name))
    player = g_hash_table_lookup (self->players, name);

  if (player == NULL)
    {
      vdp_mpris_adapter_request_player_list (self);
      return;
    }

  if (valent_packet_check_field (packet, "transferringAlbumArt"))
    {
      vdp_mpris_adapter_receive_album_art (self, packet);
      return;
    }

  vdp_mpris_player_handle_packet (VDP_MPRIS_PLAYER (player), packet);
}

/**
 * vdp_mpris_adapter_handle_packet:
 * @self: a `VdpMprisAdapter`
 * @packet: a `kdeconnect.sms.attachment_file` packet
 *
 * Handle an attachment file.
 */
void
vdp_mpris_adapter_handle_packet (VdpMprisAdapter *self,
                                 JsonNode        *packet)
{
  JsonArray *player_list;

  g_assert (VDP_IS_MPRIS_ADAPTER (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (valent_packet_get_array (packet, "playerList", &player_list))
    vdp_mpris_adapter_handle_player_list (self, player_list);
  else if (valent_packet_get_string (packet, "player", NULL))
    vdp_mpris_adapter_handle_player_update (self, packet);
  else
    g_assert_not_reached ();
}

