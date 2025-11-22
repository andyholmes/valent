// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-media-adapter"

#include "config.h"

#include <valent.h>

#include "valent-mock-media-player.h"

#include "valent-mock-media-adapter.h"


struct _ValentMockMediaAdapter
{
  ValentMediaAdapter  parent_instance;
};

G_DEFINE_FINAL_TYPE (ValentMockMediaAdapter, valent_mock_media_adapter, VALENT_TYPE_MEDIA_ADAPTER)

static void
add_player_action (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  ValentMediaAdapter *adapter = VALENT_MEDIA_ADAPTER (user_data);
  g_autoptr (ValentMediaPlayer) player = NULL;
  g_autofree char *iri = NULL;

  iri = tracker_sparql_get_uuid_urn ();
  player = g_object_new (VALENT_TYPE_MOCK_MEDIA_PLAYER,
                         "iri",    iri,
                         "parent", adapter,
                         NULL);
  valent_media_adapter_player_added (adapter, player);
}

static const GActionEntry actions[] = {
    {"add-player", add_player_action, NULL, NULL, NULL},
};

/*
 * GObject
 */
static void
valent_mock_media_adapter_class_init (ValentMockMediaAdapterClass *klass)
{
}

static void
valent_mock_media_adapter_init (ValentMockMediaAdapter *self)
{
  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
}

