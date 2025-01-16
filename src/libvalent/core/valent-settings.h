/* valent-settings.h
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gio/gio.h>

#include "valent-data-source.h"

G_BEGIN_DECLS

#define VALENT_TYPE_SETTINGS (valent_settings_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_FINAL_TYPE (ValentSettings, valent_settings, VALENT, SETTINGS, GObject)

VALENT_AVAILABLE_IN_1_0
ValentSettings * valent_settings_new               (ValentDataSource        *data_source,
                                                    const char              *schema_id);
VALENT_AVAILABLE_IN_1_0
ValentSettings * valent_settings_new_full          (ValentDataSource        *data_source,
                                                    GSettingsSchema         *schema,
                                                    const char              *path);
VALENT_AVAILABLE_IN_1_0
GVariant       * valent_settings_get_default_value (ValentSettings          *settings,
                                                    const char              *key);
VALENT_AVAILABLE_IN_1_0
GVariant       * valent_settings_get_user_value    (ValentSettings          *settings,
                                                    const char              *key);
VALENT_AVAILABLE_IN_1_0
GVariant       * valent_settings_get_value         (ValentSettings          *settings,
                                                    const char              *key);
VALENT_AVAILABLE_IN_1_0
void             valent_settings_set_value         (ValentSettings          *settings,
                                                    const char              *key,
                                                    GVariant                *value);
VALENT_AVAILABLE_IN_1_0
gboolean         valent_settings_get_boolean       (ValentSettings          *settings,
                                                    const char              *key);
VALENT_AVAILABLE_IN_1_0
void             valent_settings_set_boolean       (ValentSettings          *settings,
                                                    const char              *key,
                                                    gboolean                 val);
VALENT_AVAILABLE_IN_1_0
double           valent_settings_get_double        (ValentSettings          *settings,
                                                    const char              *key);
VALENT_AVAILABLE_IN_1_0
void             valent_settings_set_double        (ValentSettings          *settings,
                                                    const char              *key,
                                                    double                   val);
VALENT_AVAILABLE_IN_1_0
int32_t          valent_settings_get_int32         (ValentSettings          *settings,
                                                    const char              *key);
VALENT_AVAILABLE_IN_1_0
void             valent_settings_set_int32         (ValentSettings          *settings,
                                                    const char              *key,
                                                    int32_t                  val);
VALENT_AVAILABLE_IN_1_0
int64_t          valent_settings_get_int64         (ValentSettings          *settings,
                                                    const char              *key);
VALENT_AVAILABLE_IN_1_0
void             valent_settings_set_int64         (ValentSettings          *settings,
                                                    const char              *key,
                                                    int64_t                  val);
VALENT_AVAILABLE_IN_1_0
char           * valent_settings_get_string        (ValentSettings          *settings,
                                                    const char              *key);
VALENT_AVAILABLE_IN_1_0
void             valent_settings_set_string        (ValentSettings          *settings,
                                                    const char              *key,
                                                    const char              *val);
VALENT_AVAILABLE_IN_1_0
char          ** valent_settings_get_strv          (ValentSettings          *settings,
                                                    const char              *key);
VALENT_AVAILABLE_IN_1_0
void             valent_settings_set_strv          (ValentSettings          *settings,
                                                    const char              *key,
                                                    const char * const      *value);
VALENT_AVAILABLE_IN_1_0
uint32_t         valent_settings_get_uint32        (ValentSettings          *settings,
                                                    const char              *key);
VALENT_AVAILABLE_IN_1_0
void             valent_settings_set_uint32        (ValentSettings          *settings,
                                                    const char              *key,
                                                    uint32_t                 val);
VALENT_AVAILABLE_IN_1_0
uint64_t         valent_settings_get_uint64        (ValentSettings          *settings,
                                                    const char              *key);
VALENT_AVAILABLE_IN_1_0
void             valent_settings_set_uint64        (ValentSettings          *settings,
                                                    const char              *key,
                                                    uint64_t                 val);
VALENT_AVAILABLE_IN_1_0
void             valent_settings_reset             (ValentSettings          *settings,
                                                    const char              *key);
VALENT_AVAILABLE_IN_1_0
void             valent_settings_bind              (ValentSettings          *settings,
                                                    const char              *key,
                                                    gpointer                 object,
                                                    const char              *property,
                                                    GSettingsBindFlags       flags);
VALENT_AVAILABLE_IN_1_0
void             valent_settings_bind_with_mapping (ValentSettings          *settings,
                                                    const char              *key,
                                                    gpointer                 object,
                                                    const char              *property,
                                                    GSettingsBindFlags       flags,
                                                    GSettingsBindGetMapping  get_mapping,
                                                    GSettingsBindSetMapping  set_mapping,
                                                    gpointer                 user_data,
                                                    GDestroyNotify           destroy);
VALENT_AVAILABLE_IN_1_0
void             valent_settings_unbind            (ValentSettings          *settings,
                                                    const char              *property);

G_END_DECLS
