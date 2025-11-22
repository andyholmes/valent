// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-input"

#include "config.h"

#include <glib-object.h>
#include <libpeas.h>
#include <libvalent-core.h>

#include "valent-input-adapter.h"

#include "valent-input.h"

/**
 * ValentInput:
 *
 * A class for controlling pointer and keyboard devices.
 *
 * Plugins can implement [class@Valent.InputAdapter] to provide an interface to
 * control the pointer and keyboard.
 *
 * Since: 1.0
 */
struct _ValentInput
{
  ValentComponent  parent_instance;
};

G_DEFINE_FINAL_TYPE (ValentInput, valent_input, VALENT_TYPE_COMPONENT)

/*
 * GObject
 */
static void
valent_input_class_init (ValentInputClass *klass)
{
}

static void
valent_input_init (ValentInput *self)
{
}

/**
 * valent_input_get_default:
 *
 * Get the default [class@Valent.Input].
 *
 * Returns: (transfer none) (not nullable): a `ValentInput`
 *
 * Since: 1.0
 */
ValentInput *
valent_input_get_default (void)
{
  static ValentInput *default_instance = NULL;

  if (default_instance == NULL)
    {
      default_instance = g_object_new (VALENT_TYPE_INPUT,
                                       "plugin-domain", "input",
                                       "plugin-type",   VALENT_TYPE_INPUT_ADAPTER,
                                       NULL);
      g_object_add_weak_pointer (G_OBJECT (default_instance),
                                 (gpointer)&default_instance);
    }

  return default_instance;
}

