// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libpeas/peas.h>
#include <libvalent-contacts.h>

#include "valent-ebook-provider.h"


G_MODULE_EXPORT void
valent_eds_plugin_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_CONTACT_STORE_PROVIDER,
                                              VALENT_TYPE_EBOOK_PROVIDER);
}

