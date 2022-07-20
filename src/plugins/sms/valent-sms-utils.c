// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sms-utils"

#include "config.h"

#include <adwaita.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>
#include <libvalent-contacts.h>

#include "valent-sms-utils.h"


G_DEFINE_QUARK (VALENT_CONTACT_ICON, valent_contact_icon)
G_DEFINE_QUARK (VALENT_CONTACT_PAINTABLE, valent_contact_paintable)


static GLoadableIcon *
_e_contact_get_icon (EContact *contact)
{
  GLoadableIcon *icon = NULL;
  g_autoptr (EContactPhoto) photo = NULL;
  const guchar *data;
  gsize len;
  const char *uri;

  g_assert (E_IS_CONTACT (contact));

  icon = g_object_get_qdata (G_OBJECT (contact), valent_contact_icon_quark ());

  if (G_IS_LOADABLE_ICON (icon))
    return icon;

  if ((photo = e_contact_get (contact, E_CONTACT_PHOTO)) == NULL)
    return NULL;

  if (photo->type == E_CONTACT_PHOTO_TYPE_INLINED &&
      (data = e_contact_photo_get_inlined (photo, &len)))
    {
      g_autoptr (GBytes) bytes = NULL;

      bytes = g_bytes_new (data, len);
      icon = G_LOADABLE_ICON (g_bytes_icon_new (bytes));
    }
  else if (photo->type == E_CONTACT_PHOTO_TYPE_URI &&
           (uri = e_contact_photo_get_uri (photo)))
    {
      g_autoptr (GFile) file = NULL;

      file = g_file_new_for_uri (uri);
      icon = G_LOADABLE_ICON (g_file_icon_new (file));
    }

  if (G_IS_LOADABLE_ICON (icon))
    {
      g_object_set_qdata_full (G_OBJECT (contact),
                               valent_contact_icon_quark (),
                               icon,
                               g_object_unref);
    }

  return icon;
}

static GdkPaintable *
_e_contact_get_paintable (EContact  *contact,
                          int        size,
                          int        scale,
                          GError   **error)
{
  GdkPaintable *paintable = NULL;
  GLoadableIcon *icon = NULL;
  g_autoptr (GInputStream) stream = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;

  g_assert (E_IS_CONTACT (contact));
  g_assert (size > 0);
  g_assert (scale > 0);

  paintable = g_object_get_qdata (G_OBJECT (contact),
                                  valent_contact_paintable_quark ());

  if (GDK_IS_PAINTABLE (paintable))
    return paintable;

  if ((icon = _e_contact_get_icon (contact)) == NULL)
    return NULL;

  if ((stream = g_loadable_icon_load (icon, -1, NULL, NULL, error)) == NULL)
    return NULL;

  pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream,
                                                size,
                                                size,
                                                TRUE,
                                                NULL,
                                                error);

  if (pixbuf == NULL)
    return NULL;

  paintable = GDK_PAINTABLE (gdk_texture_new_for_pixbuf (pixbuf));
  g_object_set_qdata_full (G_OBJECT (contact),
                           valent_contact_paintable_quark (),
                           paintable,
                           g_object_unref);

  return paintable;
}

/**
 * valent_sms_avatar_from_contact:
 * @avatar: a #AdwAvatar
 * @contact: a #EContact
 *
 * Set the #GdkPaintable for @avatar from @contact.
 */
void
valent_sms_avatar_from_contact (AdwAvatar *avatar,
                                EContact  *contact)
{
  GdkPaintable *paintable;
  const char *name;
  int size, scale;

  g_return_if_fail (ADW_IS_AVATAR (avatar));
  g_return_if_fail (E_IS_CONTACT (contact));

  size = adw_avatar_get_size (avatar);
  scale = gtk_widget_get_scale_factor (GTK_WIDGET (avatar));
  paintable = _e_contact_get_paintable (contact, size, scale, NULL);

  name = e_contact_get_const (contact, E_CONTACT_FULL_NAME);
  adw_avatar_set_text (avatar, name);

  adw_avatar_set_custom_image (avatar, paintable);
  adw_avatar_set_show_initials (avatar, paintable != NULL);
}

static void
valent_sms_contact_from_phone_cb (ValentContactStore *store,
                                  GAsyncResult       *result,
                                  gpointer            user_data)
{
  g_autoptr (GTask) task = user_data;
  const char *number = g_task_get_task_data (task);
  g_autoslist (GObject) contacts = NULL;
  EContact *contact = NULL;
  GError *error = NULL;

  VALENT_ENTRY;

  contacts = valent_contact_store_query_finish (store, result, &error);

  if (error != NULL)
    {
      g_task_return_error (task, error);
      VALENT_EXIT;
    }

  /* Prefer using libphonenumber */
  if (e_phone_number_is_supported ())
    {
      if (contacts != NULL)
        contact = g_object_ref (contacts->data);
    }
  else
    {
      g_autofree char *normalized = NULL;

      normalized = valent_phone_number_normalize (number);

      for (const GSList *iter = contacts; iter; iter = iter->next)
        {
          if (valent_phone_number_of_contact (iter->data, normalized))
            {
              contact = g_object_ref (iter->data);
              break;
            }
        }
    }

  if (contact == NULL)
    {
      contact = e_contact_new ();
      e_contact_set (contact, E_CONTACT_FULL_NAME, number);
      e_contact_set (contact, E_CONTACT_PHONE_OTHER, number);
    }

  g_task_return_pointer (task, contact, g_object_unref);

  VALENT_EXIT;
}

/**
 * valent_sms_contact_from_phone:
 * @store: a #ValentContactStore
 * @phone: a phone number
 * @cancellable: (nullable): #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * A convenience wrapper around [method@Valent.ContactStore.query] for finding a
 * contact by phone number.
 *
 * Call valent_sms_contact_from_phone_finish() to get the result.
 */
void
valent_sms_contact_from_phone (ValentContactStore  *store,
                               const char          *number,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (EBookQuery) query = NULL;
  g_autofree char *sexp = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));
  g_return_if_fail (number != NULL && *number != '\0');
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_sms_contact_from_phone);
  g_task_set_task_data (task, g_strdup (number), g_free);

  /* Prefer using libphonenumber */
  if (e_phone_number_is_supported ())
    {
      query = e_book_query_field_test (E_CONTACT_TEL,
                                       E_BOOK_QUERY_EQUALS_SHORT_PHONE_NUMBER,
                                       number);
      sexp = e_book_query_to_string (query);
    }
  else
    {
      query = e_book_query_field_exists (E_CONTACT_TEL);
      sexp = e_book_query_to_string (query);
    }

  valent_contact_store_query (store,
                              sexp,
                              cancellable,
                              (GAsyncReadyCallback)valent_sms_contact_from_phone_cb,
                              g_steal_pointer (&task));

  VALENT_EXIT;
}

/**
 * valent_sms_contact_from_phone_finish:
 * @store: a #ValentContactStore
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by valent_sms_contact_from_phone().
 *
 * Returns: (transfer full): an #EContact
 */
EContact *
valent_sms_contact_from_phone_finish (ValentContactStore  *store,
                                      GAsyncResult        *result,
                                      GError             **error)
{
  EContact *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CONTACT_STORE (store), NULL);
  g_return_val_if_fail (g_task_is_valid (result, store), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  VALENT_RETURN (ret);
}

/**
 * valent_phone_number_normalize:
 * @number: a phone number string
 *
 * Return a normalized version of @number.
 *
 * Returns: (transfer full): a normalized phone number string
 *
 * Since: 1.0
 */
char *
valent_phone_number_normalize (const char *number)
{
  g_autofree char *normalized = NULL;
  gsize i, len;
  const char *s = number;

  g_return_val_if_fail (number != NULL, NULL);

#ifndef __clang_analyzer__
  i = 0;
  len = strlen (number);
  normalized = g_new (char, len + 1);

  while (*s == '0')
    s++;

  while (*s != '\0')
    {
      if G_LIKELY (g_ascii_isdigit (*s))
        normalized[i++] = *s;

      s++;
    }
  normalized[i++] = '\0';
  normalized = g_realloc (normalized, i * sizeof (char));

  /* If we fail or the number is stripped completely, return the original */
  if G_UNLIKELY (*normalized == '\0')
    return g_strdup (number);
#endif /* __clang_analyzer__ */

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
 *
 * Since: 1.0
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

