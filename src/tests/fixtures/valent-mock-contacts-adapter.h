// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-contacts.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_CONTACTS_ADAPTER (valent_mock_contacts_adapter_get_type ())

G_DECLARE_FINAL_TYPE (ValentMockContactsAdapter, valent_mock_contacts_adapter, VALENT, MOCK_CONTACTS_ADAPTER, ValentContactsAdapter)

G_END_DECLS

