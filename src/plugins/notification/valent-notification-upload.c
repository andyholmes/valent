// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-notification-upload"

#include "config.h"

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#ifdef HAVE_GLYCIN
#include <glycin.h>
#endif /* HAVE_GLYCIN */
#ifdef HAVE_GTK4
#include <gtk/gtk.h>
#endif /* HAVE_GTK4 */

#include "valent-notification-upload.h"

#define DEFAULT_ICON_SIZE 512


/**
 * ValentNotificationUpload:
 *
 * A class for notification icon uploads.
 *
 * `ValentNotificationUpload` is a class that abstracts uploading notifications
 * with icon payloads for `ValentNotificationPlugin`.
 */

struct _ValentNotificationUpload
{
  ValentTransfer  parent_instance;

  ValentDevice   *device;
  JsonNode       *packet;
  GIcon          *icon;
};

G_DEFINE_FINAL_TYPE (ValentNotificationUpload, valent_notification_upload, VALENT_TYPE_TRANSFER)

typedef enum {
  PROP_DEVICE = 1,
  PROP_ICON,
  PROP_PACKET,
} ValentNotificationUploadProperty;

static GParamSpec *properties[PROP_PACKET + 1] = { NULL, };


#ifdef HAVE_GTK4
static GtkIconTheme *
_gtk_icon_theme_get_default (void)
{
  static GtkIconTheme *icon_theme = NULL;
  size_t guard = 0;

  if (g_once_init_enter (&guard))
    {
      if (gtk_is_initialized ())
        {
          GdkDisplay *display = NULL;

          if ((display = gdk_display_get_default ()) != NULL)
            icon_theme = gtk_icon_theme_get_for_display (display);
        }

      g_once_init_leave (&guard, 1);
    }

  return icon_theme;
}

static int
_gtk_icon_theme_get_largest_icon (GtkIconTheme *theme,
                                  const char   *name)
{
  g_autofree int *sizes = NULL;
  int ret = 0;

  g_assert (GTK_IS_ICON_THEME (theme));
  g_assert (name != NULL);

  if (!gtk_icon_theme_has_icon (theme, name))
    return ret;

  sizes = gtk_icon_theme_get_icon_sizes (theme, name);

  for (unsigned int i = 0; sizes[i] != 0; i++)
    {
      if (sizes[i] == -1)
        return -1;

      if (sizes[i] > ret)
        ret = sizes[i];
    }

  return ret;
}

static GFile *
get_largest_icon_file (GIcon   *icon,
                       GError **error)
{
  GtkIconTheme *icon_theme = NULL;
  const char * const *names;

  g_assert (G_IS_THEMED_ICON (icon));

  icon_theme = _gtk_icon_theme_get_default ();
  if (icon_theme == NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Failed to load icon theme");
      return NULL;
    }

  names = g_themed_icon_get_names (G_THEMED_ICON (icon));
  for (size_t i = 0; names[i]; i++)
    {
      g_autoptr (GtkIconPaintable) paintable = NULL;
      int size;

      size = _gtk_icon_theme_get_largest_icon (icon_theme, names[i]);
      if (size == 0)
        continue;

      paintable = gtk_icon_theme_lookup_icon (icon_theme,
                                              names[i],
                                              NULL,
                                              size,
                                              1,
                                              GTK_TEXT_DIR_NONE,
                                              0);
      if (paintable != NULL)
        {
          g_autoptr (GFile) ret = NULL;

          ret = gtk_icon_paintable_get_file (paintable);
          if (ret != NULL)
            return g_steal_pointer (&ret);
        }
    }

  g_set_error_literal (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_FOUND,
                       "Failed to find icon");
  return NULL;
}
#endif /* HAVE_GTK4 */

#ifdef HAVE_GLYCIN
static GBytes *
_glycin_get_encoded_bytes (GBytes        *bytes,
                           GCancellable  *cancellable,
                           GError       **error)
{
  g_autoptr (GlyLoader) loader = NULL;
  g_autoptr (GlyImage) image = NULL;
  const char *mime_type = NULL;
  g_autoptr (GError) warn = NULL;

  g_assert (bytes != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* Attempt to load the image data
   */
  loader = gly_loader_new_for_bytes (bytes);
  image = gly_loader_load (loader, &warn);
  if (image == NULL)
    {
      g_debug ("%s(): Loading image: %s", G_STRFUNC, warn->message);
      return g_bytes_ref (bytes);
    }

  /* Attempt to convert the image data
   *
   * NOTE: kdeconnect-android only seems to accept PNG/JPEG
   */
  mime_type = gly_image_get_mime_type (image);
  if (g_strcmp0 (mime_type, "image/jpeg") != 0 &&
      g_strcmp0 (mime_type, "image/png") != 0)
    {
      g_autoptr (GlyFrame) frame = NULL;
      g_autoptr (GlyCreator) creator = NULL;
      g_autoptr (GlyNewFrame) new_frame = NULL;
      g_autoptr (GlyEncodedImage) encoded_image = NULL;

      /* Attempt to resize scalable icons
       */
      if (g_strcmp0 (mime_type, "image/svg+xml") == 0)
        {
          g_autoptr (GlyFrameRequest) request = NULL;

          request = gly_frame_request_new ();
          gly_frame_request_set_scale (request, DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE);
          frame = gly_image_get_specific_frame (image, request, &warn);
        }
      else
        {
          frame = gly_image_next_frame (image, &warn);
        }

      if (frame == NULL)
        {
          g_debug ("%s(): Requesting frame: %s", G_STRFUNC, warn->message);
          return g_bytes_ref (bytes);
        }

      creator = gly_creator_new ("image/png", &warn);
      if (creator == NULL)
        {
          g_debug ("%s(): Creating image: %s", G_STRFUNC, warn->message);
          return g_bytes_ref (bytes);
        }

      new_frame = gly_creator_add_frame_with_stride (creator,
                                                     gly_frame_get_width (frame),
                                                     gly_frame_get_height (frame),
                                                     gly_frame_get_stride (frame),
                                                     gly_frame_get_memory_format (frame),
                                                     gly_frame_get_buf_bytes (frame),
                                                     &warn);
      if (new_frame == NULL)
        {
          g_debug ("%s(): Adding frame: %s", G_STRFUNC, warn->message);
          return g_bytes_ref (bytes);
        }

      encoded_image = gly_creator_create (creator, &warn);
      if (encoded_image == NULL)
        {
          g_debug ("%s(): Encoding image: %s", G_STRFUNC, warn->message);
          return g_bytes_ref (bytes);
        }

      return gly_encoded_image_get_data (encoded_image);
    }

  return g_bytes_ref (bytes);
}
#endif /* HAVE_GLYCIN */

static GBytes *
valent_notification_upload_get_icon_bytes (GIcon         *icon,
                                           GCancellable  *cancellable,
                                           GError       **error)
{
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GError) warn = NULL;

  g_assert (G_IS_ICON (icon));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* Attempt to read the image data
   */
  if (G_IS_BYTES_ICON (icon))
    {
      GBytes *bytes_;

      bytes_ = g_bytes_icon_get_bytes (G_BYTES_ICON (icon));
      bytes = g_bytes_ref (bytes_);
    }
  else if (G_IS_FILE_ICON (icon))
    {
      GFile *file;

      file = g_file_icon_get_file (G_FILE_ICON (icon));
      bytes = g_file_load_bytes (file, cancellable, NULL, error);
    }
#ifdef HAVE_GTK4
  else if (G_IS_THEMED_ICON (icon))
    {
      g_autoptr (GFile) file = NULL;

      file = get_largest_icon_file (icon, error);
      if (file != NULL)
        bytes = g_file_load_bytes (file, cancellable, NULL, error);
    }
#endif /* HAVE_GTK4 */
  else
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "Unsupported icon type \"%s\"",
                   G_OBJECT_TYPE_NAME (icon));
    }

  /* If Glycin is available, attempt to resize and re-encode as 512px PNG
   */
#ifdef HAVE_GLYCIN
  if (bytes != NULL)
    return _glycin_get_encoded_bytes (bytes, cancellable, error);
#endif /* HAVE_GLYCIN */

  return g_steal_pointer (&bytes);
}

/*
 * ValentTransfer
 */
static void
valent_notification_upload_execute_task (GTask        *task,
                                         gpointer      source_object,
                                         gpointer      task_data,
                                         GCancellable *cancellable)
{
  ValentNotificationUpload *self = VALENT_NOTIFICATION_UPLOAD (source_object);
  g_autoptr (ValentChannel) channel = NULL;
  g_autoptr (GIcon) icon = NULL;
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GIOStream) target = NULL;
  g_autofree char *payload_hash = NULL;
  const uint8_t *payload_data = NULL;
  size_t payload_size = 0;
  gboolean ret = FALSE;
  GError *error = NULL;

  g_assert (VALENT_IS_NOTIFICATION_UPLOAD (self));

  if (g_task_return_error_if_cancelled (task))
    return;

  valent_object_lock (VALENT_OBJECT (self));
  channel = valent_device_ref_channel (self->device);
  icon = g_object_ref (self->icon);
  packet = json_node_ref (self->packet);
  valent_object_unlock (VALENT_OBJECT (self));

  if (channel == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_CONNECTED,
                               "Device is disconnected");
      return;
    }

  /* Try to get the icon bytes */
  bytes = valent_notification_upload_get_icon_bytes (icon, cancellable, &error);

  if (bytes == NULL)
    return g_task_return_error (task, error);

  /* A payload hash is included, allowing the remote device to ignore icon
   * transfers that it already has cached. */
  payload_data = g_bytes_get_data (bytes, &payload_size);
  payload_hash = g_compute_checksum_for_data (G_CHECKSUM_MD5,
                                              payload_data,
                                              payload_size);
  json_object_set_string_member (valent_packet_get_body (packet),
                                 "payloadHash",
                                 payload_hash);
  valent_packet_set_payload_size (packet, payload_size);

  target = valent_channel_upload (channel, packet, cancellable, &error);

  if (target == NULL)
    return g_task_return_error (task, error);

  /* Upload the icon */
  ret = g_output_stream_write_all (g_io_stream_get_output_stream (target),
                                   payload_data,
                                   payload_size,
                                   NULL,
                                   cancellable,
                                   &error);

  if (!ret)
    return g_task_return_error (task, error);

  g_task_return_boolean (task, TRUE);
}

static void
valent_notification_upload_execute (ValentTransfer      *transfer,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_NOTIFICATION_UPLOAD (transfer));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (transfer, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_notification_upload_execute);
  g_task_run_in_thread (task, valent_notification_upload_execute_task);
}

/*
 * GObject
 */
static void
valent_notification_upload_finalize (GObject *object)
{
  ValentNotificationUpload *self = VALENT_NOTIFICATION_UPLOAD (object);

  valent_object_lock (VALENT_OBJECT (self));
  g_clear_object (&self->device);
  g_clear_object (&self->icon);
  g_clear_pointer (&self->packet, json_node_unref);
  valent_object_unlock (VALENT_OBJECT (self));

  G_OBJECT_CLASS (valent_notification_upload_parent_class)->finalize (object);
}

static void
valent_notification_upload_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  ValentNotificationUpload *self = VALENT_NOTIFICATION_UPLOAD (object);

  switch ((ValentNotificationUploadProperty)prop_id)
    {
    case PROP_DEVICE:
      valent_object_lock (VALENT_OBJECT (self));
      g_value_set_object (value, self->device);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    case PROP_ICON:
      valent_object_lock (VALENT_OBJECT (self));
      g_value_set_object (value, self->icon);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    case PROP_PACKET:
      valent_object_lock (VALENT_OBJECT (self));
      g_value_set_boxed (value, self->packet);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_notification_upload_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ValentNotificationUpload *self = VALENT_NOTIFICATION_UPLOAD (object);

  switch ((ValentNotificationUploadProperty)prop_id)
    {
    case PROP_DEVICE:
      valent_object_lock (VALENT_OBJECT (self));
      self->device = g_value_dup_object (value);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    case PROP_ICON:
      valent_object_lock (VALENT_OBJECT (self));
      self->icon = g_value_dup_object (value);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    case PROP_PACKET:
      valent_object_lock (VALENT_OBJECT (self));
      self->packet = g_value_dup_boxed (value);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_notification_upload_class_init (ValentNotificationUploadClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentTransferClass *transfer_class = VALENT_TRANSFER_CLASS (klass);

  object_class->finalize = valent_notification_upload_finalize;
  object_class->get_property = valent_notification_upload_get_property;
  object_class->set_property = valent_notification_upload_set_property;

  transfer_class->execute = valent_notification_upload_execute;

  /**
   * ValentNotificationUpload:device:
   *
   * The [class@Valent.Device] this transfer is for.
   */
  properties [PROP_DEVICE] =
    g_param_spec_object ("device", NULL, NULL,
                         VALENT_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotificationUpload:icon:
   *
   * The [iface@Gio.Icon] for the notification.
   */
  properties [PROP_ICON] =
    g_param_spec_object ("icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotificationUpload:packet:
   *
   * The packet to send the payload with.
   */
  properties [PROP_PACKET] =
    g_param_spec_boxed ("packet", NULL, NULL,
                        JSON_TYPE_NODE,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_notification_upload_init (ValentNotificationUpload *self)
{
}

/**
 * valent_notification_upload_new:
 * @device: a `ValentDevice`
 * @packet: a KDE Connect packet
 * @icon: a `GIcon`
 *
 * Create a new `ValentNotificationUpload` for @packet and @icon.
 *
 * Returns: (transfer full): a `ValentNotificationUpload`
 */
ValentTransfer *
valent_notification_upload_new (ValentDevice *device,
                                JsonNode     *packet,
                                GIcon        *icon)
{
  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);
  g_return_val_if_fail (VALENT_IS_PACKET (packet), NULL);
  g_return_val_if_fail (G_IS_ICON (icon), NULL);

  return g_object_new (VALENT_TYPE_NOTIFICATION_UPLOAD,
                       "device", device,
                       "icon",   icon,
                       "packet", packet,
                       NULL);
}

