// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CONTACTS_INSIDE) && !defined (VALENT_CONTACTS_COMPILATION)
# error "Only <libvalent-contacts.h> can be included directly."
#endif

#include <glib.h>

/*
 * Silence deprecation warnings
 */
#define EDS_DISABLE_DEPRECATED
G_GNUC_BEGIN_IGNORE_DEPRECATIONS

#include <libebook/libebook.h>
#include <libebook-contacts/libebook-contacts.h>
#include <libedata-book/libedata-book.h>
#include <libedataserver/libedataserver.h>

G_GNUC_END_IGNORE_DEPRECATIONS


/*
 * Autocleanups
 */
#ifndef glib_autoptr_clear_EBookBackendFactory
G_DEFINE_AUTOPTR_CLEANUP_FUNC (EBookBackendFactory, g_object_unref)
#endif

#ifndef glib_autoptr_clear_EBookClientView
G_DEFINE_AUTOPTR_CLEANUP_FUNC (EBookClientView, g_object_unref)
#endif

#ifndef glib_autoptr_clear_EBookQuery
G_DEFINE_AUTOPTR_CLEANUP_FUNC (EBookQuery, e_book_query_unref)
#endif

#ifndef glib_autoptr_clear_EContact
G_DEFINE_AUTOPTR_CLEANUP_FUNC (EContact, g_object_unref)
#endif

#ifndef glib_autoptr_clear_EContactPhoto
G_DEFINE_AUTOPTR_CLEANUP_FUNC (EContactPhoto, e_contact_photo_free)
#endif

#ifndef glib_autoptr_clear_EVCardAttribute
G_DEFINE_AUTOPTR_CLEANUP_FUNC (EVCardAttribute, e_vcard_attribute_free)
#endif

