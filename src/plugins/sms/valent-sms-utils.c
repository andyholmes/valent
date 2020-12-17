// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <gdk/gdk.h>
#include <libvalent-contacts.h>

#include "valent-sms-utils.h"


static void
on_size_prepared (GdkPixbufLoader *loader,
                  int              width,
                  int              height,
                  int             *size)
{
  gdk_pixbuf_loader_set_size (loader, 32, 32);
}

/**
 * valent_sms_contact_photo_func:
 * @size: target diameter
 * @user_data: an #EContact
 *
 * An #AdwAvatarImageLoadFunc for #EContact photos.
 *
 * Typical usage:
 *
 * |[<!-- language="C" -->
 * adw_avatar_set_image_load_func (ADW_AVATAR (row->avatar),
 *                                 valent_sms_contact_photo_func,
 *                                 g_object_ref (row->contact),
 *                                 g_object_unref);
 * ]|
 *
 * Returns: (transfer full) (nullable): a #GdkPixbuf
 */
GdkPixbuf *
valent_sms_contact_photo_func (int      size,
                               gpointer user_data)
{
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  g_autoptr (GError) error = NULL;
  EContactPhoto *photo;
  const guchar *data;
  gsize len;
  const char *uri;

  if (user_data == NULL)
    return NULL;

  /* Check for a photo */
  if ((photo = e_contact_get (E_CONTACT (user_data), E_CONTACT_PHOTO)) == NULL)
    return NULL;

  /* Try for inlined data */
  if (photo->type == E_CONTACT_PHOTO_TYPE_INLINED &&
      (data = e_contact_photo_get_inlined (photo, &len)))
    {
      g_autoptr (GdkPixbufLoader) loader = NULL;

      loader = gdk_pixbuf_loader_new ();

      g_signal_connect (loader,
                        "size-prepared",
                        G_CALLBACK (on_size_prepared),
                        &size);

      if (!gdk_pixbuf_loader_write (loader, data, len, &error) ||
          !gdk_pixbuf_loader_close (loader, &error))
        g_warning ("Loading avatar: %s", error->message);

      pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

      if (pixbuf != NULL)
        g_object_ref (pixbuf);
    }

  /* Try for URI */
  if (photo->type == E_CONTACT_PHOTO_TYPE_URI &&
      (uri = e_contact_photo_get_uri (photo)))
    {
      g_autoptr (GFile) file = NULL;
      const char *path;
      int width, height;

      file = g_file_new_for_uri (uri);
      path = g_file_peek_path (file);
      gdk_pixbuf_get_file_info (path, &width, &height);
      pixbuf = gdk_pixbuf_new_from_file_at_scale (path,
                                                  (width <= height) ? size : -1,
                                                  (width >= height) ? size : -1,
                                                  TRUE,
                                                  &error);

      if (error != NULL)
        g_warning ("Loading avatar: %s", error->message);
    }

  e_contact_photo_free (photo);

  return g_steal_pointer (&pixbuf);
}
