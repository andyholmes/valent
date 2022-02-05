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

