// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define VALENT_TYPE_PREFERENCES_COMMAND_EDITOR (valent_preferences_command_editor_get_type())

G_DECLARE_FINAL_TYPE (ValentPreferencesCommandEditor, valent_preferences_command_editor, VALENT, PREFERENCES_COMMAND_EDITOR, AdwDialog)

GVariant   * valent_preferences_command_editor_get_command (ValentPreferencesCommandEditor *editor);
void         valent_preferences_command_editor_set_command (ValentPreferencesCommandEditor *editor,
                                                            GVariant                       *command);
const char * valent_preferences_command_editor_get_uuid    (ValentPreferencesCommandEditor *editor);
void         valent_preferences_command_editor_set_uuid    (ValentPreferencesCommandEditor *editor,
                                                            const char                     *uuid);

G_END_DECLS
