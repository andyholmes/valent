// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include "valent-object.h"

G_BEGIN_DECLS

#define VALENT_TYPE_APPLICATION_PLUGIN (valent_application_plugin_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentApplicationPlugin, valent_application_plugin, VALENT, APPLICATION_PLUGIN, ValentObject)

struct _ValentApplicationPluginClass
{
  ValentObjectClass   parent_class;

  /* virtual functions */
  void                (*disable)         (ValentApplicationPlugin  *plugin);
  void                (*enable)          (ValentApplicationPlugin  *plugin);
  gboolean            (*activate)        (ValentApplicationPlugin  *plugin);
  int                 (*command_line)    (ValentApplicationPlugin  *plugin,
                                          GApplicationCommandLine  *command_line);
  gboolean            (*dbus_register)   (ValentApplicationPlugin  *plugin,
                                          GDBusConnection          *connection,
                                          const char               *object_path,
                                          GError                  **error);
  void                (*dbus_unregister) (ValentApplicationPlugin  *plugin,
                                          GDBusConnection          *connection,
                                          const char               *object_path);
  gboolean            (*open)            (ValentApplicationPlugin  *plugin,
                                          GFile                   **files,
                                          int                       n_files,
                                          const char               *hint);
  void                (*shutdown)        (ValentApplicationPlugin  *plugin);
  void                (*startup)         (ValentApplicationPlugin  *plugin);

  /*< private >*/
  gpointer            padding[8];
};

VALENT_AVAILABLE_IN_1_0
GApplication * valent_application_plugin_get_application (ValentApplicationPlugin  *plugin);
VALENT_AVAILABLE_IN_1_0
void           valent_application_plugin_disable         (ValentApplicationPlugin  *plugin);
VALENT_AVAILABLE_IN_1_0
void           valent_application_plugin_enable          (ValentApplicationPlugin  *plugin);
VALENT_AVAILABLE_IN_1_0
gboolean       valent_application_plugin_activate        (ValentApplicationPlugin  *plugin);
VALENT_AVAILABLE_IN_1_0
int            valent_application_plugin_command_line    (ValentApplicationPlugin  *plugin,
                                                          GApplicationCommandLine  *command_line);
VALENT_AVAILABLE_IN_1_0
gboolean       valent_application_plugin_dbus_register   (ValentApplicationPlugin  *plugin,
                                                          GDBusConnection          *connection,
                                                          const char               *object_path,
                                                          GError                  **error);
VALENT_AVAILABLE_IN_1_0
void           valent_application_plugin_dbus_unregister (ValentApplicationPlugin  *plugin,
                                                          GDBusConnection          *connection,
                                                          const char               *object_path);
VALENT_AVAILABLE_IN_1_0
gboolean       valent_application_plugin_open            (ValentApplicationPlugin  *plugin,
                                                          GFile                   **files,
                                                          int                       n_files,
                                                          const char               *hint);
VALENT_AVAILABLE_IN_1_0
void           valent_application_plugin_shutdown        (ValentApplicationPlugin  *plugin);
VALENT_AVAILABLE_IN_1_0
void           valent_application_plugin_startup         (ValentApplicationPlugin  *plugin);

G_END_DECLS

