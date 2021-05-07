// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-input-dialog"

#include "config.h"

#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-input.h>
#include <libvalent-ui.h>
#include <math.h>

#include "valent-input-dialog.h"

#define CAPTURE_THRESHOLD_MS 50


struct _ValentInputDialog
{
  AdwWindow           parent_instance;

  ValentDevice       *device;

  /* Keyboard */
  GtkEventController *keyboard;

  /* Pointer */
  GtkGesture         *touch1;
  GtkGesture         *touch2;
  GtkGesture         *touch3;

  unsigned int        claimed : 1;
  guint32             last_t;
  double              last_v;
  double              last_x;
  double              last_y;
  unsigned int        longpress_id;
  int                 scale;

  /* Template widgets */
  GtkWidget          *touchpad;
  GtkWidget          *editor;
};

static void valent_input_dialog_pointer_axis    (ValentInputDialog *self,
                                                 double             dx,
                                                 double             dy);
static void valent_input_dialog_pointer_button  (ValentInputDialog *self,
                                                 unsigned int       button,
                                                 unsigned int       n_press);
static void valent_input_dialog_pointer_motion  (ValentInputDialog *self,
                                                 double             dx,
                                                 double             dy);
static void valent_input_dialog_pointer_press   (ValentInputDialog *self);
static void valent_input_dialog_pointer_release (ValentInputDialog *self);
static void valent_input_dialog_reset           (ValentInputDialog *self);

G_DEFINE_TYPE (ValentInputDialog, valent_input_dialog, ADW_TYPE_WINDOW)

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
static inline guint
get_special_key (guint key)
{
  switch (key)
    {
    case GDK_KEY_BackSpace:
      return 1;
    case GDK_KEY_Tab:
      return 2;
    case GDK_KEY_Linefeed:
      return 3;
    case GDK_KEY_Left:
      return 4;
    case GDK_KEY_Up:
      return 5;
    case GDK_KEY_Right:
      return 6;
    case GDK_KEY_Down:
      return 7;
    case GDK_KEY_Page_Up:
      return 8;
    case GDK_KEY_Page_Down:
      return 9;
    case GDK_KEY_Home:
      return 10;
    case GDK_KEY_End:
      return 11;
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
      return 12;
    case GDK_KEY_Delete:
      return 13;
    case GDK_KEY_Escape:
      return 14;
    case GDK_KEY_Sys_Req:
      return 15;
    case GDK_KEY_Scroll_Lock:
      return 16;

    case GDK_KEY_F1:
      return 21;
    case GDK_KEY_F2:
      return 22;
    case GDK_KEY_F3:
      return 23;
    case GDK_KEY_F4:
      return 24;
    case GDK_KEY_F5:
      return 25;
    case GDK_KEY_F6:
      return 26;
    case GDK_KEY_F7:
      return 27;
    case GDK_KEY_F8:
      return 28;
    case GDK_KEY_F9:
      return 29;
    case GDK_KEY_F10:
      return 30;
    case GDK_KEY_F11:
      return 31;
    case GDK_KEY_F12:
      return 32;

    default:
      return 0;
    }
}

static inline gboolean
is_alt (guint keyval)
{
  return (keyval == GDK_KEY_Alt_L || keyval == GDK_KEY_Alt_R);
}

static inline gboolean
is_ctrl (guint keyval)
{
  return (keyval == GDK_KEY_Control_L || keyval == GDK_KEY_Control_R);
}

static inline gboolean
is_shift (guint keyval)
{
  return (keyval == GDK_KEY_Shift_L || keyval == GDK_KEY_Shift_R);
}

static inline gboolean
is_super (guint keyval)
{
  return (keyval == GDK_KEY_Super_L || keyval == GDK_KEY_Super_R);
}

static gboolean
on_key_pressed (GtkEventControllerKey *controller,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        state,
                ValentInputDialog     *self)
{
  GdkEvent *event;
  GdkModifierType real_mask = 0;
  guint keyval_lower = 0, special_key;
  JsonBuilder *builder;
  g_autoptr (JsonNode) request = NULL;

  g_assert (VALENT_IS_INPUT_DIALOG (self));

  // Skip modifier keyvals
  event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (controller));

  if (gdk_key_event_is_modifier (event))
    return TRUE;

  keyval_lower = gdk_keyval_to_lower (keyval);
  real_mask = state & gtk_accelerator_get_default_mod_mask ();

  // Normalize Tab
  if (keyval_lower == GDK_KEY_ISO_Left_Tab)
    keyval_lower = GDK_KEY_Tab;

  // Put shift back if it changed the case of the key, not otherwise.
  if (keyval_lower != keyval)
    real_mask |= GDK_SHIFT_MASK;

  // HACK: we don't want to use SysRq as a keybinding (but we do want
  // Alt+Print), so we avoid translation from Alt+Print to SysRq
  if (keyval_lower == GDK_KEY_Sys_Req && (real_mask & GDK_ALT_MASK) != 0)
    keyval_lower = GDK_KEY_Print;

  // CapsLock isn't supported as a keybinding modifier, so keep it from
  // confusing us
  real_mask &= ~GDK_LOCK_MASK;

  if (keyval_lower != 0)
    {
      g_debug ("keyval: %d, mask: %d", keyval, real_mask);

      /* Check the mask and whether it's a regular or special key */
      special_key = get_special_key (keyval);

      builder = valent_packet_start ("kdeconnect.mousepad.request");

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

      json_builder_set_member_name (builder, "sendAck");
      json_builder_add_boolean_value (builder, TRUE);

      /* A non-printable key */
      if (special_key > 0)
        {
          json_builder_set_member_name (builder, "specialKey");
          json_builder_add_int_value (builder, special_key);
        }
      /* Printable unicode */
      else
        {
          g_autoptr (GError) error = NULL;
          guint32 codepoint;
          g_autofree char *key = NULL;

          codepoint = gdk_keyval_to_unicode (keyval);
          key = g_ucs4_to_utf8 (&codepoint, -1, NULL, NULL, &error);

          if (key == NULL)
            {
              g_warning ("Failed to convert keyval to string: %s", error->message);
              return FALSE;
            }

          json_builder_set_member_name (builder, "key");
          json_builder_add_string_value (builder, key);
        }

      request = valent_packet_finish (builder);
      valent_device_queue_packet (self->device, request);

      // Pass these key combinations rather than using the echo reply
      //return super.vfunc_key_press_event(event);
      if (super)
        return FALSE;
      else
        return TRUE;
  }

  return FALSE;
}

static void
move_cursor (ValentInputDialog *dialog,
             GtkMovementStep    step,
             gint               count)
{
  g_signal_emit_by_name (dialog->editor, "move-cursor", step, count, FALSE);
}

/*
 * Pointer Input
 */
static inline gboolean
calculate_delta (ValentInputDialog *self,
                 double             dx,
                 double             dy,
                 guint32            dt,
                 double            *cx,
                 double            *cy)
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
           ValentInputDialog        *self)
{
  valent_input_dialog_pointer_axis (self, dx, dy);

  return TRUE;
}

static gboolean
longpress_timeout (gpointer data)
{
  ValentInputDialog *self = VALENT_INPUT_DIALOG (data);

  self->claimed = TRUE;
  self->longpress_id = 0;
  gtk_gesture_set_state (self->touch1, GTK_EVENT_SEQUENCE_CLAIMED);

  valent_input_dialog_pointer_press (self);

  return G_SOURCE_REMOVE;
}

static void
on_single_begin (GtkGestureDrag    *gesture,
                 double             start_x,
                 double             start_y,
                 ValentInputDialog *self)
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
on_single_update (GtkGesture        *gesture,
                  GdkEventSequence  *sequence,
                  ValentInputDialog *self)
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

  valent_input_dialog_pointer_motion (self, cx, cy);
}

static void
on_single_end (GtkGestureDrag    *gesture,
               double             offset_x,
               double             offset_y,
               ValentInputDialog *self)
{
  if (!self->claimed)
    {
      unsigned int button = 0;

      button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
      valent_input_dialog_pointer_button (self, button, 1);
    }

  valent_input_dialog_reset (self);
}

static void
on_double_begin (GtkGestureDrag    *gesture,
                 double             start_x,
                 double             start_y,
                 ValentInputDialog *self)
{
  self->last_x = start_x;
  self->last_y = start_y;
}

static void
on_double_update (GtkGesture        *gesture,
                  GdkEventSequence  *sequence,
                  ValentInputDialog *self)
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

  valent_input_dialog_pointer_axis (self, 0.0, round (dy));
}

static void
on_double_end (GtkGestureDrag    *gesture,
               double             offset_x,
               double             offset_y,
               ValentInputDialog *self)
{
  if (!self->claimed)
    valent_input_dialog_pointer_button (self, GDK_BUTTON_SECONDARY, 1);

  valent_input_dialog_reset (self);
}

static void
on_triple_end (GtkGestureDrag    *gesture,
               double             offset_x,
               double             offset_y,
               ValentInputDialog *self)
{
  if (!self->claimed)
    valent_input_dialog_pointer_button (self, GDK_BUTTON_MIDDLE, 1);

  valent_input_dialog_reset (self);
}

static void
valent_input_dialog_pointer_axis (ValentInputDialog *self,
                                  double             dx,
                                  double             dy)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

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
valent_input_dialog_pointer_button (ValentInputDialog *self,
                                    unsigned int       button,
                                    unsigned int       n_press)
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
valent_input_dialog_pointer_motion (ValentInputDialog *self,
                                    double             dx,
                                    double             dy)
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
valent_input_dialog_pointer_press (ValentInputDialog *self)
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
valent_input_dialog_pointer_release (ValentInputDialog *self)
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
valent_input_dialog_reset (ValentInputDialog *self)
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
valent_input_dialog_dispose (GObject *object)
{
  ValentInputDialog *self = VALENT_INPUT_DIALOG (object);

  valent_input_dialog_reset (self);

  G_OBJECT_CLASS (valent_input_dialog_parent_class)->dispose (object);
}

static void
valent_input_dialog_finalize (GObject *object)
{
  ValentInputDialog *self = VALENT_INPUT_DIALOG (object);

  g_clear_object (&self->device);

  G_OBJECT_CLASS (valent_input_dialog_parent_class)->finalize (object);
}

static void
valent_input_dialog_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentInputDialog *self = VALENT_INPUT_DIALOG (object);

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
valent_input_dialog_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ValentInputDialog *self = VALENT_INPUT_DIALOG (object);

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
valent_input_dialog_class_init (ValentInputDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = valent_input_dialog_dispose;
  object_class->finalize = valent_input_dialog_finalize;
  object_class->get_property = valent_input_dialog_get_property;
  object_class->set_property = valent_input_dialog_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/mousepad/valent-input-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentInputDialog, editor);
  gtk_widget_class_bind_template_child (widget_class, ValentInputDialog, touchpad);


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
valent_input_dialog_init (ValentInputDialog *self)
{
  GtkEventController *scroll;

  gtk_widget_init_template (GTK_WIDGET (self));
  self->scale = gtk_widget_get_scale_factor (GTK_WIDGET (self));

  /* Keyboard */
  self->keyboard = g_object_new (GTK_TYPE_EVENT_CONTROLLER_KEY,
                                 "name", "keyboard",
                                 NULL);
  g_signal_connect (self->keyboard,
                    "key-pressed",
                    G_CALLBACK (on_key_pressed),
                    self);
  gtk_widget_add_controller (self->editor, self->keyboard);

  /* Pointer */
  scroll = g_object_new (GTK_TYPE_EVENT_CONTROLLER_SCROLL,
                         "name",  "pointer-scroll",
                         "flags", GTK_EVENT_CONTROLLER_SCROLL_VERTICAL,
                         NULL);
  g_signal_connect (scroll,
                    "scroll",
                    G_CALLBACK (on_scroll),
                    self);
  gtk_widget_add_controller (self->touchpad, GTK_EVENT_CONTROLLER (scroll));

  self->touch1 = g_object_new (GTK_TYPE_GESTURE_DRAG,
                               "name",     "touch-single",
                               "n-points", 1,
                               "button",   0,
                               NULL);
  g_signal_connect (self->touch1,
                    "drag-begin",
                    G_CALLBACK (on_single_begin),
                    self);
  g_signal_connect (self->touch1,
                    "update",
                    G_CALLBACK (on_single_update),
                    self);
  g_signal_connect (self->touch1,
                    "drag-end",
                    G_CALLBACK (on_single_end),
                    self);
  gtk_widget_add_controller (self->touchpad,
                             GTK_EVENT_CONTROLLER (self->touch1));

  self->touch2 = g_object_new (GTK_TYPE_GESTURE_DRAG,
                               "name",       "touch-double",
                               "n-points",   2,
                               "touch-only", TRUE,
                               NULL);
  g_signal_connect (self->touch2,
                    "drag-begin",
                    G_CALLBACK (on_double_begin),
                    self);
  g_signal_connect (self->touch2,
                    "update",
                    G_CALLBACK (on_double_update),
                    self);
  g_signal_connect (self->touch2,
                    "drag-end",
                    G_CALLBACK (on_double_end),
                    self);
  gtk_widget_add_controller (self->touchpad,
                             GTK_EVENT_CONTROLLER (self->touch2));

  self->touch3 = g_object_new (GTK_TYPE_GESTURE_DRAG,
                               "name",       "touch-triple",
                               "n-points",   3,
                               "touch-only", TRUE,
                               NULL);
  g_signal_connect (self->touch3,
                    "drag-end",
                    G_CALLBACK (on_triple_end),
                    self);
  gtk_widget_add_controller (self->touchpad,
                             GTK_EVENT_CONTROLLER (self->touch3));

  gtk_gesture_group (self->touch1, self->touch2);
  gtk_gesture_group (self->touch1, self->touch3);
}

/**
 * valent_input_dialog_new:
 * @device: a #ValentDevice
 *
 * Create a new input dialog for sending input events to @device.
 *
 * Returns: (transfer full): a new #ValentInputDialog
 */
ValentInputDialog *
valent_input_dialog_new (ValentDevice *device)
{
  GApplication *application;
  GtkWindow *window = NULL;

  application = g_application_get_default ();

  if (application != NULL)
    window = gtk_application_get_active_window (GTK_APPLICATION (application));

  return g_object_new (VALENT_TYPE_INPUT_DIALOG,
                       "device",        device,
                       "transient-for", window,
                       NULL);
}

/**
 * valent_input_dialog_echo_key:
 * @dialog: a #ValentInputDialog
 * @packet: a #JsonNode
 *
 * Handle the remote device's acknowledgement of a key we sent.
 */
void
valent_input_dialog_echo_key (ValentInputDialog *dialog,
                              const char        *key,
                              GdkModifierType    mask)
{
  GtkTextBuffer *buffer;
  g_autofree char *old_text = NULL;
  g_autofree char *new_text = NULL;

  g_return_if_fail (VALENT_IS_INPUT_DIALOG (dialog));

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
 * valent_input_dialog_echo_special:
 * @dialog: a #ValentInputDialog
 * @packet: a #JsonNode
 *
 * Handle the remote device's acknowledgement of a special key we sent.
 */
void
valent_input_dialog_echo_special (ValentInputDialog *dialog,
                                  guint              keyval,
                                  GdkModifierType    mask)
{
  GtkTextBuffer *buffer;
  g_autofree char *old_text = NULL;
  g_autofree char *new_text = NULL;

  g_return_if_fail (VALENT_IS_INPUT_DIALOG (dialog));

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

