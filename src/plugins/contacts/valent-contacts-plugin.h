// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libpeas/peas.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CONTACTS_PLUGIN (valent_contacts_plugin_get_type())

G_DECLARE_FINAL_TYPE (ValentContactsPlugin, valent_contacts_plugin, VALENT, CONTACTS_PLUGIN, PeasExtensionBase)

G_END_DECLS

