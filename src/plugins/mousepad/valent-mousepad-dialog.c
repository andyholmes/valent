// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mousepad-dialog"

#include "config.h"

#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-input.h>
#include <libvalent-ui.h>
#include <math.h>

#include "valent-mousepad-dialog.h"
#include "valent-mousepad-keydef.h"

#define CAPTURE_THRESHOLD_MS 50


struct _ValentMousepadDialog
{
  AdwWindow           parent_instance;

  ValentDevice       *device;

  /* Pointer State */
  unsigned int        claimed : 1;
  guint32             last_t;
  double              last_v;
  double              last_x;
  double              last_y;
  unsigned int        longpress_id;
  int                 scale;

  /* Template widgets */
  GtkWidget          *editor;
  GtkEventController *keyboard;
  GtkWidget          *touchpad;
  GtkGesture         *pointer_scroll;
  GtkGesture         *touch_single;
  GtkGesture         *touch_double;
  GtkGesture         *touch_triple;
};

static void valent_mousepad_dialog_pointer_axis    (ValentMousepadDialog *self,
                                                    double                dx,
                                                    double                dy);
static void valent_mousepad_dialog_pointer_button  (ValentMousepadDialog *self,
                                                    unsigned int          button,
                                                    unsigned int          n_press);
static void valent_mousepad_dialog_pointer_motion  (ValentMousepadDialog *self,
                                                    double                dx,
                                                    double                dy);
static void valent_mousepad_dialog_pointer_press   (ValentMousepadDialog *self);
static void valent_mousepad_dialog_pointer_release (ValentMousepadDialog *self);
static void valent_mousepad_dialog_reset           (ValentMousepadDialog *self);

G_DEFINE_TYPE (ValentMousepadDialog, valent_mousepad_dialog, ADW_TYPE_WINDOW)

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static void
get_last_update_time (GtkGesture       *gesture,
                      GdkEventSequence *sequence,
                      guint32          *time)
{
  GdkEvent *event = NULL;

  if (sequence != NULL)
    event = gtk_gesture_get_last_event (gesture, sequence);

  if (event != NULL)
    *time = gdk_event_get_time (event);
}

/*
 * Keyboard Input
 */
static gboolean
on_key_pressed (GtkEventControllerKey *controller,
                unsigned int           keyval,
                unsigned int           keycode,
                GdkModifierType        state,
                ValentMousepadDialog  *self)
{
  GdkEvent *event;
  unsigned int special_key;
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_MOUSEPAD_DIALOG (self));

  /* Skip modifier keyvals */
  event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (controller));

  if (keyval == 0 || gdk_key_event_is_modifier (event))
    return TRUE;

  builder = valent_packet_start ("kdeconnect.mousepad.request");

  /* Check for control character or printable key */
  if ((special_key = valent_mousepad_keyval_to_keycode (keyval)) > 0)
    {
      json_builder_set_member_name (builder, "specialKey");
      json_builder_add_int_value (builder, special_key);
    }
  else
    {
      g_autoptr (GError) error = NULL;
      g_autofree char *key = NULL;
      gunichar wc;

      wc = gdk_keyval_to_unicode (keyval);
      key = g_ucs4_to_utf8 (&wc, 1, NULL, NULL, &error);

      if (key == NULL)
        {
          g_warning ("Converting %s to string: %s",
                     gdk_keyval_name (keyval),
                     error->message);
          g_object_unref (builder);
          return TRUE;
        }

      json_builder_set_member_name (builder, "key");
      json_builder_add_string_value (builder, key);
    }

  /* Check our supported modifiers */
  if (state & GDK_ALT_MASK)
    {
      json_builder_set_member_name (builder, "alt");
      json_builder_add_boolean_value (builder, TRUE);
    }

  if (state & GDK_CONTROL_MASK)
    {
      json_builder_set_member_name (builder, "ctrl");
      json_builder_add_boolean_value (builder, TRUE);
    }

  if (state & GDK_SHIFT_MASK)
    {
      json_builder_set_member_name (builder, "shift");
      json_builder_add_boolean_value (builder, TRUE);
    }

  if (state & GDK_SUPER_MASK)
    {
      json_builder_set_member_name (builder, "super");
      json_builder_add_boolean_value (builder, TRUE);
    }

  /* Request acknowledgment of the event */
  json_builder_set_member_name (builder, "sendAck");
  json_builder_add_boolean_value (builder, TRUE);

  packet = valent_packet_finish (builder);
  valent_device_queue_packet (self->device, packet);

  return TRUE;
}

static void
move_cursor (ValentMousepadDialog *dialog,
             GtkMovementStep       step,
             int                   count)
{
  g_signal_emit_by_name (dialog->editor, "move-cursor", step, count, FALSE);
}

/*
 * Pointer Input
 */
static inline gboolean
calculate_delta (ValentMousepadDialog *self,
                 double                dx,
                 double                dy,
                 guint32               dt,
                 double               *cx,
                 double               *cy)
{
  double dr, v, m;

  dr = sqrt (pow (dx, 2) + pow (dy, 2));
  v = dr / dt;

  if (self->last_v != 0.0)
    self->last_v = (v + self->last_v) / 2;
  else
    self->last_v = v;

  // TODO: acceleration setting
  m = pow (self->last_v, 1.0);
  m = fmin (4.0, fmax (m, 0.25));

  *cx = round (dx * m);
  *cy = round (dy * m);

  return dt >= CAPTURE_THRESHOLD_MS;
}

static gboolean
on_scroll (GtkEventControllerScroll *controller,
           double                    dx,
           double                    dy,
           ValentMousepadDialog     *self)
{
  valent_mousepad_dialog_pointer_axis (self, dx, dy);

  return TRUE;
}

static gboolean
longpress_timeout (gpointer data)
{
  ValentMousepadDialog *self = VALENT_MOUSEPAD_DIALOG (data);

  self->claimed = TRUE;
  self->longpress_id = 0;
  gtk_gesture_set_state (self->touch_single, GTK_EVENT_SEQUENCE_CLAIMED);

  valent_mousepad_dialog_pointer_press (self);

  return G_SOURCE_REMOVE;
}

static void
on_single_begin (GtkGestureDrag       *gesture,
                 double                start_x,
                 double                start_y,
                 ValentMousepadDialog *self)
{
  GdkEventSequence *sequence;
  unsigned int button = 0;
  guint32 time = 0;
  int delay;

  /* No drags or longpresses with these buttons */
  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button == GDK_BUTTON_MIDDLE || button == GDK_BUTTON_SECONDARY)
    return;

  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  get_last_update_time (GTK_GESTURE (gesture), sequence, &time);

  self->last_t = time;
  self->last_x = start_x;
  self->last_y = start_y;

  /* Start the longpress timeout */
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (self)),
                "gtk-long-press-time", &delay,
                NULL);

  self->longpress_id = g_timeout_add (delay, longpress_timeout, self);
  g_source_set_name_by_id (self->longpress_id, "[valent] longpress_timeout");
}

static void
on_single_update (GtkGesture           *gesture,
                  GdkEventSequence     *sequence,
                  ValentMousepadDialog *self)
{
  guint32 time = 0;
  double x, y;
  double dx, dy, dt;
  double cx, cy;

  get_last_update_time (gesture, sequence, &time);
  gtk_gesture_get_point (gesture, sequence, &x, &y);

  dt = time - self->last_t;
  dx = (x - self->last_x) * self->scale;
  dy = (y - self->last_y) * self->scale;

  if (!calculate_delta (self, dx, dy, dt, &cx, &cy))
    return;

  if (dx >= 1.0 || dx <= -1.0 || dy >= 1.0 || dy <= -1.0)
    {
      self->claimed = TRUE;
      g_clear_handle_id (&self->longpress_id, g_source_remove);
      gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);
    }
  else
    return;

  self->last_t = time;
  self->last_x = x;
  self->last_y = y;

  valent_mousepad_dialog_pointer_motion (self, cx, cy);
}

static void
on_single_end (GtkGestureDrag       *gesture,
               double                offset_x,
               double                offset_y,
               ValentMousepadDialog *self)
{
  if (!self->claimed)
    {
      unsigned int button = 0;

      button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
      valent_mousepad_dialog_pointer_button (self, button, 1);
    }

  valent_mousepad_dialog_reset (self);
}

static void
on_double_begin (GtkGestureDrag       *gesture,
                 double                start_x,
                 double                start_y,
                 ValentMousepadDialog *self)
{
  self->last_x = start_x;
  self->last_y = start_y;
}

static void
on_double_update (GtkGesture           *gesture,
                  GdkEventSequence     *sequence,
                  ValentMousepadDialog *self)
{
  double x, y;
  double dx, dy;

  gtk_gesture_get_point (gesture, sequence, &x, &y);
  dx = x - self->last_x;
  dy = y - self->last_y;

  /* NOTE: We only support the Y-axis */
  if (dy >= 1.0 || dy <= -1.0)
    {
      self->claimed = TRUE;
      gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);
    }
  else
    return;

  self->last_x = x;
  self->last_y = y;

  valent_mousepad_dialog_pointer_axis (self, round (dx), round (dy));
}

static void
on_double_end (GtkGestureDrag       *gesture,
               double                offset_x,
               double                offset_y,
               ValentMousepadDialog *self)
{
  if (!self->claimed)
    valent_mousepad_dialog_pointer_button (self, GDK_BUTTON_SECONDARY, 1);

  valent_mousepad_dialog_reset (self);
}

static void
on_triple_end (GtkGestureDrag       *gesture,
               double                offset_x,
               double                offset_y,
               ValentMousepadDialog *self)
{
  if (!self->claimed)
    valent_mousepad_dialog_pointer_button (self, GDK_BUTTON_MIDDLE, 1);

  valent_mousepad_dialog_reset (self);
}

static void
valent_mousepad_dialog_pointer_axis (ValentMousepadDialog *self,
                                     double                dx,
                                     double                dy)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  /* NOTE: We only support the Y-axis */
  dx = 0.0;

  builder = valent_packet_start ("kdeconnect.mousepad.request");
  json_builder_set_member_name (builder, "dx");
  json_builder_add_double_value (builder, dx);
  json_builder_set_member_name (builder, "dy");
  json_builder_add_double_value (builder, dy);
  json_builder_set_member_name (builder, "scroll");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static void
valent_mousepad_dialog_pointer_button (ValentMousepadDialog *self,
                                       unsigned int          button,
                                       unsigned int          n_press)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  if (n_press == 1)
    {
      builder = valent_packet_start ("kdeconnect.mousepad.request");

      switch (button)
        {
        case GDK_BUTTON_PRIMARY:
          json_builder_set_member_name (builder, "singleclick");
          json_builder_add_boolean_value (builder, TRUE);
          break;

        case GDK_BUTTON_MIDDLE:
          json_builder_set_member_name (builder, "middleclick");
          json_builder_add_boolean_value (builder, TRUE);
          break;

        case GDK_BUTTON_SECONDARY:
          json_builder_set_member_name (builder, "rightclick");
          json_builder_add_boolean_value (builder, TRUE);
          break;

        default:
          g_object_unref (builder);
          g_return_if_reached ();
        }

      packet = valent_packet_finish (builder);
    }
  else if (button == GDK_BUTTON_PRIMARY && n_press == 2)
    {
      builder = valent_packet_start ("kdeconnect.mousepad.request");
      json_builder_set_member_name (builder, "doubleclick");
      json_builder_add_boolean_value (builder, TRUE);
      packet = valent_packet_finish (builder);
    }

  if (packet != NULL)
    valent_device_queue_packet (self->device, packet);
}

static void
valent_mousepad_dialog_pointer_motion (ValentMousepadDialog *self,
                                       double                dx,
                                       double                dy)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  builder = valent_packet_start ("kdeconnect.mousepad.request");
  json_builder_set_member_name (builder, "dx");
  json_builder_add_double_value (builder, dx);
  json_builder_set_member_name (builder, "dy");
  json_builder_add_double_value (builder, dy);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static void
valent_mousepad_dialog_pointer_press (ValentMousepadDialog *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  builder = valent_packet_start ("kdeconnect.mousepad.request");
  json_builder_set_member_name (builder, "singlehold");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static void
valent_mousepad_dialog_pointer_release (ValentMousepadDialog *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  builder = valent_packet_start ("kdeconnect.mousepad.request");
  json_builder_set_member_name (builder, "singlerelease");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static void
valent_mousepad_dialog_reset (ValentMousepadDialog *self)
{
  self->claimed = FALSE;
  self->last_t = 0;
  self->last_v = 0.0;
  self->last_x = 0.0;
  self->last_y = 0.0;
  g_clear_handle_id (&self->longpress_id, g_source_remove);
}

/*
 * GObject
 */
static void
valent_mousepad_dialog_dispose (GObject *object)
{
  ValentMousepadDialog *self = VALENT_MOUSEPAD_DIALOG (object);

  valent_mousepad_dialog_reset (self);

  G_OBJECT_CLASS (valent_mousepad_dialog_parent_class)->dispose (object);
}

static void
valent_mousepad_dialog_finalize (GObject *object)
{
  ValentMousepadDialog *self = VALENT_MOUSEPAD_DIALOG (object);

  g_clear_object (&self->device);

  G_OBJECT_CLASS (valent_mousepad_dialog_parent_class)->finalize (object);
}

static void
valent_mousepad_dialog_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ValentMousepadDialog *self = VALENT_MOUSEPAD_DIALOG (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mousepad_dialog_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ValentMousepadDialog *self = VALENT_MOUSEPAD_DIALOG (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mousepad_dialog_class_init (ValentMousepadDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = valent_mousepad_dialog_dispose;
  object_class->finalize = valent_mousepad_dialog_finalize;
  object_class->get_property = valent_mousepad_dialog_get_property;
  object_class->set_property = valent_mousepad_dialog_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/mousepad/valent-mousepad-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentMousepadDialog, editor);
  gtk_widget_class_bind_template_child (widget_class, ValentMousepadDialog, keyboard);
  gtk_widget_class_bind_template_child (widget_class, ValentMousepadDialog, pointer_scroll);
  gtk_widget_class_bind_template_child (widget_class, ValentMousepadDialog, touchpad);
  gtk_widget_class_bind_template_child (widget_class, ValentMousepadDialog, touch_single);
  gtk_widget_class_bind_template_child (widget_class, ValentMousepadDialog, touch_double);
  gtk_widget_class_bind_template_child (widget_class, ValentMousepadDialog, touch_triple);

  gtk_widget_class_bind_template_callback (widget_class, on_key_pressed);
  gtk_widget_class_bind_template_callback (widget_class, on_scroll);
  gtk_widget_class_bind_template_callback (widget_class, on_single_begin);
  gtk_widget_class_bind_template_callback (widget_class, on_single_update);
  gtk_widget_class_bind_template_callback (widget_class, on_single_end);
  gtk_widget_class_bind_template_callback (widget_class, on_double_begin);
  gtk_widget_class_bind_template_callback (widget_class, on_double_update);
  gtk_widget_class_bind_template_callback (widget_class, on_double_end);
  gtk_widget_class_bind_template_callback (widget_class, on_triple_end);

  properties [PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "The target device",
                         VALENT_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_mousepad_dialog_init (ValentMousepadDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_gesture_group (self->touch_single, self->touch_double);
  gtk_gesture_group (self->touch_single, self->touch_triple);

  self->scale = gtk_widget_get_scale_factor (GTK_WIDGET (self));
}

/**
 * valent_mousepad_dialog_new:
 * @device: a #ValentDevice
 *
 * Create a new input dialog for sending input events to @device.
 *
 * Returns: (transfer full): a new #ValentMousepadDialog
 */
ValentMousepadDialog *
valent_mousepad_dialog_new (ValentDevice *device)
{
  GApplication *application = NULL;
  GtkWindow *window = NULL;

  application = g_application_get_default ();

  if (GTK_IS_APPLICATION (application))
    window = gtk_application_get_active_window (GTK_APPLICATION (application));

  return g_object_new (VALENT_TYPE_MOUSEPAD_DIALOG,
                       "device",        device,
                       "transient-for", window,
                       NULL);
}

/**
 * valent_mousepad_dialog_echo_key:
 * @dialog: a #ValentMousepadDialog
 * @packet: a #JsonNode
 *
 * Handle the remote device's acknowledgement of a key we sent.
 */
void
valent_mousepad_dialog_echo_key (ValentMousepadDialog *dialog,
                                 const char           *key,
                                 GdkModifierType       mask)
{
  GtkTextBuffer *buffer;
  g_autofree char *old_text = NULL;
  g_autofree char *new_text = NULL;

  g_return_if_fail (VALENT_IS_MOUSEPAD_DIALOG (dialog));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->editor));

  if (!!(mask & GDK_CONTROL_MASK) && g_strcmp0 (key, "a") == 0)
    {
      g_signal_emit_by_name (dialog->editor,
                             "select-all",
                             !!(mask & GDK_SHIFT_MASK));
    }
  else
    {
      g_object_get (buffer, "text", &old_text, NULL);
      new_text = g_strjoin ("", old_text, key, NULL);
      g_object_set (buffer, "text", new_text, NULL);
    }
}

/**
 * valent_mousepad_dialog_echo_special:
 * @dialog: a #ValentMousepadDialog
 * @packet: a #JsonNode
 *
 * Handle the remote device's acknowledgement of a special key we sent.
 */
void
valent_mousepad_dialog_echo_special (ValentMousepadDialog *dialog,
                                     unsigned int          keyval,
                                     GdkModifierType       mask)
{
  GtkTextBuffer *buffer;
  g_autofree char *old_text = NULL;
  g_autofree char *new_text = NULL;

  g_return_if_fail (VALENT_IS_MOUSEPAD_DIALOG (dialog));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->editor));

  switch (keyval)
    {
    case GDK_KEY_BackSpace:
      g_signal_emit_by_name (dialog->editor, "backspace");
      break;

    case GDK_KEY_Linefeed:
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
      g_object_get (buffer, "text", &old_text, NULL);
      new_text = g_strjoin ("", old_text, "\n", NULL);
      g_object_set (buffer, "text", new_text, NULL);
      break;

    /* Home/End in terms of "real lines", since the other side probably doesn't
     * work in terms of "display lines".
     */
    case GDK_KEY_Home:
      move_cursor (dialog, GTK_MOVEMENT_PARAGRAPH_ENDS, -1);
      break;

    case GDK_KEY_End:
      move_cursor (dialog, GTK_MOVEMENT_PARAGRAPH_ENDS, 1);
      break;

    case GDK_KEY_Page_Up:
      move_cursor (dialog, GTK_MOVEMENT_PAGES, -1);
      break;

    case GDK_KEY_Page_Down:
      move_cursor (dialog, GTK_MOVEMENT_PAGES, 1);
      break;

    /* We Up/Down in terms of "real lines", for the same reason as above. */
    case GDK_KEY_Up:
      move_cursor (dialog, GTK_MOVEMENT_PARAGRAPHS, -1);
      break;

    case GDK_KEY_Down:
      move_cursor (dialog, GTK_MOVEMENT_PARAGRAPHS, 1);
      break;

    case GDK_KEY_Left:
      move_cursor (dialog, GTK_MOVEMENT_VISUAL_POSITIONS, -1);
      break;

    case GDK_KEY_Right:
      move_cursor (dialog, GTK_MOVEMENT_VISUAL_POSITIONS, 1);
      break;
    }
}

