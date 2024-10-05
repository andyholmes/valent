// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <gio/gio.h>
#include <libebook-contacts/libebook-contacts.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "../core/valent-version.h"

G_BEGIN_DECLS

VALENT_AVAILABLE_IN_1_0
TrackerResource * valent_contact_resource_from_econtact (EContact *contact);

G_END_DECLS
