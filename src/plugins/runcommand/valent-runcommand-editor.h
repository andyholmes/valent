// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VALENT_TYPE_RUNCOMMAND_EDITOR (valent_runcommand_editor_get_type())

G_DECLARE_FINAL_TYPE (ValentRuncommandEditor, valent_runcommand_editor, VALENT, RUNCOMMAND_EDITOR, GtkDialog)

GtkDialog  * valent_runcommand_editor_new         (void);

void         valent_runcommand_editor_clear       (ValentRuncommandEditor *editor);
const char * valent_runcommand_editor_get_command (ValentRuncommandEditor *editor);
void         valent_runcommand_editor_set_command (ValentRuncommandEditor *editor,
                                                   const char             *command);
const char * valent_runcommand_editor_get_name    (ValentRuncommandEditor *editor);
void         valent_runcommand_editor_set_name    (ValentRuncommandEditor *editor,
                                                   const char             *name);
const char * valent_runcommand_editor_get_uuid    (ValentRuncommandEditor *editor);
void         valent_runcommand_editor_set_uuid    (ValentRuncommandEditor *editor,
                                                   const char             *uuid);

G_END_DECLS
