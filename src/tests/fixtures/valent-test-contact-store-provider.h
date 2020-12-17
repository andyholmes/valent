// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-core.h>
#include <libvalent-contacts.h>

G_BEGIN_DECLS

#define VALENT_TYPE_TEST_CONTACT_STORE_PROVIDER (valent_test_contact_store_provider_get_type ())

G_DECLARE_FINAL_TYPE (ValentTestContactStoreProvider, valent_test_contact_store_provider, VALENT, TEST_CONTACT_STORE_PROVIDER, ValentContactStoreProvider)

G_END_DECLS

