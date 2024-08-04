// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>
#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CONTACT_PAGE (valent_contact_page_get_type())

G_DECLARE_FINAL_TYPE (ValentContactPage, valent_contact_page, VALENT, CONTACT_PAGE, AdwNavigationPage)

GtkWidget          * valent_contact_page_new               (ValentContactStore *store);
ValentContactStore * valent_contact_page_get_contact_store (ValentContactPage  *page);
void                 valent_contact_page_set_contact_store (ValentContactPage  *page,
                                                            ValentContactStore *store);
const char         * valent_contact_page_get_iri           (ValentContactPage  *page);
void                 valent_contact_page_set_iri           (ValentContactPage  *page,
                                                            const char         *iri);

G_END_DECLS
