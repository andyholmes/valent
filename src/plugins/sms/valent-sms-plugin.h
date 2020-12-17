// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libpeas/peas.h>

G_BEGIN_DECLS

#define VALENT_TYPE_SMS_PLUGIN (valent_sms_plugin_get_type())

G_DECLARE_FINAL_TYPE (ValentSmsPlugin, valent_sms_plugin, VALENT, SMS_PLUGIN, PeasExtensionBase)

void                 valent_sms_plugin_request_conversation  (ValentSmsPlugin *self,
                                                              gint64           thread_id);

void                 valent_sms_plugin_request_conversations (ValentSmsPlugin *self);
void                 valent_sms_plugin_request               (ValentSmsPlugin *self,
                                                              const char      *address,
                                                              const char      *text);

G_END_DECLS

