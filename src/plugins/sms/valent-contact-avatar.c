// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contact-avatar"

#include "config.h"

#include <math.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <pango/pango.h>

#include "valent-contact-avatar.h"
#include "valent-eds.h"


struct _ValentContactAvatar
{
  GtkWidget     parent_instance;

  EContact     *contact;

  /* Avatar Cache */
  guint         loaded : 1;
  GdkTexture   *photo;
  PangoLayout  *letter;
  GdkPaintable *icon;
  GdkRGBA      *bg_color;
};

G_DEFINE_TYPE (ValentContactAvatar, valent_contact_avatar, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_CONTACT,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static GLoadableIcon *
_e_contact_dup_gicon (EContact *contact)
{
  GLoadableIcon *icon = NULL;
  EContactPhoto *photo;
  const guchar *data;
  gsize len;
  const char *uri;

  /* Check for a photo */
  photo = e_contact_get (contact, E_CONTACT_PHOTO);

  if (photo == NULL)
    return NULL;

  /* Try for inlined data */
  if (photo->type == E_CONTACT_PHOTO_TYPE_INLINED &&
      (data = e_contact_photo_get_inlined (photo, &len)))
    {
      g_autoptr (GBytes) bytes = NULL;

      bytes = g_bytes_new (data, len);
      icon = G_LOADABLE_ICON (g_bytes_icon_new (bytes));
    }

  /* Try for URI */
  if (photo->type == E_CONTACT_PHOTO_TYPE_URI &&
      (uri = e_contact_photo_get_uri (photo)))
    {
      g_autoptr (GFile) file = NULL;

      file = g_file_new_for_uri (uri);
      icon = G_LOADABLE_ICON (g_file_icon_new (file));
    }

  e_contact_photo_free (photo);

  return icon;
}

static GdkRGBA
color_for_string (const char *str)
{
  // https://gitlab.gnome.org/Community/Design/HIG-app-icons/blob/master/GNOME%20HIG.gpl
  static gdouble gnome_color_palette[][3] = {
    {  98, 160, 234 },
    {  53, 132, 228 },
    {  28, 113, 216 },
    {  26,  95, 180 },
    {  87, 227, 137 },
    {  51, 209, 122 },
    {  46, 194, 126 },
    {  38, 162, 105 },
    { 248, 228,  92 },
    { 246, 211,  45 },
    { 245, 194,  17 },
    { 229, 165,  10 },
    { 255, 163,  72 },
    { 255, 120,   0 },
    { 230,  97,   0 },
    { 198,  70,   0 },
    { 237,  51,  59 },
    { 224,  27,  36 },
    { 192,  28,  40 },
    { 165,  29,  45 },
    { 192,  97, 203 },
    { 163,  71, 186 },
    { 129,  61, 156 },
    {  97,  53, 131 },
    { 181, 131,  90 },
    { 152, 106,  68 },
    { 134,  94,  60 },
    {  99,  69,  44 }
  };

  GdkRGBA color = { 255, 255, 255, 1.0 };
  guint hash;
  gint number_of_colors;
  gint idx;

  if (str == NULL || strlen (str) == 0)
    return color;

  hash = g_str_hash (str);
  number_of_colors = G_N_ELEMENTS (gnome_color_palette);
  idx = hash % number_of_colors;

  color.red   = gnome_color_palette[idx][0] / 255;
  color.green = gnome_color_palette[idx][1] / 255;
  color.blue  = gnome_color_palette[idx][2] / 255;

  return color;
}

static char *
get_letter_from_contact (EContact *contact)
{
  const char *full;
  gunichar wc;

  g_assert (E_IS_CONTACT (contact));

  full = e_contact_get_const (contact, E_CONTACT_FULL_NAME);

  if (g_utf8_strlen (full, 1) == 0)
    return NULL;

  wc = g_utf8_get_char (full);
  wc = g_unichar_toupper (wc);

  if (g_unichar_isdigit (wc))
    return NULL;

  return g_ucs4_to_utf8 (&wc, 1, NULL, NULL, NULL);
}

static void
load_color (ValentContactAvatar *avatar)
{
  GdkRGBA color;
  g_autofree char *name = NULL;

  if (avatar->contact)
    name = e_contact_get (avatar->contact, E_CONTACT_FULL_NAME);

  if (name == NULL || g_utf8_strlen (name, 1) == 0)
    name = g_uuid_string_random ();

  color = color_for_string (name);
  avatar->bg_color = gdk_rgba_copy (&color);
}

static void
load_icon (ValentContactAvatar *avatar,
           int                  size,
           int                  scale)
{
  GtkWidget *widget = GTK_WIDGET (avatar);
  GtkIconTheme *theme;
  GtkIconPaintable *icon;
  const char *icon_names[] = {"avatar-default", NULL};

  if (avatar->bg_color == NULL)
    load_color (avatar);

  /* Size */
  theme = gtk_icon_theme_get_for_display (gtk_widget_get_display(widget));
  icon = gtk_icon_theme_lookup_icon (theme,
                                     "avatar-default-symbolic",
                                     icon_names,
                                     floor (0.75 * size),
                                     scale,
                                     GTK_TEXT_DIR_NONE,
                                     GTK_ICON_LOOKUP_FORCE_SYMBOLIC);

  avatar->icon = GDK_PAINTABLE (icon);
  avatar->loaded = TRUE;
}

static void
snapshot_icon (ValentContactAvatar *avatar,
               GtkSnapshot       *snapshot,
               graphene_rect_t   *bounds)
{
  const GdkRGBA *fg = &(GdkRGBA){1.0, 1.0, 1.0, 0.95};
  int width, height;
  graphene_matrix_t fg_matrix;
  graphene_vec4_t fg_offset;

  /* Set the background color */
  gtk_snapshot_append_color (snapshot, avatar->bg_color, bounds);

  /* Push the foreground color */
  graphene_matrix_init_from_float (&fg_matrix,
                                   (float[16]) {
                                     0.0, 0.0, 0.0, 0.0,
                                     0.0, 0.0, 0.0, 0.0,
                                     0.0, 0.0, 0.0, 0.0,
                                     fg->red, fg->green, fg->blue, fg->alpha
                                   });
  graphene_vec4_init (&fg_offset, 0.0, 0.0, 0.0, 0.0);
  gtk_snapshot_push_color_matrix (snapshot, &fg_matrix, &fg_offset);

  /* Icon Offset */
  width = gdk_paintable_get_intrinsic_width (avatar->icon);
  height = gdk_paintable_get_intrinsic_height (avatar->icon);
  gtk_snapshot_translate (snapshot,
                          &GRAPHENE_POINT_INIT ((bounds->size.width - width) / 2,
                                                (bounds->size.height - height) / 2));

  gdk_paintable_snapshot (avatar->icon, snapshot, width, height);

  /* Pop the foreground color */
  gtk_snapshot_pop (snapshot);
}

static gboolean
load_photo (ValentContactAvatar *avatar,
            int                  size,
            int                  scale)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GLoadableIcon) icon = NULL;
  g_autoptr (GInputStream) stream = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;

  if (avatar->contact)
    icon = _e_contact_dup_gicon (avatar->contact);

  if (icon == NULL)
    return FALSE;

  stream = g_loadable_icon_load (icon, -1, NULL, NULL, &error);

  if (stream == NULL)
    {
      g_warning ("Loading icon for %s: %s",
                 (const char *)e_contact_get_const (avatar->contact,
                                                    E_CONTACT_FULL_NAME),
                 error->message);
      return FALSE;
    }

  pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream,
                                                size,
                                                size,
                                                TRUE,
                                                NULL,
                                                &error);

  if (pixbuf == NULL)
    {
      g_warning ("Loading icon for %s: %s",
                 (const char *)e_contact_get_const (avatar->contact,
                                                    E_CONTACT_FULL_NAME),
                 error->message);
      return FALSE;
    }

  /* Cache the surface */
  avatar->photo = gdk_texture_new_for_pixbuf (pixbuf);
  avatar->loaded = TRUE;

  return TRUE;
}

static gboolean
load_letter (ValentContactAvatar *avatar,
             unsigned int         size)
{
  PangoFontDescription *font_desc;
  PangoContext *context;
  g_autofree char *initial = NULL;
  g_autofree char *font = NULL;

  if (avatar->contact)
    initial = get_letter_from_contact (avatar->contact);

  if (initial == NULL)
    return FALSE;

  if (avatar->bg_color == NULL)
    load_color (avatar);

  font = g_strdup_printf ("Sans %d", (int)ceil (size / 2.5));

  /* Draw the initials on top */
  context = gtk_widget_get_pango_context (GTK_WIDGET (avatar));
  avatar->letter = pango_layout_new (context);
  pango_layout_set_text (avatar->letter, initial, -1);
  font_desc = pango_font_description_from_string (font);
  pango_layout_set_font_description (avatar->letter, font_desc);
  pango_font_description_free (font_desc);

  avatar->loaded = TRUE;
  return TRUE;
}

static void
snapshot_letter (ValentContactAvatar *avatar,
                 GtkSnapshot         *snapshot,
                 graphene_rect_t     *bounds)
{
  GdkRGBA fg_color = {1.0, 1.0, 1.0, 0.96};
  gint x_offset, y_offset;
  PangoRectangle rect;
  graphene_point_t offset;

  /* Append the background color */
  gtk_snapshot_append_color (snapshot, avatar->bg_color, bounds);

  /* Push a subtle shadow for lighter backgrounds */
  gtk_snapshot_push_shadow (snapshot,
                            &(GskShadow){{0.0, 0.0, 0.0, 0.25}, 0.0, 0.0, 2.0},
                            1);

  /* Offset the text based on ink extents */
  pango_layout_get_pixel_extents (avatar->letter, &rect, NULL);
  x_offset = (bounds->size.width - rect.width)/2;
  y_offset = (bounds->size.height - rect.height)/2;
  graphene_point_init (&offset, x_offset - rect.x, y_offset - rect.y);
  gtk_snapshot_translate (snapshot, &offset);

  gtk_snapshot_append_layout (snapshot, avatar->letter, &fg_color);
  gtk_snapshot_pop (snapshot);
}

static void
prepare_snapshot (ValentContactAvatar *avatar,
                  int                  width,
                  int                  height)
{
  gint scale, size;

  scale = gtk_widget_get_scale_factor (GTK_WIDGET (avatar));
  size = MAX (width, height);

  if (load_photo (avatar, size, scale))
    return;

  if (load_letter (avatar, size))
    return;

  load_icon (avatar, width, scale);
}

/*
 * GtkWidget
 */
static void
valent_contact_avatar_measure (GtkWidget      *widget,
                               GtkOrientation  orientation,
                               int             for_size,
                               int            *minimum,
                               int            *natural,
                               int            *minimum_baseline,
                               int            *natural_baseline)
{
  *minimum = *natural = 0;
}

static void
valent_contact_avatar_snapshot (GtkWidget   *widget,
                                GdkSnapshot *snapshot)
{
  ValentContactAvatar *avatar = VALENT_CONTACT_AVATAR (widget);
  int width, height;
  graphene_size_t radius;
  graphene_rect_t bounds;
  GskRoundedRect clip;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);
  bounds = GRAPHENE_RECT_INIT(0, 0, width, height);
  radius = GRAPHENE_SIZE_INIT (width / 2, height / 2);

  if G_UNLIKELY (!avatar->loaded)
    prepare_snapshot (avatar, width, height);

  /* Clip to a circle */
  gsk_rounded_rect_init (&clip, &bounds, &radius, &radius, &radius, &radius);
  gtk_snapshot_push_rounded_clip (snapshot, &clip);

  /* Contact Photo */
  if (avatar->photo)
    gtk_snapshot_append_texture (snapshot, avatar->photo, &bounds);

  /* Contact Initial */
  else if (avatar->letter)
    snapshot_letter (avatar, snapshot, &bounds);

  /* Default Avatar */
  else
    snapshot_icon (avatar, snapshot, &bounds);

  /* Pop the clip */
  gtk_snapshot_pop (snapshot);
}

/*
 * GObject
 */
static void
valent_contact_avatar_finalize (GObject *object)
{
  ValentContactAvatar *self = VALENT_CONTACT_AVATAR (object);

  g_clear_object (&self->contact);
  g_clear_object (&self->icon);
  g_clear_object (&self->photo);
  g_clear_object (&self->letter);
  g_clear_pointer (&self->bg_color, gdk_rgba_free);

  G_OBJECT_CLASS (valent_contact_avatar_parent_class)->finalize (object);
}

static void
valent_contact_avatar_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ValentContactAvatar *self = VALENT_CONTACT_AVATAR (object);

  switch (prop_id)
    {
    case PROP_CONTACT:
      g_value_set_object (value, self->contact);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contact_avatar_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ValentContactAvatar *self = VALENT_CONTACT_AVATAR (object);

  switch (prop_id)
    {
    case PROP_CONTACT:
      valent_contact_avatar_set_contact (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contact_avatar_class_init (ValentContactAvatarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = valent_contact_avatar_finalize;
  object_class->get_property = valent_contact_avatar_get_property;
  object_class->set_property = valent_contact_avatar_set_property;

  widget_class->measure = valent_contact_avatar_measure;
  widget_class->snapshot = valent_contact_avatar_snapshot;

  /**
   * ValentContactAvatar:contact
   *
   * The #EContact for the avatar.
   */
  properties [PROP_CONTACT] =
    g_param_spec_object ("contact",
                         "Contact",
                         "The contact for the avatar",
                         E_TYPE_CONTACT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_contact_avatar_init (ValentContactAvatar *self)
{
}

/**
 * valent_contact_avatar_new:
 * @contact: (nullable): an #EContact
 *
 * Create a contact avatar for @contact.
 *
 * Returns: (transfer full): a #GtkWidget
 */
GtkWidget *
valent_contact_avatar_new (EContact *contact)
{
  return g_object_new (VALENT_TYPE_CONTACT_AVATAR,
                       "contact",        contact,
                       "height-request", 32,
                       "width-request",  32,
                       NULL);
}

/**
 * valent_contact_avatar_copy:
 * @avatar: (nullable): an #ValentContactAvatar
 *
 * Create a copy of @avatar, taking references on data instead of copying.
 *
 * Returns: (transfer full): a #GtkWidget
 */
GtkWidget *
valent_contact_avatar_copy (ValentContactAvatar *avatar)
{
  ValentContactAvatar *copy;
  GtkWidget *widget = GTK_WIDGET (avatar);

  copy = g_object_new (VALENT_TYPE_CONTACT_AVATAR,
                       "contact",        avatar->contact,
                       "height-request", gtk_widget_get_height (widget),
                       "width-request",  gtk_widget_get_width (widget),
                       NULL);

  if (avatar->bg_color)
    copy->bg_color = gdk_rgba_copy (copy->bg_color);

  if (avatar->letter)
    avatar->letter = g_object_ref (avatar->letter);

  if (avatar->photo != NULL)
    copy->photo = g_object_ref (avatar->photo);
  else if (avatar->letter)
    avatar->letter = g_object_ref (avatar->letter);
  else if (avatar->icon)
    avatar->icon = g_object_ref (avatar->icon);

  return GTK_WIDGET (copy);
}

/**
 * valent_contact_avatar_get_contact:
 * @avatar: a #ValentContactAvatar
 *
 * Get the #EContact for @avatar.
 *
 * Returns: (transfer none) (nullable): a #EContact
 */
EContact *
valent_contact_avatar_get_contact (ValentContactAvatar *avatar)
{
  g_return_val_if_fail (VALENT_IS_CONTACT_AVATAR (avatar), NULL);

  return avatar->contact;
}

/**
 * valent_contact_avatar_set_contact:
 * @avatar: a #ValentContactAvatar
 * @contact: a #EContact
 *
 * Set the #EContact for @avatar to @contact.
 */
void
valent_contact_avatar_set_contact (ValentContactAvatar *avatar,
                                   EContact            *contact)
{
  gboolean redraw = FALSE;

  g_return_if_fail (VALENT_IS_CONTACT_AVATAR (avatar));
  g_return_if_fail (contact == NULL || E_IS_CONTACT (contact));

  if (g_set_object (&avatar->contact, contact))
    {
      redraw = avatar->loaded;

      g_clear_pointer (&avatar->bg_color, gdk_rgba_free);
      g_clear_object (&avatar->photo);
      g_clear_object (&avatar->letter);
      g_clear_object (&avatar->icon);

      avatar->loaded = FALSE;
    }

  if (redraw)
    gtk_widget_queue_draw (GTK_WIDGET (avatar));
}
