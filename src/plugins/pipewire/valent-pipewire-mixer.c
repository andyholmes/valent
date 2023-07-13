// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-pipewire-mixer"

#include "config.h"

#include <pipewire/pipewire.h>
#include <pipewire/core.h>
#include <pipewire/loop.h>
#include <pipewire/extensions/metadata.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/pod/iter.h>
#include <valent.h>

#include "valent-pipewire-mixer.h"
#include "valent-pipewire-stream.h"

#define MIXER_DEVICE "Audio/Device"
#define MIXER_SINK   "Audio/Sink"
#define MIXER_SOURCE "Audio/Source"


struct _ValentPipewireMixer
{
  ValentMixerAdapter     parent_instance;

  GHashTable            *streams;
  char                  *default_input;
  char                  *default_output;

  /* PipeWire */
  struct pw_thread_loop *loop;
  struct pw_context     *context;

  struct pw_core        *core;
  struct spa_hook        core_listener;
  struct pw_registry    *registry;
  struct spa_hook        registry_listener;
  struct pw_metadata    *metadata;
  struct spa_hook        metadata_listener;
  struct spa_list        devices;
  struct spa_list        nodes;

  gboolean               closed;
};

G_DEFINE_FINAL_TYPE (ValentPipewireMixer, valent_pipewire_mixer, VALENT_TYPE_MIXER_ADAPTER)


/*
 * Pipewire
 */
struct node_data
{
  ValentPipewireMixer *adapter;

  uint32_t             id;
  uint64_t             serial;
  uint32_t             device_id;

  struct pw_node      *proxy;
  struct spa_hook      proxy_listener;
  struct spa_hook      object_listener;
  struct spa_list      link;

  /* State*/
  char                *node_name;
  char                *node_description;
  enum spa_direction   direction;
  float                volume;
  uint8_t              n_channels;
  bool                 mute;
};

struct device_data
{
  ValentPipewireMixer *adapter;

  uint32_t             id;
  uint64_t             serial;

  struct pw_device    *proxy;
  struct spa_hook      proxy_listener;
  struct spa_hook      object_listener;
  struct spa_list      link;

  /* State*/
  char                *input_description;
  uint32_t             input_device;
  uint32_t             input_port;
  char                *output_description;
  uint32_t             output_device;
  uint32_t             output_port;
};

struct registry_data
{
  ValentPipewireMixer *adapter;

  struct pw_registry  *registry;
  struct pw_proxy     *proxy;
};

static const struct pw_node_events     node_events;
static const struct pw_proxy_events    node_proxy_events;
static const struct pw_device_events   device_events;
static const struct pw_proxy_events    device_proxy_events;
static const struct pw_core_events     core_events;
static const struct pw_metadata_events metadata_events;
static const struct pw_registry_events registry_events;

static void   valent_pipewire_mixer_open  (ValentPipewireMixer *self);
static void   valent_pipewire_mixer_close (ValentPipewireMixer *self);


static inline struct device_data *
valent_pipewire_mixer_lookup_device (ValentPipewireMixer *self,
                                     uint32_t             device_id)
{
  struct device_data *device = NULL;

  spa_list_for_each (device, &self->devices, link)
    {
      if G_UNLIKELY (device == NULL)
        continue;

      if (device->id == device_id)
        return device;
    }

  return NULL;
}

static inline struct node_data *
valent_pipewire_mixer_lookup_device_node (ValentPipewireMixer *self,
                                          uint32_t             device_id,
                                          enum spa_direction   direction)
{
  struct device_data *device = NULL;
  struct node_data *node = NULL;

  if ((device = valent_pipewire_mixer_lookup_device (self, device_id)) == NULL)
    return NULL;

  spa_list_for_each (node, &self->nodes, link)
    {
      if G_UNLIKELY (node == NULL)
        continue;

      if (node->device_id == device->id && node->direction == direction)
        return node;
    }

  return NULL;
}

static inline struct node_data *
valent_pipewire_mixer_lookup_node (ValentPipewireMixer *self,
                                   uint32_t             node_id)
{
  struct node_data *node = NULL;

  spa_list_for_each (node, &self->nodes, link)
    {
      if G_UNLIKELY (node == NULL)
        continue;

      if (node->id == node_id)
        return node;
    }

  return NULL;
}

static inline struct node_data *
valent_pipewire_mixer_lookup_node_name (ValentPipewireMixer *self,
                                        const char          *node_name)
{
  struct node_data *node = NULL;

  spa_list_for_each (node, &self->nodes, link)
    {
      if G_UNLIKELY (node == NULL)
        continue;

      if (g_strcmp0 (node->node_name, node_name) == 0)
        return node;
    }

  return NULL;
}


/*
 * ValentMixerAdapter <-> PipeWire
 */
typedef struct
{
  GRecMutex             mutex;

  ValentPipewireMixer  *adapter;
  uint32_t              device_id;
  uint32_t              node_id;

  /* ValentMixerStream */
  char                 *name;
  char                 *description;
  ValentMixerDirection  direction;
  gboolean              muted;
  uint32_t              level;
} StreamState;

static inline void
stream_state_free (gpointer data)
{
  StreamState *state = (StreamState *)data;

  g_rec_mutex_lock (&state->mutex);
  g_clear_object (&state->adapter);
  g_clear_pointer (&state->name, g_free);
  g_clear_pointer (&state->description, g_free);
  g_rec_mutex_unlock (&state->mutex);
  g_rec_mutex_clear (&state->mutex);
  g_clear_pointer (&state, g_free);
}

static inline StreamState *
stream_state_new (ValentPipewireMixer *self,
                  struct node_data    *node)
{
  struct device_data *device = NULL;
  StreamState *state = NULL;
  ValentMixerDirection direction;
  g_autofree char *description = NULL;

  g_assert (VALENT_IS_PIPEWIRE_MIXER (self));

  device = valent_pipewire_mixer_lookup_device (self, node->device_id);

  if (node->direction == SPA_DIRECTION_INPUT)
    direction = VALENT_MIXER_INPUT;
  else
    direction = VALENT_MIXER_OUTPUT;

  if (direction == VALENT_MIXER_INPUT &&
      (device != NULL && device->input_description != NULL))
    {
      description = g_strdup_printf ("%s (%s)",
                                     device->input_description,
                                     node->node_description);
    }
  else if (direction == VALENT_MIXER_OUTPUT &&
           (device != NULL && device->output_description != NULL))
    {
      description = g_strdup_printf ("%s (%s)",
                                     device->output_description,
                                     node->node_description);
    }
  else
    {
      description = g_strdup (node->node_description);
    }

  state = g_new0 (StreamState, 1);
  g_rec_mutex_init (&state->mutex);
  g_rec_mutex_lock (&state->mutex);
  state->adapter = g_object_ref (self);
  state->device_id = node->device_id;
  state->node_id = node->id;

  state->name = g_strdup (node->node_name);
  state->description = g_steal_pointer (&description);
  state->direction = direction;
  state->level = (uint32_t)ceil (cbrt (node->volume) * 100.0);
  state->muted = !!node->mute;
  g_rec_mutex_unlock (&state->mutex);

  return state;
}

static inline gboolean
stream_state_flush (gpointer data)
{
  StreamState *state = (StreamState *)data;
  ValentPipewireMixer *self = NULL;
  ValentMixerStream *stream = NULL;

  g_assert (VALENT_IS_MAIN_THREAD ());

  g_rec_mutex_lock (&state->mutex);
  self = VALENT_PIPEWIRE_MIXER (state->adapter);

  if (g_atomic_int_get (&self->closed))
    goto closed;

  if ((stream = g_hash_table_lookup (self->streams, state->name)) == NULL)
    {
      stream = g_object_new (VALENT_TYPE_PIPEWIRE_STREAM,
                             "adapter",     self,
                             "device-id",   state->device_id,
                             "node-id",     state->node_id,
                             "name",        state->name,
                             "direction",   state->direction,
                             "level",       state->level,
                             "muted",       state->muted,
                             NULL);
      valent_pipewire_stream_update (VALENT_PIPEWIRE_STREAM (stream),
                                     state->description,
                                     state->level,
                                     state->muted);

      /* Ensure there is a default stream set when `items-changed` is emitted */
      if (self->default_input == NULL && state->direction == VALENT_MIXER_INPUT)
        self->default_input = g_strdup (state->name);
      if (self->default_output == NULL && state->direction == VALENT_MIXER_OUTPUT)
        self->default_output = g_strdup (state->name);

      g_hash_table_replace (self->streams, g_strdup (state->name), stream);
      valent_mixer_adapter_stream_added (VALENT_MIXER_ADAPTER (self), stream);
    }
  else
    {
      valent_pipewire_stream_update (VALENT_PIPEWIRE_STREAM (stream),
                                     state->description,
                                     state->level,
                                     state->muted);
    }

closed:
  g_rec_mutex_unlock (&state->mutex);

  return G_SOURCE_REMOVE;
}

static inline int
stream_state_main (struct spa_loop *loop,
                   bool             async,
                   uint32_t         seq,
                   const void      *data,
                   size_t           size,
                   void            *user_data)
{
  struct node_data *ndata = (struct node_data *)user_data;
  ValentPipewireMixer *self = VALENT_PIPEWIRE_MIXER (ndata->adapter);
  struct device_data *ddata = NULL;
  StreamState *state = NULL;

  if (valent_object_in_destruction (VALENT_OBJECT (self)))
    return 0;

  if ((ddata = valent_pipewire_mixer_lookup_device (self, ndata->device_id)) == NULL)
    return 0;

  state = stream_state_new (self, ndata);
  g_main_context_invoke_full (NULL,
                              G_PRIORITY_DEFAULT,
                              stream_state_flush,
                              g_steal_pointer (&state),
                              stream_state_free);

  return 0;
}

static inline int
stream_state_update (struct spa_loop *loop,
                     bool             async,
                     uint32_t         seq,
                     const void      *data,
                     size_t           size,
                     void            *user_data)
{
  StreamState *state = (StreamState *)user_data;
  struct node_data *ndata = NULL;
  struct device_data *ddata = NULL;
  struct spa_pod_builder builder;
	struct spa_pod_frame f[2];
	struct spa_pod *param;
  char buffer[1024] = { 0, };
  float volumes[SPA_AUDIO_MAX_CHANNELS] = { 0.0, };
  float volume = 0.0;
  uint32_t route_device = 0;
  uint32_t route_index = 0;

  VALENT_ENTRY;

  g_rec_mutex_lock (&state->mutex);
  if (valent_object_in_destruction (VALENT_OBJECT (state->adapter)))
    VALENT_GOTO (closed);

  ndata = valent_pipewire_mixer_lookup_node (state->adapter, state->node_id);
  ddata = valent_pipewire_mixer_lookup_device (state->adapter, state->device_id);

  if (ndata == NULL || ddata == NULL)
    VALENT_GOTO (closed);

  if (ndata->direction == SPA_DIRECTION_OUTPUT)
    {
      route_device = ddata->output_device;
      route_index = ddata->output_port;
    }
  else if (ndata->direction == SPA_DIRECTION_INPUT)
    {
      route_device = ddata->input_device;
      route_index = ddata->input_port;
    }

  volume = ((float)state->level / 100);
  for (uint32_t i = 0; i < ndata->n_channels; i++)
    volumes[i] = volume * volume * volume;


	builder = SPA_POD_BUILDER_INIT (buffer, sizeof (buffer));
	spa_pod_builder_push_object (&builder, &f[0],
			                         SPA_TYPE_OBJECT_ParamRoute, SPA_PARAM_Route);
	spa_pod_builder_add (&builder,
			                 SPA_PARAM_ROUTE_index,  SPA_POD_Int (route_index),
			                 SPA_PARAM_ROUTE_device, SPA_POD_Int (route_device),
			                 0);

	spa_pod_builder_prop (&builder, SPA_PARAM_ROUTE_props, 0);
	spa_pod_builder_push_object (&builder, &f[1],
			                         SPA_TYPE_OBJECT_Props, SPA_PARAM_Route);
  spa_pod_builder_add (&builder,
			                 SPA_PROP_mute,           SPA_POD_Bool ((bool)state->muted),
		                   SPA_PROP_channelVolumes, SPA_POD_Array (sizeof (float),
						                                                   SPA_TYPE_Float,
						                                                   ndata->n_channels,
						                                                   volumes),
                       0);
	spa_pod_builder_pop (&builder, &f[1]);

	spa_pod_builder_prop (&builder, SPA_PARAM_ROUTE_save, 0);
	spa_pod_builder_bool (&builder, true);
	param = spa_pod_builder_pop (&builder, &f[0]);

	pw_device_set_param (ddata->proxy, SPA_PARAM_Route, 0, param);

closed:
  g_rec_mutex_unlock (&state->mutex);
  g_clear_pointer (&state, stream_state_free);

  VALENT_RETURN (0);
}


typedef struct
{
  GRecMutex            mutex;

  ValentPipewireMixer *adapter;
  struct spa_source   *source;

  char                *default_input;
  char                *default_output;
  GPtrArray           *streams;
} MixerState;

static inline void
mixer_state_free (gpointer data)
{
  MixerState *state = (MixerState *)data;

  g_rec_mutex_lock (&state->mutex);
  g_clear_object (&state->adapter);
  g_clear_pointer (&state->default_input, g_free);
  g_clear_pointer (&state->default_output, g_free);
  g_clear_pointer (&state->streams, g_ptr_array_unref);
  g_rec_mutex_unlock (&state->mutex);
  g_rec_mutex_clear (&state->mutex);
  g_clear_pointer (&state, g_free);
}

static inline gboolean
mixer_state_flush (gpointer data)
{
  MixerState *state = (MixerState *)data;
  ValentPipewireMixer *self = NULL;

  g_assert (VALENT_IS_MAIN_THREAD ());

  g_rec_mutex_lock (&state->mutex);
  self = VALENT_PIPEWIRE_MIXER (state->adapter);

  if (!g_atomic_int_get (&self->closed))
    {
      if (state->default_input != NULL)
        {
          if (g_set_str (&self->default_input, state->default_input))
            g_object_notify (G_OBJECT (self), "default-input");
        }

      if (state->default_output != NULL)
        {
          if (g_set_str (&self->default_output, state->default_output))
            g_object_notify (G_OBJECT (self), "default-output");
        }
    }
  g_rec_mutex_unlock (&state->mutex);

  return G_SOURCE_REMOVE;
}

static inline gboolean
has_stream (gconstpointer a,
            gconstpointer b)
{
  return g_strcmp0 (((StreamState *)a)->name, (const char *)b) == 0;
}

static inline gboolean
mixer_streams_flush (gpointer data)
{
  MixerState *state = (MixerState *)data;
  ValentPipewireMixer *self = NULL;
  GHashTableIter iter;
  const char *name;
  ValentMixerStream *stream;

  g_assert (VALENT_IS_MAIN_THREAD ());

  g_rec_mutex_lock (&state->mutex);
  self = VALENT_PIPEWIRE_MIXER (state->adapter);

  if (!g_atomic_int_get (&self->closed))
    {
      g_hash_table_iter_init (&iter, self->streams);

      while (g_hash_table_iter_next (&iter, (void **)&name, (void **)&stream))
        {
          unsigned int index_ = 0;

          if (g_ptr_array_find_with_equal_func (state->streams, name, has_stream, &index_))
            {
              g_ptr_array_remove_index (state->streams, index_);
              continue;
            }

          valent_mixer_adapter_stream_removed (VALENT_MIXER_ADAPTER (self), stream);
          g_hash_table_iter_remove (&iter);
        }

      for (unsigned int i = 0; i < state->streams->len; i++)
        stream_state_flush (g_ptr_array_index (state->streams, i));
    }
  g_rec_mutex_unlock (&state->mutex);

  return G_SOURCE_REMOVE;
}


/*
 * Nodes
 */
static inline void
on_node_info (void                      *object,
              const struct pw_node_info *info)
{
  struct node_data *ndata = (struct node_data *)object;
  ValentPipewireMixer *self = VALENT_PIPEWIRE_MIXER (ndata->adapter);

  if (valent_object_in_destruction (VALENT_OBJECT (self)))
    return;

  if (info->change_mask & PW_NODE_CHANGE_MASK_PARAMS)
    {
      for (uint32_t i = 0; i < info->n_params; i++)
        {
          uint32_t id = info->params[i].id;
          uint32_t flags = info->params[i].flags;

          if (id == SPA_PARAM_Props && (flags & SPA_PARAM_INFO_READ) != 0)
            {
              pw_node_enum_params (ndata->proxy, 0, id, 0, UINT32_MAX, NULL);
              pw_core_sync (self->core, PW_ID_CORE, 0);
            }
        }
    }
}

static void
on_node_param (void                 *object,
               int                   seq,
               uint32_t              id,
               uint32_t              index,
               uint32_t              next,
               const struct spa_pod *param)
{
  struct node_data *ndata = (struct node_data *)object;
  ValentPipewireMixer *self = VALENT_PIPEWIRE_MIXER (ndata->adapter);
  gboolean notify = FALSE;
  bool mute = false;
  uint32_t csize, ctype;
  uint32_t n_channels = 0;
  float *volumes = NULL;
  float volume = 0.0;

  if (valent_object_in_destruction (VALENT_OBJECT (self)))
    return;

  if (id != SPA_PARAM_Props || param == NULL)
    return;

  if (spa_pod_parse_object (param, SPA_TYPE_OBJECT_Props, NULL,
                            SPA_PROP_mute,           SPA_POD_Bool (&mute),
                            SPA_PROP_volume,         SPA_POD_Float (&volume),
                            SPA_PROP_channelVolumes, SPA_POD_Array (&csize,
                                                                    &ctype,
                                                                    &n_channels,
                                                                    &volumes)) < 0)
    return;

  if (ndata->mute != mute)
    {
      ndata->mute = mute;
      notify = TRUE;
    }

  if (n_channels > 0)
    {
      volume = 0.0;

      for (uint32_t i = 0; i < n_channels; i++)
        volume = MAX (volume, volumes[i]);
    }

  if (!G_APPROX_VALUE (ndata->volume, volume, 0.0000001))
    {
      ndata->volume = volume;
      ndata->n_channels = n_channels;
      notify = TRUE;
    }

  if (notify)
    {
      pw_loop_invoke (pw_thread_loop_get_loop (self->loop),
                      stream_state_main,
                      0,
                      NULL,
                      0,
                      false,
                      ndata);
    }
}

static const struct pw_node_events node_events = {
  .info = on_node_info,
  .param = on_node_param,
};


static void
on_node_proxy_removed (void *data)
{
  struct node_data *ndata = data;

  VALENT_PROBE;

  spa_hook_remove (&ndata->object_listener);
  pw_proxy_destroy ((struct pw_proxy*)ndata->proxy);
}

static void
on_node_proxy_destroyed (void *data)
{
  struct node_data *ndata = (struct node_data *)data;
  ValentPipewireMixer *self = VALENT_PIPEWIRE_MIXER (ndata->adapter);
  MixerState *state = NULL;

  VALENT_NOTE ("id: %u, serial: %zu", ndata->id, ndata->serial);

  g_clear_pointer (&ndata->node_name, g_free);
  g_clear_pointer (&ndata->node_description, g_free);
  spa_list_remove (&ndata->link);

  if (valent_object_in_destruction (VALENT_OBJECT (ndata->adapter)))
    return;

  state = g_new0 (MixerState, 1);
  g_rec_mutex_init (&state->mutex);
  g_rec_mutex_lock (&state->mutex);
  state->adapter = g_object_ref (self);
  state->streams = g_ptr_array_new_with_free_func (stream_state_free);

  spa_list_for_each (ndata, &self->nodes, link)
    g_ptr_array_add (state->streams, stream_state_new (self, ndata));

  g_rec_mutex_unlock (&state->mutex);

  g_main_context_invoke_full (NULL,
                              G_PRIORITY_DEFAULT,
                              mixer_streams_flush,
                              g_steal_pointer (&state),
                              mixer_state_free);
}

static const struct pw_proxy_events node_proxy_events = {
  PW_VERSION_PROXY_EVENTS,
  .removed = on_node_proxy_removed,
  .destroy = on_node_proxy_destroyed,
};


static void
on_device_info (void                        *object,
                const struct pw_device_info *info)
{
  struct device_data *ddata = (struct device_data *)object;
  ValentPipewireMixer *self = VALENT_PIPEWIRE_MIXER (ddata->adapter);

  if (valent_object_in_destruction (VALENT_OBJECT (self)))
    return;

  if ((info->change_mask & PW_DEVICE_CHANGE_MASK_PARAMS) != 0)
    {
      for (uint32_t i = 0; i < info->n_params; i++)
        {
          uint32_t id = info->params[i].id;
          uint32_t flags = info->params[i].flags;

          if (id == SPA_PARAM_Route && (flags & SPA_PARAM_INFO_READ) != 0)
            {
              pw_device_enum_params (ddata->proxy, 0, id, 0, UINT32_MAX, NULL);
              pw_core_sync (self->core, PW_ID_CORE, 0);
            }
        }
    }
}

static void
on_device_param (void                 *data,
                 int                   seq,
                 uint32_t              id,
                 uint32_t              index,
                 uint32_t              next,
                 const struct spa_pod *param)
{
  struct device_data *ddata = (struct device_data *)data;
  struct node_data *ndata = NULL;
  ValentPipewireMixer *self = VALENT_PIPEWIRE_MIXER (ddata->adapter);
  const char *name;
  const char *description;
  uint32_t route_index = 0;
  uint32_t route_device = 0;
  enum spa_direction direction = 0;
  enum spa_param_availability available = 0;
	struct spa_pod *props = NULL;

  if (valent_object_in_destruction (VALENT_OBJECT (self)))
    return;

  if (id != SPA_PARAM_Route || param == NULL)
    return;

  if (spa_pod_parse_object (param, SPA_TYPE_OBJECT_ParamRoute, NULL,
                            SPA_PARAM_ROUTE_name,        SPA_POD_String (&name),
                            SPA_PARAM_ROUTE_description, SPA_POD_String (&description),
                            SPA_PARAM_ROUTE_direction,   SPA_POD_Id (&direction),
                            SPA_PARAM_ROUTE_index,       SPA_POD_Int (&route_index),
                            SPA_PARAM_ROUTE_device,      SPA_POD_Int (&route_device),
                            SPA_PARAM_ROUTE_available,   SPA_POD_Id (&available),
                            SPA_PARAM_ROUTE_props,       SPA_POD_OPT_Pod (&props)) < 0)
    return;

  if (direction == SPA_DIRECTION_INPUT)
    {
      ddata->input_device = route_device;
      ddata->input_port = route_index;

      if (!g_set_str (&ddata->input_description, description))
        return;
    }
  else if (direction == SPA_DIRECTION_OUTPUT)
    {
      ddata->output_device = route_device;
      ddata->output_port = route_index;

      if (!g_set_str (&ddata->output_description, description))
        return;
    }

  /* There may not be a node yet */
  ndata = valent_pipewire_mixer_lookup_device_node (self, ddata->id, direction);

  if (ndata != NULL)
    {
      pw_loop_invoke (pw_thread_loop_get_loop (self->loop),
                      stream_state_main,
                      0,
                      NULL,
                      0,
                      false,
                      ndata);
    }
}


static const struct pw_device_events device_events = {
  PW_VERSION_DEVICE_EVENTS,
  .info = on_device_info,
  .param = on_device_param,
};


static void
on_device_proxy_removed (void *data)
{
  struct device_data *ddata = data;

  spa_hook_remove (&ddata->object_listener);
  pw_proxy_destroy ((struct pw_proxy *)ddata->proxy);
}

static void
on_device_proxy_destroyed (void *data)
{
  struct device_data *ddata = data;

  VALENT_NOTE ("id: %u, serial: %zu", ddata->id, ddata->serial);

  g_clear_pointer (&ddata->input_description, g_free);
  g_clear_pointer (&ddata->output_description, g_free);
  spa_list_remove (&ddata->link);
}

static const struct pw_proxy_events device_proxy_events = {
  PW_VERSION_PROXY_EVENTS,
  .removed = on_device_proxy_removed,
  .destroy = on_device_proxy_destroyed,
};


static int
on_metadata_property (void       *data,
                      uint32_t    id,
                      const char *key,
                      const char *type,
                      const char *value)
{
  ValentPipewireMixer *self = VALENT_PIPEWIRE_MIXER (data);
  MixerState *state = NULL;
  g_autoptr (JsonNode) node = NULL;
  JsonObject *root = NULL;
  const char *name = NULL;

  VALENT_NOTE ("id: %u, key: %s, type: %s, value: %s", id, key, type, value);

  if (valent_object_in_destruction (VALENT_OBJECT (self)))
    return 0;

  if G_UNLIKELY (key == NULL || type == NULL || value == NULL)
    return 0;

  if (!g_str_equal (key, "default.audio.sink") &&
      !g_str_equal (key, "default.audio.source"))
    return 0;

  if (!g_str_equal (type, "Spa:String:JSON"))
    return 0;

  if ((node = json_from_string (value, NULL)) == NULL ||
      (root = json_node_get_object (node)) == NULL ||
      (name = json_object_get_string_member (root, "name")) == NULL)
    {
      g_warning ("%s(): Failed to parse metadata", G_STRFUNC);
      return 0;
    }

  state = g_new0 (MixerState, 1);
  g_rec_mutex_init (&state->mutex);
  g_rec_mutex_lock (&state->mutex);
  state->adapter = g_object_ref (self);

  if (g_str_equal (key, "default.audio.sink"))
    g_set_str (&state->default_output, name);
  else if (g_str_equal (key, "default.audio.source"))
    g_set_str (&state->default_input, name);

  g_rec_mutex_unlock (&state->mutex);

  g_main_context_invoke_full (NULL,
                              G_PRIORITY_DEFAULT,
                              mixer_state_flush,
                              g_steal_pointer (&state),
                              mixer_state_free);
  pw_core_sync (self->core, PW_ID_CORE, 0);

  return 0;
}

static const struct pw_metadata_events metadata_events = {
  PW_VERSION_METADATA_EVENTS,
  on_metadata_property
};


/*
 * Pipewire Registry
 */
static void
registry_event_global (void                  *data,
                       uint32_t               id,
                       uint32_t               permissions,
                       const char            *type,
                       uint32_t               version,
                       const struct spa_dict *props)
{
  ValentPipewireMixer *self = VALENT_PIPEWIRE_MIXER (data);

  if (valent_object_in_destruction (VALENT_OBJECT (self)))
    return;

  if G_UNLIKELY (id == SPA_ID_INVALID)
    return;

  if (g_strcmp0 (type, PW_TYPE_INTERFACE_Device) == 0)
    {
      struct pw_device *device = NULL;
      struct device_data *ddata = NULL;
      const char *media_class = NULL;

      VALENT_NOTE ("id: %u, permissions: %u, type: %s, version: %u",
                   id, permissions, type, version);

      /* Only audio devices are of interest, for now */
      media_class = spa_dict_lookup (props, PW_KEY_MEDIA_CLASS);

      if (g_strcmp0 (media_class, "Audio/Device") != 0)
        return;

      device = pw_registry_bind (self->registry, id, type,
                                 PW_VERSION_PORT, sizeof (*ddata));
      g_return_if_fail (device != NULL);

      ddata = pw_proxy_get_user_data ((struct pw_proxy *)device);
      ddata->adapter = self;
      ddata->proxy = device;
      ddata->id = id;

      spa_list_append (&self->devices, &ddata->link);
      pw_device_add_listener (ddata->proxy,
                              &ddata->object_listener,
                              &device_events,
                              ddata);
      pw_proxy_add_listener ((struct pw_proxy *)ddata->proxy,
                             &ddata->proxy_listener,
                             &device_proxy_events,
                             ddata);
      pw_core_sync (self->core, PW_ID_CORE, 0);
    }
  else if (g_strcmp0 (type, PW_TYPE_INTERFACE_Node) == 0)
    {
      struct pw_node *node = NULL;
      struct node_data *ndata = NULL;
      struct device_data *ddata = NULL;
      uint32_t device_id;
      const char *media_class = NULL;

      VALENT_NOTE ("id: %u, permissions: %u, type: %s, version: %u",
                   id, permissions, type, version);

      /* Only audio sinks and sources are of interest, for now */
      media_class = spa_dict_lookup (props, PW_KEY_MEDIA_CLASS);

      if (g_strcmp0 (media_class, "Audio/Sink") != 0 &&
          g_strcmp0 (media_class, "Audio/Source") != 0)
        return;

      /* Only nodes with devices are of interest */
      if (!spa_atou32 (spa_dict_lookup (props, PW_KEY_DEVICE_ID), &device_id, 10) ||
          (ddata = valent_pipewire_mixer_lookup_device (self, device_id)) == NULL)
        return;

      node = pw_registry_bind (self->registry, id, type,
                               PW_VERSION_NODE, sizeof (*ndata));
      g_return_if_fail (node != NULL);

      ndata = pw_proxy_get_user_data ((struct pw_proxy *)node);
      ndata->adapter = self;
      ndata->proxy = node;
      ndata->id = id;
      ndata->device_id = device_id;

      ndata->node_name = g_strdup (spa_dict_lookup (props, PW_KEY_NODE_NAME));
      ndata->node_description = g_strdup (spa_dict_lookup (props, PW_KEY_NODE_DESCRIPTION));

      if (g_str_equal (media_class, "Audio/Sink"))
        ndata->direction = SPA_DIRECTION_OUTPUT;
      else if (g_str_equal (media_class, "Audio/Source"))
        ndata->direction = SPA_DIRECTION_INPUT;

      spa_list_append (&self->nodes, &ndata->link);
      pw_node_add_listener (ndata->proxy,
                            &ndata->object_listener,
                            &node_events,
                            ndata);
      pw_proxy_add_listener ((struct pw_proxy *)ndata->proxy,
                             &ndata->proxy_listener,
                             &node_proxy_events,
                             ndata);
      pw_core_sync (self->core, PW_ID_CORE, 0);
    }
  else if (g_strcmp0 (type, PW_TYPE_INTERFACE_Metadata) == 0)
    {
      const char *metadata_name = NULL;

      VALENT_NOTE ("id: %u, permissions: %u, type: %s, version: %u",
                   id, permissions, type, version);

      metadata_name = spa_dict_lookup (props, PW_KEY_METADATA_NAME);

      if (g_strcmp0 (metadata_name, "default") == 0)
        {
          if (self->metadata != NULL)
            spa_hook_remove (&self->metadata_listener);

          self->metadata = pw_registry_bind (self->registry, id, type,
                                             PW_VERSION_METADATA, 0);

          if (self->metadata != NULL)
            {
              pw_metadata_add_listener (self->metadata,
                                        &self->metadata_listener,
                                        &metadata_events,
                                        self);
            }
        }

      pw_core_sync (self->core, PW_ID_CORE, 0);
    }
}

static const struct pw_registry_events registry_events = {
  PW_VERSION_REGISTRY_EVENTS,
  .global = registry_event_global,
};


static void
on_core_done (void     *data,
              uint32_t  id,
              int       seq)
{
  ValentPipewireMixer *self = VALENT_PIPEWIRE_MIXER (data);

  VALENT_NOTE ("id: %u, seq: %d", id, seq);

  if (id == PW_ID_CORE)
    pw_thread_loop_signal (self->loop, FALSE);
}

static void
on_core_error (void       *data,
               uint32_t    id,
               int         seq,
               int         res,
               const char *message)
{
  ValentPipewireMixer *self = VALENT_PIPEWIRE_MIXER (data);

  VALENT_NOTE ("id: %u, seq: %i, res: %i, message: %s", id, seq, res, message);

  if (id == PW_ID_CORE)
    g_warning ("%s(): %s (%i)", G_STRFUNC, message, res);

  pw_thread_loop_signal (self->loop, FALSE);
}

static const struct pw_core_events core_events = {
  PW_VERSION_CORE_EVENTS,
  .done = on_core_done,
  .error = on_core_error,
};


/*
 *
 */
static void
valent_pipewire_mixer_open (ValentPipewireMixer *self)
{
  struct pw_properties *context_properties = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_PIPEWIRE_MIXER (self));
  g_assert (VALENT_IS_MAIN_THREAD ());

  self->loop = pw_thread_loop_new ("valent", NULL);
  pw_thread_loop_lock (self->loop);

  if (self->loop == NULL || pw_thread_loop_start (self->loop) != 0)
    {
      pw_thread_loop_unlock (self->loop);
      g_set_error_literal (&error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "failed to start the thread loop");
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ERROR,
                                             error);
      return;
    }

  spa_list_init (&self->devices);
  spa_list_init (&self->nodes);

  /* Register as a manager */
  context_properties = pw_properties_new (PW_KEY_CONFIG_NAME,    "client-rt.conf",
                                          PW_KEY_MEDIA_TYPE,     "Audio",
                                          PW_KEY_MEDIA_CATEGORY, "Manager",
                                          PW_KEY_MEDIA_ROLE,     "Music",
                                          NULL);
  self->context = pw_context_new (pw_thread_loop_get_loop (self->loop),
                                  context_properties,
                                  0);

  if (self->context == NULL)
    {
      pw_thread_loop_unlock (self->loop);
      g_set_error_literal (&error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "failed to create context");
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ERROR,
                                             error);
      return;
    }

  /* Failure here usually means missing Flatpak permissions */
  self->core = pw_context_connect (self->context, NULL, 0);

  if (self->core == NULL)
    {
      pw_thread_loop_unlock (self->loop);
      g_set_error_literal (&error,
                           G_IO_ERROR,
                           G_IO_ERROR_PERMISSION_DENIED,
                           "failed to connect context");
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ERROR,
                                             error);
      return;
    }

  spa_zero (self->core_listener);
  pw_core_add_listener (self->core,
                        &self->core_listener,
                        &core_events,
                        self);

  self->registry = pw_core_get_registry (self->core, PW_VERSION_REGISTRY, 0);

  if (self->registry == NULL)
    {
      pw_thread_loop_unlock (self->loop);
      g_set_error_literal (&error,
                           G_IO_ERROR,
                           G_IO_ERROR_PERMISSION_DENIED,
                           "failed to connect to registry");
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ERROR,
                                             error);
      return;
    }

  spa_zero (self->registry_listener);
  pw_registry_add_listener (self->registry,
                            &self->registry_listener,
                            &registry_events,
                            self);
  pw_core_sync (self->core, PW_ID_CORE, 0);
  pw_thread_loop_unlock (self->loop);
}

static void
valent_pipewire_mixer_close (ValentPipewireMixer *self)
{
  VALENT_ENTRY;

  g_assert (VALENT_IS_PIPEWIRE_MIXER (self));
  g_assert (VALENT_IS_MAIN_THREAD ());

  g_atomic_int_set (&self->closed, TRUE);

  if (self->loop != NULL)
    {
      pw_thread_loop_lock (self->loop);

      if (self->metadata != NULL)
        {
          spa_hook_remove (&self->metadata_listener);
          pw_proxy_destroy ((struct pw_proxy *)self->metadata);
          self->metadata = NULL;
        }

      if (self->registry != NULL)
        {
          spa_hook_remove (&self->registry_listener);
          pw_proxy_destroy ((struct pw_proxy *)self->registry);
          self->registry = NULL;
        }

      if (self->core != NULL)
        {
          spa_hook_remove (&self->core_listener);
          g_clear_pointer (&self->core, pw_core_disconnect);
        }

      g_clear_pointer (&self->context, pw_context_destroy);

      pw_thread_loop_unlock (self->loop);
      pw_thread_loop_stop (self->loop);

      g_clear_pointer (&self->loop, pw_thread_loop_destroy);
    }

  VALENT_EXIT;
}


/*
 * ValentMixerAdapter
 */
static ValentMixerStream *
valent_pipewire_mixer_get_default_input (ValentMixerAdapter *adapter)
{
  ValentPipewireMixer *self = VALENT_PIPEWIRE_MIXER (adapter);

  return g_hash_table_lookup (self->streams, self->default_input);
}

static void
valent_pipewire_mixer_set_default_input (ValentMixerAdapter *adapter,
                                         ValentMixerStream  *stream)
{
  ValentPipewireMixer *self = VALENT_PIPEWIRE_MIXER (adapter);
  struct node_data *ndata = NULL;
  const char *name = NULL;

  g_assert (VALENT_IS_PIPEWIRE_MIXER (self));
  g_assert (VALENT_IS_MIXER_STREAM (stream));

  name = valent_mixer_stream_get_name (stream);

  if (g_strcmp0 (self->default_input, name) == 0)
    return;

  pw_thread_loop_lock (self->loop);
  if ((ndata = valent_pipewire_mixer_lookup_node_name (self, name)) != NULL)
    {
      g_autofree char *json = NULL;

      json = g_strdup_printf ("{\"name\": \"%s\"}", name);
      pw_metadata_set_property (self->metadata, PW_ID_CORE,
                                "default.audio.source", "Spa:Id", json);

      /* Emit now, since we won't get notification from pipewire */
      if (g_set_str (&self->default_input, name))
        g_object_notify (G_OBJECT (self), "default-input");
    }
  pw_thread_loop_unlock (self->loop);
}

static ValentMixerStream *
valent_pipewire_mixer_get_default_output (ValentMixerAdapter *adapter)
{
  ValentPipewireMixer *self = VALENT_PIPEWIRE_MIXER (adapter);

  return g_hash_table_lookup (self->streams, self->default_output);
}

static void
valent_pipewire_mixer_set_default_output (ValentMixerAdapter *adapter,
                                          ValentMixerStream  *stream)
{
  ValentPipewireMixer *self = VALENT_PIPEWIRE_MIXER (adapter);
  struct node_data *ndata = NULL;
  const char *name = NULL;

  g_assert (VALENT_IS_PIPEWIRE_MIXER (self));
  g_assert (VALENT_IS_MIXER_STREAM (stream));

  name = valent_mixer_stream_get_name (stream);

  if (g_strcmp0 (self->default_output, name) == 0)
    return;

  pw_thread_loop_lock (self->loop);
  if ((ndata = valent_pipewire_mixer_lookup_node_name (self, name)) != NULL)
    {
      g_autofree char *json = NULL;

      json = g_strdup_printf ("{\"name\": \"%s\"}", name);
      pw_metadata_set_property (self->metadata, PW_ID_CORE,
                                "default.audio.sink", "Spa:Id", json);

      /* Emit now, since we won't get notification from pipewire */
      if (g_set_str (&self->default_output, name))
        g_object_notify (G_OBJECT (self), "default-output");
    }
  pw_thread_loop_unlock (self->loop);
}

/*
 * GObject
 */
static void
valent_pipewire_mixer_constructed (GObject *object)
{
  ValentPipewireMixer *self = VALENT_PIPEWIRE_MIXER (object);

  valent_pipewire_mixer_open (self);

  G_OBJECT_CLASS (valent_pipewire_mixer_parent_class)->constructed (object);
}

static void
valent_pipewire_mixer_destroy (ValentObject *object)
{
  ValentPipewireMixer *self = VALENT_PIPEWIRE_MIXER (object);

  valent_pipewire_mixer_close (self);
  g_hash_table_remove_all (self->streams);

  VALENT_OBJECT_CLASS (valent_pipewire_mixer_parent_class)->destroy (object);
}

static void
valent_pipewire_mixer_finalize (GObject *object)
{
  ValentPipewireMixer *self = VALENT_PIPEWIRE_MIXER (object);

  pw_deinit ();
  g_clear_pointer (&self->streams, g_hash_table_unref);

  G_OBJECT_CLASS (valent_pipewire_mixer_parent_class)->finalize (object);
}

static void
valent_pipewire_mixer_class_init (ValentPipewireMixerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentMixerAdapterClass *adapter_class = VALENT_MIXER_ADAPTER_CLASS (klass);

  object_class->constructed = valent_pipewire_mixer_constructed;
  object_class->finalize = valent_pipewire_mixer_finalize;

  vobject_class->destroy = valent_pipewire_mixer_destroy;

  adapter_class->get_default_input = valent_pipewire_mixer_get_default_input;
  adapter_class->set_default_input = valent_pipewire_mixer_set_default_input;
  adapter_class->get_default_output = valent_pipewire_mixer_get_default_output;
  adapter_class->set_default_output = valent_pipewire_mixer_set_default_output;
}

static void
valent_pipewire_mixer_init (ValentPipewireMixer *self)
{
  self->closed = FALSE;
  self->streams = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         g_object_unref);
  pw_init (NULL, NULL);
}

void
valent_pipewire_mixer_set_stream_state (ValentPipewireMixer *adapter,
                                        uint32_t             device_id,
                                        uint32_t             node_id,
                                        unsigned int         level,
                                        gboolean             muted)
{
  StreamState *state = NULL;

  g_assert (VALENT_IS_PIPEWIRE_MIXER (adapter));
  g_assert (device_id > 0);
  g_assert (node_id > 0);

  VALENT_NOTE ("device: %u, node: %u, level: %u, muted: %u",
               device_id, node_id, level, muted);

  state = g_new0 (StreamState, 1);
  g_rec_mutex_init (&state->mutex);
  g_rec_mutex_lock (&state->mutex);
  state->adapter = g_object_ref (adapter);
  state->device_id = device_id;
  state->node_id = node_id;
  state->level = level;
  state->muted = muted;
  g_rec_mutex_unlock (&state->mutex);

  pw_thread_loop_lock (adapter->loop);
  pw_loop_invoke (pw_thread_loop_get_loop (adapter->loop),
                  stream_state_update,
                  0,
                  NULL,
                  0,
                  false,
                  state);
  pw_thread_loop_unlock (adapter->loop);
}

