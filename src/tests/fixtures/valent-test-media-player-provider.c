// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-test-media-player-provider"

#include "config.h"

#include <libvalent-core.h>
#include <libvalent-media.h>

#include "valent-test-media-player-provider.h"


struct _ValentTestMediaPlayerProvider
{
  ValentMediaPlayerProvider  parent_instance;
};

G_DEFINE_TYPE (ValentTestMediaPlayerProvider, valent_test_media_player_provider, VALENT_TYPE_MEDIA_PLAYER_PROVIDER)


/*
 * ValentMediaPlayerProvider
 */
static void
valent_test_media_player_provider_load_async (ValentMediaPlayerProvider *provider,
                                              GCancellable              *cancellable,
                                              GAsyncReadyCallback        callback,
                                              gpointer                   user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_TEST_MEDIA_PLAYER_PROVIDER (provider));

  task = g_task_new (provider, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_test_media_player_provider_load_async);
  g_task_return_boolean (task, TRUE);
}

/*
 * GObject
 */
static void
valent_test_media_player_provider_class_init (ValentTestMediaPlayerProviderClass *klass)
{
  ValentMediaPlayerProviderClass *provider_class = VALENT_MEDIA_PLAYER_PROVIDER_CLASS (klass);

  provider_class->load_async = valent_test_media_player_provider_load_async;
}

static void
valent_test_media_player_provider_init (ValentTestMediaPlayerProvider *self)
{
}

