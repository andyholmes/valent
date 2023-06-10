// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-input-remote"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>
#include <libvalent-input.h>

#include "valent-input-remote.h"
#include "valent-ui-utils.h"
#include "valent-ui-utils-private.h"

#define CAPTURE_THRESHOLD_MS 50


struct _ValentInputRemote
{
  AdwWindow           parent_instance;

  GListModel         *adapters;
  ValentInputAdapter *adapter;

  /* Pointer State */
  unsigned int        claimed : 1;
  uint32_t            timestamp;

  double              last_x;
  double              last_y;
  double              last_v;
  int                 scale;

  /* template */
  GtkDropDown        *input_adapter;
  GtkWidget          *editor;
  GtkEventController *keyboard;
  GtkWidget          *touchpad;
  GtkGesture         *pointer_scroll;
  GtkGesture         *touch_single;
  GtkGesture         *touch_double;
  GtkGesture         *touch_triple;
};

G_DEFINE_FINAL_TYPE (ValentInputRemote, valent_input_remote, ADW_TYPE_WINDOW)

enum {
  PROP_0,
  PROP_ADAPTERS,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static inline gboolean
valent_input_remote_check_adapter (ValentInputRemote *self)
{
  g_assert (VALENT_IS_INPUT_REMOTE (self));

  if G_UNLIKELY (self->adapter == NULL)
    {
      self->claimed = FALSE;
      self->timestamp = 0;
      self->last_x = 0.0;
      self->last_y = 0.0;
      self->last_v = 0.0;

      return FALSE;
    }

  return TRUE;
}

/*
 * Keyboard Input
 */
static gboolean
on_key_pressed (GtkEventControllerKey *controller,
                unsigned int           keyval,
                unsigned int           keycode,
                GdkModifierType        state,
                ValentInputRemote     *self)
{
  g_assert (VALENT_IS_INPUT_REMOTE (self));

  if (valent_input_remote_check_adapter (self))
    valent_input_adapter_keyboard_keysym (self->adapter, keyval, TRUE);

  return TRUE;
}

static gboolean
on_key_released (GtkEventControllerKey *controller,
                 unsigned int           keyval,
                 unsigned int           keycode,
                 GdkModifierType        state,
                 ValentInputRemote     *self)
{
  g_assert (VALENT_IS_INPUT_REMOTE (self));

  if (valent_input_remote_check_adapter (self))
    valent_input_adapter_keyboard_keysym (self->adapter, keyval, FALSE);

  return TRUE;
}

/*
 * Pointer Input
 */
static inline void
get_last_update_time (GtkGesture       *gesture,
                      GdkEventSequence *sequence,
                      uint32_t          *timestamp)
{
  GdkEvent *event = NULL;

  if (sequence != NULL)
    event = gtk_gesture_get_last_event (gesture, sequence);

  if (event != NULL)
    *timestamp = gdk_event_get_time (event);
}

static inline gboolean
calculate_delta (ValentInputRemote *self,
                 double             dx,
                 double             dy,
                 uint32_t           dt,
                 double            *cx,
                 double            *cy)
{
  double dr, v, m;

  dr = sqrt (pow (dx, 2) + pow (dy, 2));
  v = dr / dt;

  if (!G_APPROX_VALUE (self->last_v, 0.0, 0.01))
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

static inline void
valent_input_remote_pointer_reset (ValentInputRemote *self)
{
  self->claimed = FALSE;
  self->last_v = 0.0;
  self->last_x = 0.0;
  self->last_y = 0.0;
  self->timestamp = 0;
}

/*
 * Scroll Mapping
 */
static gboolean
on_scroll (GtkEventControllerScroll *controller,
           double                    dx,
           double                    dy,
           ValentInputRemote        *self)
{
  g_assert (VALENT_IS_INPUT_REMOTE (self));

  if (valent_input_remote_check_adapter (self))
    valent_input_adapter_pointer_axis (self->adapter, dx, dy);

  return TRUE;
}

/*
 * Pointer Button Mapping
 *
 * This gesture maps pointer button presses and releases directly, except in the
 * case of a press-move sequence of the primary button, which is used to emulate
 * touchpad motion.
 */
static void
on_single_begin (GtkGestureDrag    *gesture,
                 double             start_x,
                 double             start_y,
                 ValentInputRemote *self)
{
  GtkGestureSingle *single = GTK_GESTURE_SINGLE (gesture);
  unsigned int button = 0;

  g_assert (VALENT_IS_INPUT_REMOTE (self));

  if (!valent_input_remote_check_adapter (self))
    return;

  /* Relative pointer motion is only emulated for the primary button, otherwise
   * presses and releases are mapped directly to the adapter. */
  button = gtk_gesture_single_get_current_button (single);

  if (button == GDK_BUTTON_PRIMARY)
    {
      GdkEventSequence *sequence = NULL;
      uint32_t timestamp = 0;

      sequence = gtk_gesture_single_get_current_sequence (single);
      get_last_update_time (GTK_GESTURE (gesture), sequence, &timestamp);

      self->last_x = start_x;
      self->last_y = start_y;
      self->timestamp = timestamp;
    }

  /* Always pass through the button press, since pointer motion is only
   * emulated behaviour. */
  valent_input_adapter_pointer_button (self->adapter, button, TRUE);
}

static void
on_single_update (GtkGesture        *gesture,
                  GdkEventSequence  *sequence,
                  ValentInputRemote *self)
{
  unsigned int button = 0;
  uint32_t timestamp = 0;
  double x, y;
  double dx, dy, dt;
  double cx, cy;

  g_assert (VALENT_IS_INPUT_REMOTE (self));

  if (!valent_input_remote_check_adapter (self))
    return;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button != GDK_BUTTON_PRIMARY)
    return;

  get_last_update_time (gesture, sequence, &timestamp);
  gtk_gesture_get_point (gesture, sequence, &x, &y);

  dt = timestamp - self->timestamp;
  dx = (x - self->last_x) * self->scale;
  dy = (y - self->last_y) * self->scale;

  if (!calculate_delta (self, dx, dy, dt, &cx, &cy))
    return;

  if (dx >= 1.0 || dx <= -1.0 || dy >= 1.0 || dy <= -1.0)
    {
      self->claimed = TRUE;
      gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);

      self->last_x = x;
      self->last_y = y;
      self->timestamp = timestamp;

      valent_input_adapter_pointer_motion (self->adapter, cx, cy);
    }
}

static void
on_single_end (GtkGestureDrag    *gesture,
               double             offset_x,
               double             offset_y,
               ValentInputRemote *self)
{
  unsigned int button = 0;

  g_assert (VALENT_IS_INPUT_REMOTE (self));

  if (!valent_input_remote_check_adapter (self))
    return;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  valent_input_adapter_pointer_button (self->adapter, button, FALSE);
  valent_input_remote_pointer_reset (self);
}

/*
 * Touchpad Emulation
 *
 * These callbacks map gestures on the "touchpad" area to events including:
 *
 * - two-finger tap   -> right click
 * - three-finger tap -> middle click
 */
static void
on_double_begin (GtkGestureDrag    *gesture,
                 double             start_x,
                 double             start_y,
                 ValentInputRemote *self)
{
  g_assert (VALENT_IS_INPUT_REMOTE (self));

  // TODO: In order to map two-finger presses directly to the input adapter,
  //       the implementation has to handle unpaired press-release sequences.
#if 0
  if (!valent_input_remote_check_adapter (self))
    return;

  valent_input_adapter_pointer_button (self->adapter,
                                       GDK_BUTTON_SECONDARY,
                                       TRUE);
#endif
}

static void
on_double_end (GtkGestureDrag    *gesture,
               double             offset_x,
               double             offset_y,
               ValentInputRemote *self)
{
  g_assert (VALENT_IS_INPUT_REMOTE (self));

  if (!valent_input_remote_check_adapter (self))
    return;

  /* If the two-finger press wasn't claimed as a scroll event on the y-axis,
   * simulate a right click by pressing and releasing the secondary button. */
  valent_input_adapter_pointer_button (self->adapter,
                                       GDK_BUTTON_SECONDARY,
                                       TRUE);
  valent_input_adapter_pointer_button (self->adapter,
                                       GDK_BUTTON_SECONDARY,
                                       FALSE);
}

static void
on_triple_begin (GtkGestureDrag    *gesture,
                 double             offset_x,
                 double             offset_y,
                 ValentInputRemote *self)
{
  g_assert (VALENT_IS_INPUT_REMOTE (self));

  if (!valent_input_remote_check_adapter (self))
    return;

  /* Since there is no high-level event for three-finger drags, three-finger
   * presses and releases can be mapped directly. */
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
  valent_input_adapter_pointer_button (self->adapter,
                                       GDK_BUTTON_MIDDLE,
                                       TRUE);
}

static void
on_triple_end (GtkGestureDrag    *gesture,
               double             offset_x,
               double             offset_y,
               ValentInputRemote *self)
{
  g_assert (VALENT_IS_INPUT_REMOTE (self));

  if (!valent_input_remote_check_adapter (self))
    return;

  valent_input_adapter_pointer_button (self->adapter,
                                       GDK_BUTTON_MIDDLE,
                                       FALSE);
}

static void
on_selected_item (GObject           *object,
                  GParamSpec        *pspec,
                  ValentInputRemote *self)
{
  ValentInputAdapter *adapter = NULL;

  g_assert (VALENT_IS_INPUT_REMOTE (self));

  adapter = gtk_drop_down_get_selected_item (GTK_DROP_DOWN (object));

  if (g_set_object (&self->adapter, adapter))
    valent_input_remote_check_adapter (self);
}

static char *
dup_adapter_name (ValentInputAdapter *adapter)
{
  GObject *object = NULL;
  GParamSpec *pspec = NULL;
  g_autofree char *name = NULL;

  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));

  object = valent_extension_get_object (VALENT_EXTENSION (adapter));

  if (object != NULL)
    pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (object), "name");

  if (pspec != NULL)
    g_object_get (object, "name", &name, NULL);

  if (name == NULL)
    return g_strdup (G_OBJECT_TYPE_NAME (adapter));

  return g_steal_pointer (&name);
}

/*
 * GObject
 */
static void
valent_input_remote_constructed (GObject *object)
{
  ValentInputRemote *self = VALENT_INPUT_REMOTE (object);
  g_autoptr (GtkExpression) expression = NULL;

  expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL,
                                            0, NULL,
                                            G_CALLBACK (dup_adapter_name),
                                            NULL, NULL);

  gtk_drop_down_set_expression (self->input_adapter, expression);
  gtk_drop_down_set_model (self->input_adapter, self->adapters);

  G_OBJECT_CLASS (valent_input_remote_parent_class)->constructed (object);
}

static void
valent_input_remote_dispose (GObject *object)
{
  ValentInputRemote *self = VALENT_INPUT_REMOTE (object);

  g_clear_object (&self->adapter);
  g_clear_object (&self->adapters);

  gtk_widget_dispose_template (GTK_WIDGET (object), VALENT_TYPE_INPUT_REMOTE);

  G_OBJECT_CLASS (valent_input_remote_parent_class)->dispose (object);
}

static void
valent_input_remote_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentInputRemote *self = VALENT_INPUT_REMOTE (object);

  switch (prop_id)
    {
    case PROP_ADAPTERS:
      g_value_set_object (value, self->adapters);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_input_remote_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ValentInputRemote *self = VALENT_INPUT_REMOTE (object);

  switch (prop_id)
    {
    case PROP_ADAPTERS:
      self->adapters = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_input_remote_class_init (ValentInputRemoteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_input_remote_constructed;
  object_class->dispose = valent_input_remote_dispose;
  object_class->get_property = valent_input_remote_get_property;
  object_class->set_property = valent_input_remote_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Valent/ui/valent-input-remote.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentInputRemote, input_adapter);
  gtk_widget_class_bind_template_child (widget_class, ValentInputRemote, editor);
  gtk_widget_class_bind_template_child (widget_class, ValentInputRemote, keyboard);
  gtk_widget_class_bind_template_child (widget_class, ValentInputRemote, pointer_scroll);
  gtk_widget_class_bind_template_child (widget_class, ValentInputRemote, touchpad);
  gtk_widget_class_bind_template_child (widget_class, ValentInputRemote, touch_single);
  gtk_widget_class_bind_template_child (widget_class, ValentInputRemote, touch_double);
  gtk_widget_class_bind_template_child (widget_class, ValentInputRemote, touch_triple);

  gtk_widget_class_bind_template_callback (widget_class, on_selected_item);
  gtk_widget_class_bind_template_callback (widget_class, on_key_pressed);
  gtk_widget_class_bind_template_callback (widget_class, on_key_released);
  gtk_widget_class_bind_template_callback (widget_class, on_scroll);
  gtk_widget_class_bind_template_callback (widget_class, on_single_begin);
  gtk_widget_class_bind_template_callback (widget_class, on_single_update);
  gtk_widget_class_bind_template_callback (widget_class, on_single_end);
  gtk_widget_class_bind_template_callback (widget_class, on_double_begin);
  gtk_widget_class_bind_template_callback (widget_class, on_double_end);
  gtk_widget_class_bind_template_callback (widget_class, on_triple_begin);
  gtk_widget_class_bind_template_callback (widget_class, on_triple_end);

  properties [PROP_ADAPTERS] =
    g_param_spec_object ("adapters", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_input_remote_init (ValentInputRemote *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_gesture_group (self->touch_single, self->touch_double);
  gtk_gesture_group (self->touch_single, self->touch_triple);

  self->scale = gtk_widget_get_scale_factor (GTK_WIDGET (self));
}

