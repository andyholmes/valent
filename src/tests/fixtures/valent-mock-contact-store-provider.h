// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_TEST_INSIDE) && !defined (VALENT_TEST_COMPILATION)
# error "Only <libvalent-test.h> can be included directly."
#endif

#include <libvalent-contacts.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_CONTACT_STORE_PROVIDER (valent_mock_contact_store_provider_get_type ())

G_DECLARE_FINAL_TYPE (ValentMockContactStoreProvider, valent_mock_contact_store_provider, VALENT, MOCK_CONTACT_STORE_PROVIDER, ValentContactStoreProvider)

ValentContactStoreProvider * valent_mock_contact_store_provider_get_instance (void);

G_END_DECLS

