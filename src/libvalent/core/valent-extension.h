// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include "valent-context.h"

G_BEGIN_DECLS

/**
 * ValentPluginState:
 * @VALENT_PLUGIN_STATE_ACTIVE: the plugin functionality is available
 * @VALENT_PLUGIN_STATE_INACTIVE: the plugin functionality is unavailable
 * @VALENT_PLUGIN_STATE_ERROR: the plugin encountered an unrecoverable error
 * Since: 1.0
 */
typedef enum
{
  VALENT_PLUGIN_STATE_ACTIVE,
  VALENT_PLUGIN_STATE_INACTIVE,
  VALENT_PLUGIN_STATE_ERROR,
} ValentPluginState;


#define VALENT_TYPE_EXTENSION (valent_extension_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentExtension, valent_extension, VALENT, EXTENSION, ValentObject)

struct _ValentExtensionClass
{
  ValentObjectClass   parent_class;

  /* virtual functions */

  /*< private >*/
  gpointer            padding[8];
};

VALENT_AVAILABLE_IN_1_0
ValentContext     * valent_extension_get_context          (ValentExtension    *extension);
VALENT_AVAILABLE_IN_1_0
gpointer            valent_extension_get_object           (ValentExtension    *extension);
VALENT_AVAILABLE_IN_1_0
GSettings         * valent_extension_get_settings         (ValentExtension    *extension);
VALENT_AVAILABLE_IN_1_0
ValentPluginState   valent_extension_plugin_state_check   (ValentExtension    *extension,
                                                           GError            **error);
VALENT_AVAILABLE_IN_1_0
void                valent_extension_plugin_state_changed (ValentExtension    *extension,
                                                           ValentPluginState   state,
                                                           GError             *error);
VALENT_AVAILABLE_IN_1_0
void                valent_extension_toggle_actions       (ValentExtension    *extension,
                                                           gboolean            enabled);

G_END_DECLS

