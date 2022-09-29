// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-media-adapter"

#include "config.h"

#include <libvalent-core.h>
#include <libvalent-media.h>

#include "valent-mock-media-adapter.h"


struct _ValentMockMediaAdapter
{
  ValentMediaAdapter  parent_instance;
};

G_DEFINE_TYPE (ValentMockMediaAdapter, valent_mock_media_adapter, VALENT_TYPE_MEDIA_ADAPTER)


/*
 * ValentMediaAdapter
 */
static void
valent_mock_media_adapter_load_async (ValentMediaAdapter  *adapter,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_MOCK_MEDIA_ADAPTER (adapter));

  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mock_media_adapter_load_async);
  g_task_return_boolean (task, TRUE);
}

/*
 * GObject
 */
static void
valent_mock_media_adapter_class_init (ValentMockMediaAdapterClass *klass)
{
  ValentMediaAdapterClass *adapter_class = VALENT_MEDIA_ADAPTER_CLASS (klass);

  adapter_class->load_async = valent_mock_media_adapter_load_async;
}

static void
valent_mock_media_adapter_init (ValentMockMediaAdapter *self)
{
}

