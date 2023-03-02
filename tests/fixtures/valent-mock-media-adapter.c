// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-media-adapter"

#include "config.h"

#include <valent.h>

#include "valent-mock-media-adapter.h"


struct _ValentMockMediaAdapter
{
  ValentMediaAdapter  parent_instance;
};

G_DEFINE_FINAL_TYPE (ValentMockMediaAdapter, valent_mock_media_adapter, VALENT_TYPE_MEDIA_ADAPTER)


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
}

