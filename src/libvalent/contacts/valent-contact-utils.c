// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contact-utils"

#include "config.h"

#include <glib.h>

#include "valent-contact-utils.h"
#include "valent-eds.h"


/**
 * SECTION:valentcontactutils
 * @short_description: Utilities for working with contacts
 * @title: Contact Utilities
 * @stability: Unstable
 * @include: libvalent-contacts.h
 *
 * Helper functions and utilities for working with contacts and phone numbers.
 */


/**
 * valent_phone_number_normalize:
 * @number: a phone number string
 *
 * Return a normalized version of @number.
 *
 * Returns: (transfer full): a normalized phone number string
 */
char *
valent_phone_number_normalize (const char *number)
{
  g_autofree char *normalized = NULL;
  gsize i, len;
  gboolean start = TRUE;
  const char *s = number;

  g_return_val_if_fail (number != NULL, NULL);

  i = 0;
  len = strlen (number);
  normalized = g_new (char, len + 1);

  while (*s != '\0')
    {
      // Strip leading 0's
      if G_UNLIKELY (start)
        {
          if (*s == '0')
            {
              s++;
              continue;
            }

          start = FALSE;
        }

      if G_LIKELY (g_ascii_isdigit (*s))
        normalized[i++] = *s;

      s++;
    }
  normalized[i++] = '\0';
  normalized = g_realloc (normalized, i * sizeof (char));

  /* If we fail or the number is stripped completely, return the original */
  if G_UNLIKELY (normalized == NULL || g_utf8_strlen (normalized, -1) == 0)
    return g_strdup (number);

  return g_steal_pointer (&normalized);
}

static inline gboolean
valent_phone_number_compare_normalized (const char *number1,
                                        const char *number2)
{
  gsize number1_len, number2_len;

  g_assert (number1 != NULL);
  g_assert (number2 != NULL);

  number1_len = strlen (number1);
  number2_len = strlen (number2);

  if (number1_len > number2_len)
    return strcmp (number1 + number1_len - number2_len, number2) == 0;
  else
    return strcmp (number2 + number2_len - number1_len, number1) == 0;
}

/**
 * valent_phone_number_equal:
 * @number1: a phone number string
 * @number2: a phone number string
 *
 * Normalize and compare @number1 with @number2 and return %TRUE if they match
 * or %FALSE if they don't.
 *
 * Returns: %TRUE or %FALSE indicating equality
 */
gboolean
valent_phone_number_equal (const char *number1,
                           const char *number2)
{
  g_autofree char *normalized1 = NULL;
  g_autofree char *normalized2 = NULL;
  gsize num1_len, num2_len;

  g_return_val_if_fail (number1 != NULL, FALSE);
  g_return_val_if_fail (number2 != NULL, FALSE);

  normalized1 = valent_phone_number_normalize (number1);
  normalized2 = valent_phone_number_normalize (number2);

  num1_len = strlen (normalized1);
  num2_len = strlen (normalized2);

  if (num1_len > num2_len)
    return strcmp (normalized1 + num1_len - num2_len, normalized2) == 0;
  else
    return strcmp (normalized2 + num2_len - num1_len, normalized1) == 0;
}

/**
 * valent_phone_number_of_contact:
 * @contact: an #EContact
 * @number: a normalized phone number
 *
 * Check if @contact has @number as one of it's phone numbers.
 *
 * Since this function is typically used to test against a series of contacts, it is expected that
 * @number has already been normalized with valent_phone_number_normalize().
 *
 * Returns: %TRUE if @number belongs to the contact
 */
gboolean
valent_phone_number_of_contact (EContact   *contact,
                                const char *number)
{
  GList *numbers = NULL;
  gboolean ret = FALSE;

  g_return_val_if_fail (E_IS_CONTACT (contact), FALSE);
  g_return_val_if_fail (number != NULL, FALSE);

  numbers = e_contact_get (contact, E_CONTACT_TEL);

  for (const GList *iter = numbers; iter; iter = iter->next)
    {
      g_autofree char *normalized = NULL;

      normalized = valent_phone_number_normalize (iter->data);

      if ((ret = valent_phone_number_compare_normalized (number, normalized)))
        break;
    }
  g_list_free_full (numbers, g_free);

  return ret;
}

