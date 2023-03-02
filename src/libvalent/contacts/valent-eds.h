// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
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
#if !(EDS_CHECK_VERSION(3,47,0))
G_DEFINE_AUTOPTR_CLEANUP_FUNC (EBookBackendFactory, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (EBookClientView, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (EBookQuery, e_book_query_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (EContact, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (EContactPhoto, e_contact_photo_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (EVCardAttribute, e_vcard_attribute_free)
#endif /* !(EDS_CHECK_VERSION(3,47,0)) */

