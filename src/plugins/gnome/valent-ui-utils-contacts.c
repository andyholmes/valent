// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-ui-messages"

#include "config.h"

#include <adwaita.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>
#include <valent.h>

#include "valent-ui-utils-private.h"

G_DEFINE_QUARK (VALENT_CONTACT_ICON, valent_contact_icon)
G_DEFINE_QUARK (VALENT_CONTACT_PAINTABLE, valent_contact_paintable)


static GLoadableIcon *
_e_contact_get_icon (EContact *contact)
{
  GLoadableIcon *icon = NULL;
  g_autoptr (EContactPhoto) photo = NULL;
  const unsigned char *data;
  size_t len;
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

  icon = _e_contact_get_icon (contact);
  if (icon == NULL)
    return NULL;

  stream = g_loadable_icon_load (icon, -1, NULL, NULL, error);
  if (stream == NULL)
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
 * @avatar: a `AdwAvatar`
 * @contact: a `EContact`
 *
 * Set the `GdkPaintable` for @avatar from @contact.
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

  adw_avatar_set_custom_image (avatar, paintable);
  adw_avatar_set_show_initials (avatar, paintable != NULL);
  adw_avatar_set_text (avatar, name);
}

static void
valent_contact_store_lookup_contact_cb (ValentContactStore *store,
                                        GAsyncResult       *result,
                                        gpointer            user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  const char *medium = g_task_get_task_data (task);
  g_autoslist (GObject) contacts = NULL;
  EContact *contact = NULL;
  GError *error = NULL;

  contacts = valent_contact_store_query_finish (store, result, &error);
  if (error != NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (contacts != NULL)
    {
      contact = g_object_ref (contacts->data);
    }
  else
    {
      g_autoptr (EPhoneNumber) number = NULL;

      contact = e_contact_new ();
      number = e_phone_number_from_string (medium, NULL, NULL);
      if (number != NULL)
        {
          g_autofree char *name = NULL;

          name = e_phone_number_to_string (number,
                                           E_PHONE_NUMBER_FORMAT_NATIONAL);
          e_contact_set (contact, E_CONTACT_FULL_NAME, name);
          e_contact_set (contact, E_CONTACT_PHONE_OTHER, medium);
        }
      else
        {
          e_contact_set (contact, E_CONTACT_FULL_NAME, medium);
          if (g_strrstr (medium, "@") != NULL)
            e_contact_set (contact, E_CONTACT_EMAIL_1, medium);
          else
            e_contact_set (contact, E_CONTACT_PHONE_OTHER, medium);
        }
    }

  g_task_return_pointer (task, g_steal_pointer (&contact), g_object_unref);
}

/**
 * valent_contact_store_lookup_contact:
 * @store: a `ValentContactStore`
 * @medium: a contact medium
 * @cancellable: (nullable): `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: user supplied data
 *
 * A convenience wrapper around [method@Valent.ContactStore.query] for finding a
 * contact by phone number or email address.
 *
 * Call valent_contact_store_lookup_contact_finish() to get the result.
 */
void
valent_contact_store_lookup_contact (ValentContactStore  *store,
                                     const char          *medium,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (EBookQuery) query = NULL;
  g_autofree char *sexp = NULL;

  g_return_if_fail (VALENT_IS_CONTACT_STORE (store));
  g_return_if_fail (medium != NULL && *medium != '\0');
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_contact_store_lookup_contact);
  g_task_set_task_data (task, g_strdup (medium), g_free);

  if (g_strrstr (medium, "@") != NULL)
    {
      query = e_book_query_field_test (E_CONTACT_EMAIL,
                                       E_BOOK_QUERY_IS,
                                       medium);
    }
  else
    {
      query = e_book_query_field_test (E_CONTACT_TEL,
                                       E_BOOK_QUERY_EQUALS_SHORT_PHONE_NUMBER,
                                       medium);
    }

  sexp = e_book_query_to_string (query);
  valent_contact_store_query (store,
                              sexp,
                              cancellable,
                              (GAsyncReadyCallback)valent_contact_store_lookup_contact_cb,
                              g_object_ref (task));
}

/**
 * valent_contact_store_lookup_contact_finish:
 * @store: a `ValentContactStore`
 * @result: a `GAsyncResult`
 * @error: (nullable): a `GError`
 *
 * Finish an operation started by valent_contact_store_lookup_contact().
 *
 * Returns: (transfer full): an `EContact`
 */
EContact *
valent_contact_store_lookup_contact_finish (ValentContactStore  *store,
                                            GAsyncResult        *result,
                                            GError             **error)
{
  g_return_val_if_fail (VALENT_IS_CONTACT_STORE (store), NULL);
  g_return_val_if_fail (g_task_is_valid (result, store), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}
