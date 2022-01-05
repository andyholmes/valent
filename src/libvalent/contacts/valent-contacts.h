// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CONTACTS_INSIDE) && !defined (VALENT_CONTACTS_COMPILATION)
# error "Only <libvalent-contacts.h> can be included directly."
#endif

#include <libvalent-core.h>

#include "valent-contact-store.h"


G_BEGIN_DECLS

/**
 * ValentPhoneNumberFlags:
 * @VALENT_PHONE_NUMBER_UNKNOWN: Equivalent to `VALENT_PHONE_NUMBER_VOICE`
 * @VALENT_PHONE_NUMBER_BBS: A BBS system
 * @VALENT_PHONE_NUMBER_CAR: A car phone number
 * @VALENT_PHONE_NUMBER_CELL: A cellular or mobile phone number
 * @VALENT_PHONE_NUMBER_FAX: A fax machine number
 * @VALENT_PHONE_NUMBER_HOME: A home or landline phone number
 * @VALENT_PHONE_NUMBER_ISDN: An ISDN service telephone number
 * @VALENT_PHONE_NUMBER_MODEM: A MODEM connected telephone number
 * @VALENT_PHONE_NUMBER_MSG: A telephone number with voice messaging support
 * @VALENT_PHONE_NUMBER_PAGER: A paging device telephone number
 * @VALENT_PHONE_NUMBER_PCS: A personal communication services telephone number
 * @VALENT_PHONE_NUMBER_PREF: Indicates a preferred or primary number
 * @VALENT_PHONE_NUMBER_VIDEO: A video conferencing telephone number
 * @VALENT_PHONE_NUMBER_VOICE: A voice capable phone number; the default type
 * @VALENT_PHONE_NUMBER_WORK: A work or office phone number
 *
 * Flags that describe a #ValentPhoneNumber type, capabilities or other
 * attributes such as whether the number is the preferred or primary number.
 *
 * These values are based on RFC2426 (vCard 3.0), however only `HOME`, `CELL`,
 * `WORK` and `PREF` are used in Valent.
 */
typedef enum
{
  VALENT_PHONE_NUMBER_UNKNOWN,
  VALENT_PHONE_NUMBER_BBS   = (1<<0),
  VALENT_PHONE_NUMBER_CAR   = (1<<1),
  VALENT_PHONE_NUMBER_CELL  = (1<<2),
  VALENT_PHONE_NUMBER_FAX   = (1<<3),
  VALENT_PHONE_NUMBER_HOME  = (1<<4),
  VALENT_PHONE_NUMBER_ISDN  = (1<<5),
  VALENT_PHONE_NUMBER_MODEM = (1<<6),
  VALENT_PHONE_NUMBER_MSG   = (1<<7),
  VALENT_PHONE_NUMBER_PAGER = (1<<8),
  VALENT_PHONE_NUMBER_PCS   = (1<<9),
  VALENT_PHONE_NUMBER_PREF  = (1<<10),
  VALENT_PHONE_NUMBER_VIDEO = (1<<11),
  VALENT_PHONE_NUMBER_VOICE = (1<<12),
  VALENT_PHONE_NUMBER_WORK  = (1<<13)
} ValentPhoneNumberFlags;


#define VALENT_TYPE_CONTACTS (valent_contacts_get_type ())

G_DECLARE_FINAL_TYPE (ValentContacts, valent_contacts, VALENT, CONTACTS, ValentComponent)

ValentContacts     * valent_contacts_get_default  (void);

ValentContactStore * valent_contacts_ensure_store (ValentContacts *contacts,
                                                   const char     *uid,
                                                   const char     *name);
ValentContactStore * valent_contacts_get_store    (ValentContacts *contacts,
                                                   const char     *uid);
GPtrArray          * valent_contacts_get_stores   (ValentContacts *contacts);

G_END_DECLS

