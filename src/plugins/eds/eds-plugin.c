// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <libpeas.h>
#include <valent.h>

#include "valent-ebook-adapter.h"


_VALENT_EXTERN void
valent_eds_plugin_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_CONTACTS_ADAPTER,
                                              VALENT_TYPE_EBOOK_ADAPTER);
}

