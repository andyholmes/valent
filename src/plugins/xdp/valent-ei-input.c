// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>
// SPDX-FileContributor: Jonas Ådahl <jadahl@gmail.com>

#define G_LOG_DOMAIN "valent-xdp-input"

#include "config.h"

#include <linux/input-event-codes.h>

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <libei.h>
#include <valent.h>
#include <xkbcommon/xkbcommon.h>

#include "valent-ei-input.h"

struct _ValentEiInput
{
  ValentInputAdapter  parent_instance;

  int                 fd;

  struct ei          *ei;
  GSource            *ei_source;
  struct ei_seat     *ei_seat;
  struct ei_device   *ei_pointer;
  struct ei_device   *ei_pointer_abs;
  struct ei_device   *ei_keyboard;
  struct ei_device   *ei_touch;
  uint32_t            ei_sequence;

  struct xkb_context *xkb_context;
  struct xkb_keymap  *xkb_keymap;
  struct xkb_state   *xkb_state;
};

static void   g_initable_iface_init (GInitableIface *iface);
static void   valent_ei_input_stop  (ValentEiInput  *self);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentEiInput, valent_ei_input, VALENT_TYPE_INPUT_ADAPTER,
                               G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, g_initable_iface_init))

typedef enum {
  PROP_FD = 1,
} ValentEiInputProperty;

static GParamSpec *properties[PROP_FD + 1] = { NULL, };

typedef enum
{
  EVDEV_BUTTON_TYPE_NONE,
  EVDEV_BUTTON_TYPE_KEY,
  EVDEV_BUTTON_TYPE_BUTTON,
} EvdevButtonType;

static EvdevButtonType
_evdev_code_get_button_type (uint16_t code)
{
  switch (code)
    {
    case BTN_TOOL_PEN:
    case BTN_TOOL_RUBBER:
    case BTN_TOOL_BRUSH:
    case BTN_TOOL_PENCIL:
    case BTN_TOOL_AIRBRUSH:
    case BTN_TOOL_MOUSE:
    case BTN_TOOL_LENS:
    case BTN_TOOL_QUINTTAP:
    case BTN_TOOL_DOUBLETAP:
    case BTN_TOOL_TRIPLETAP:
    case BTN_TOOL_QUADTAP:
    case BTN_TOOL_FINGER:
    case BTN_TOUCH:
      return EVDEV_BUTTON_TYPE_NONE;
    }

  if (code >= KEY_ESC && code <= KEY_MICMUTE)
    return EVDEV_BUTTON_TYPE_KEY;
  if (code >= BTN_MISC && code <= BTN_GEAR_UP)
    return EVDEV_BUTTON_TYPE_BUTTON;
  if (code >= KEY_OK && code <= KEY_LIGHTS_TOGGLE)
    return EVDEV_BUTTON_TYPE_KEY;
  if (code >= BTN_DPAD_UP && code <= BTN_DPAD_RIGHT)
    return EVDEV_BUTTON_TYPE_BUTTON;
  if (code >= KEY_ALS_TOGGLE && code <= KEY_KBDINPUTASSIST_CANCEL)
    return EVDEV_BUTTON_TYPE_KEY;
  if (code >= BTN_TRIGGER_HAPPY && code <= BTN_TRIGGER_HAPPY40)
    return EVDEV_BUTTON_TYPE_BUTTON;

  return EVDEV_BUTTON_TYPE_NONE;
}

/*
 * XKB Helpers
 */
static gboolean
_xkb_keycode_from_keysym (struct xkb_keymap *xkb_keymap,
                          struct xkb_state  *xkb_state,
                          uint32_t           keysym,
                          uint32_t          *keycode_out,
                          uint32_t          *level_out)
{
  uint32_t layout;
  xkb_keycode_t min_keycode, max_keycode;

  g_assert (xkb_keymap != NULL);
  g_assert (xkb_state != NULL);
  g_assert (keycode_out != NULL);

  layout = xkb_state_serialize_layout (xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
  min_keycode = xkb_keymap_min_keycode (xkb_keymap);
  max_keycode = xkb_keymap_max_keycode (xkb_keymap);
  for (xkb_keycode_t keycode = min_keycode; keycode < max_keycode; keycode++)
    {
      xkb_level_index_t num_levels;

      num_levels = xkb_keymap_num_levels_for_key (xkb_keymap, keycode, layout);
      for (xkb_level_index_t level = 0; level < num_levels; level++)
        {
          const xkb_keysym_t *syms;
          int num_syms;

          num_syms = xkb_keymap_key_get_syms_by_level (xkb_keymap,
                                                       keycode,
                                                       layout,
                                                       level,
                                                       &syms);
          for (int sym = 0; sym < num_syms; sym++)
            {
              if (syms[sym] == keysym)
                {
                  *keycode_out = keycode;
                  if (level_out != NULL)
                    *level_out = level;

                  return TRUE;
                }
            }
        }
    }

  return FALSE;
}

static uint32_t
_xkb_keycode_to_evdev_code (xkb_keycode_t keycode)
{
  return keycode - 8;
}

/*
 * libei
 */
static gboolean
process_keymap (ValentEiInput     *self,
                struct ei_device  *device,
                GError           **error)
{
  g_autoptr (GInputStream) stream = NULL;
  struct ei_keymap *keymap;
  enum ei_keymap_type keymap_type;
  g_autofree char *keymap_str = NULL;
  size_t keymap_size;

  g_clear_pointer (&self->xkb_context, xkb_context_unref);
  g_clear_pointer (&self->xkb_keymap, xkb_keymap_unref);
  g_clear_pointer (&self->xkb_state, xkb_state_unref);

  keymap = ei_device_keyboard_get_keymap (device);
  if (keymap == NULL)
    {
      VALENT_NOTE ("No keymap");
      return TRUE;
    }

  keymap_type = ei_keymap_get_type (keymap);
  switch ((enum ei_keymap_type)keymap_type)
    {
    case EI_KEYMAP_TYPE_XKB:
      break;

    default:
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Unknown keyboard layout type");
      return FALSE;
    }

  keymap_size = ei_keymap_get_size (keymap);
  keymap_str = g_malloc0 (keymap_size + 1);
  stream = g_unix_input_stream_new (ei_keymap_get_fd (keymap), FALSE);
  if (!g_input_stream_read_all (stream, keymap_str, keymap_size, NULL, NULL, NULL))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to load EI keymap");
      return FALSE;
    }

  self->xkb_context = xkb_context_new (XKB_CONTEXT_NO_FLAGS);
  if (self->xkb_context == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to create XKB context");
      return FALSE;
    }

  self->xkb_keymap = xkb_keymap_new_from_string (self->xkb_context,
                                                 keymap_str,
                                                 XKB_KEYMAP_FORMAT_TEXT_V1,
                                                 XKB_KEYMAP_COMPILE_NO_FLAGS);
  if (self->xkb_keymap == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to create XKB keymap");
      g_clear_pointer (&self->xkb_context, xkb_context_unref);
      return FALSE;
    }

  self->xkb_state = xkb_state_new (self->xkb_keymap);
  if (self->xkb_state == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to create XKB state");
      g_clear_pointer (&self->xkb_context, xkb_context_unref);
      g_clear_pointer (&self->xkb_keymap, xkb_keymap_unref);
      return FALSE;
    }

  return TRUE;
}

/*
 * GSource
 */
typedef struct _EiEventSource
{
  GSource        base;
  ValentEiInput *self;
} EiEventSource;

static gboolean
ei_source_prepare (GSource *source,
                   int     *timeout)
{
  EiEventSource *ei_source = (EiEventSource *) source;
  ValentEiInput *self = ei_source->self;

  if (timeout != NULL)
    *timeout = -1;

  return !!ei_peek_event (self->ei);
}

static gboolean
ei_source_dispatch (GSource     *source,
                    GSourceFunc  callback,
                    gpointer     user_data)
{
  EiEventSource *ei_source = (EiEventSource *) source;
  ValentEiInput *self = ei_source->self;
  struct ei_event *event;

  ei_dispatch (self->ei);

  while ((event = ei_get_event (self->ei)) != NULL)
    {
      enum ei_event_type event_type = ei_event_get_type (event);

      VALENT_NOTE ("Received event type %s",
                   ei_event_type_to_string (event_type));

      switch ((enum ei_event_type)event_type)
        {
        case EI_EVENT_CONNECT:
          {
            valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                                   VALENT_PLUGIN_STATE_ACTIVE,
                                                   NULL);
            break;
          }
        case EI_EVENT_DISCONNECT:
          {
            valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                                   VALENT_PLUGIN_STATE_INACTIVE,
                                                   NULL);
            break;
          }
        case EI_EVENT_SEAT_ADDED:
          {
            if (self->ei_seat != NULL)
              break;

            self->ei_seat = ei_seat_ref (ei_event_get_seat (event));
            ei_seat_bind_capabilities (self->ei_seat,
                                       EI_DEVICE_CAP_POINTER,
                                       EI_DEVICE_CAP_KEYBOARD,
                                       EI_DEVICE_CAP_POINTER_ABSOLUTE,
                                       EI_DEVICE_CAP_BUTTON,
                                       EI_DEVICE_CAP_SCROLL,
                                       EI_DEVICE_CAP_TOUCH,
                                       NULL);
            break;
          }

        case EI_EVENT_SEAT_REMOVED:
          {
            if (ei_event_get_seat (event) == self->ei_seat)
              g_clear_pointer (&self->ei_seat, ei_seat_unref);

            break;
          }

        case EI_EVENT_DEVICE_ADDED:
          {
            struct ei_device *device = ei_event_get_device (event);

            VALENT_NOTE ("Device '%s' added", ei_device_get_name (device));

            if (ei_device_has_capability (device, EI_DEVICE_CAP_KEYBOARD))
              {
                g_autoptr (GError) error = NULL;

                g_clear_pointer (&self->ei_keyboard, ei_device_unref);
                self->ei_keyboard = ei_device_ref (device);
                if (!process_keymap (self, self->ei_keyboard, &error))
                  {
                    valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                                           VALENT_PLUGIN_STATE_ERROR,
                                                           error);
                    ei_event_unref (event);
                    valent_ei_input_stop (self);
                    return G_SOURCE_REMOVE;
                  }
              }

            if (ei_device_has_capability (device, EI_DEVICE_CAP_POINTER))
              {
                g_clear_pointer (&self->ei_pointer, ei_device_unref);
                self->ei_pointer = ei_device_ref (device);
              }

            if (ei_device_has_capability (device, EI_DEVICE_CAP_POINTER_ABSOLUTE))
              {
                g_clear_pointer (&self->ei_pointer_abs, ei_device_unref);
                self->ei_pointer_abs = ei_device_ref (device);
              }

            if (ei_device_has_capability (device, EI_DEVICE_CAP_TOUCH))
              {
                g_clear_pointer (&self->ei_touch, ei_device_unref);
                self->ei_touch = ei_device_ref (device);
              }

            break;
          }

        case EI_EVENT_DEVICE_REMOVED:
          {
            struct ei_device *device = ei_event_get_device (event);

            VALENT_NOTE ("Device '%s' removed", ei_device_get_name (device));

            if (device == self->ei_keyboard)
              g_clear_pointer (&self->ei_keyboard, ei_device_unref);
            if (device == self->ei_pointer)
              g_clear_pointer (&self->ei_pointer, ei_device_unref);
            if (device == self->ei_pointer_abs)
              g_clear_pointer (&self->ei_pointer_abs, ei_device_unref);
            if (device == self->ei_touch)
              g_clear_pointer (&self->ei_touch, ei_device_unref);

            break;
          }

        case EI_EVENT_DEVICE_RESUMED:
          {
            struct ei_device *device = ei_event_get_device (event);

            VALENT_NOTE ("Device '%s' resumed", ei_device_get_name (device));

            if (device == self->ei_pointer)
              ei_device_start_emulating (self->ei_pointer, ++self->ei_sequence);
            if (device == self->ei_pointer_abs)
              ei_device_start_emulating (self->ei_pointer_abs, ++self->ei_sequence);
            if (device == self->ei_keyboard)
              ei_device_start_emulating (self->ei_keyboard, ++self->ei_sequence);
            if (device == self->ei_touch)
              ei_device_start_emulating (self->ei_touch, ++self->ei_sequence);

            break;
          }

        case EI_EVENT_DEVICE_PAUSED:
          {
            struct ei_device *device = ei_event_get_device (event);

            VALENT_NOTE ("Device '%s' paused", ei_device_get_name (device));

            break;
          }

        case EI_EVENT_KEYBOARD_MODIFIERS:
        case EI_EVENT_PONG:
        case EI_EVENT_SYNC:
        case EI_EVENT_FRAME:
        case EI_EVENT_DEVICE_START_EMULATING:
        case EI_EVENT_DEVICE_STOP_EMULATING:
        case EI_EVENT_POINTER_MOTION:
        case EI_EVENT_POINTER_MOTION_ABSOLUTE:
        case EI_EVENT_BUTTON_BUTTON:
        case EI_EVENT_SCROLL_DELTA:
        case EI_EVENT_SCROLL_STOP:
        case EI_EVENT_SCROLL_CANCEL:
        case EI_EVENT_SCROLL_DISCRETE:
        case EI_EVENT_KEYBOARD_KEY:
        case EI_EVENT_TOUCH_DOWN:
        case EI_EVENT_TOUCH_UP:
        case EI_EVENT_TOUCH_MOTION:
        default:
          break;
        }

      ei_event_unref (event);
    }

  return G_SOURCE_CONTINUE;
}

static GSourceFuncs ei_source_funcs =
{
  .prepare = ei_source_prepare,
  .dispatch = ei_source_dispatch,
};

static GSource *
valent_ei_input_create_source (ValentEiInput *self)
{
  GSource *source;

  source = g_source_new (&ei_source_funcs, sizeof (EiEventSource));
  g_source_set_name (source, "valent-ei");
  g_source_add_unix_fd (source, ei_get_fd (self->ei), G_IO_IN | G_IO_ERR);
  ((EiEventSource *) source)->self = self;

  return source;
}

/*
 * ValentInputAdapter
 */
static void
valent_ei_input_keyboard_modifiers (ValentEiInput *self,
                                    uint64_t       time_us,
                                    uint32_t       level,
                                    uint32_t       key_state)
{
  uint32_t keysym, keycode, evcode;

  switch (level)
    {
    case 0:
      return;

    case 1:
      keysym = XKB_KEY_Shift_L;
      break;

    case 2:
      keysym = XKB_KEY_ISO_Level3_Shift;
      break;

    default:
      g_warning ("Unknown modifier level `%u`", level);
      return;
    }

  if (!_xkb_keycode_from_keysym (self->xkb_keymap,
                                 self->xkb_state,
                                 keysym,
                                 &keycode,
                                 NULL))
    {
      return;
    }

  evcode = _xkb_keycode_to_evdev_code (keycode);
  ei_device_keyboard_key (self->ei_keyboard, evcode, key_state);
  ei_device_frame (self->ei_keyboard, time_us);
}

static void
valent_ei_input_keyboard_keysym (ValentInputAdapter *adapter,
                                 uint32_t            keysym,
                                 gboolean            state)
{
  ValentEiInput *self = VALENT_EI_INPUT (adapter);
  int64_t timestamp;
  uint32_t keycode = 0;
  uint32_t level = 0;
  uint32_t evcode = 0;

  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_EI_INPUT (self));

  if (self->ei_keyboard == NULL || self->xkb_keymap == NULL)
    return;

  timestamp = ei_now (self->ei);
  if (!_xkb_keycode_from_keysym (self->xkb_keymap,
                                 self->xkb_state,
                                 keysym,
                                 &keycode,
                                 &level))
    {
      g_warning ("%s(): No keycode for keysym 0x%x in current group",
                 G_STRFUNC, keysym);
      return;
    }

  evcode = _xkb_keycode_to_evdev_code (keycode);
  if (_evdev_code_get_button_type (evcode) != EVDEV_BUTTON_TYPE_KEY)
    {
      g_warning ("%s(): Invalid keycode 0x%x (keysym 0x%x) for keyboard",
                 G_STRFUNC, evcode, keysym);
      return;
    }

  if (state)
    valent_ei_input_keyboard_modifiers (self, timestamp, level, state);

  ei_device_keyboard_key (self->ei_keyboard, evcode, state);
  ei_device_frame (self->ei_keyboard, timestamp);

  if (!state)
    valent_ei_input_keyboard_modifiers (self, timestamp, level, state);
}

static unsigned int
translate_to_evdev_button (unsigned int button)
{
  switch (button)
    {
    case VALENT_POINTER_PRIMARY:
      return BTN_LEFT;

    case VALENT_POINTER_MIDDLE:
      return BTN_MIDDLE;

    case VALENT_POINTER_SECONDARY:
      return BTN_RIGHT;

    default:
      /* Any other buttons go after the legacy scroll buttons (4-7). */
      return button + (BTN_LEFT - 1) - 4;
    }
}

static void
valent_ei_input_pointer_axis (ValentInputAdapter *adapter,
                              double              dx,
                              double              dy)
{
  ValentEiInput *self = VALENT_EI_INPUT (adapter);

  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_EI_INPUT (self));
  g_assert (!G_APPROX_VALUE (dx, 0.0, 0.01) || !G_APPROX_VALUE (dy, 0.0, 0.01));

  if (self->ei_pointer == NULL)
    return;

  ei_device_scroll_delta (self->ei_pointer, dx, dy);
  ei_device_frame (self->ei_pointer, ei_now (self->ei));
  ei_device_scroll_stop (self->ei_pointer, TRUE, TRUE);
  ei_device_frame (self->ei_pointer, ei_now (self->ei));
}

static void
valent_ei_input_pointer_button (ValentInputAdapter *adapter,
                                unsigned int        button,
                                gboolean            pressed)
{
  ValentEiInput *self = VALENT_EI_INPUT (adapter);

  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_EI_INPUT (self));

  if (self->ei_pointer == NULL)
    return;

  button = translate_to_evdev_button (button);
  ei_device_button_button (self->ei_pointer, button, pressed);
  ei_device_frame (self->ei_pointer, ei_now (self->ei));
}

static void
valent_ei_input_pointer_motion (ValentInputAdapter *adapter,
                                double              dx,
                                double              dy)
{
  ValentEiInput *self = VALENT_EI_INPUT (adapter);

  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_EI_INPUT (self));

  if (self->ei_pointer == NULL)
    return;

  ei_device_pointer_motion (self->ei_pointer, dx, dy);
  ei_device_frame (self->ei_pointer, ei_now (self->ei));
}

static void
valent_ei_input_stop (ValentEiInput *self)
{
  g_assert (VALENT_IS_EI_INPUT (self));

  if (self->ei_source != NULL)
    {
      g_source_destroy (self->ei_source);
      g_clear_pointer (&self->ei_source, g_source_unref);
    }

  g_clear_pointer (&self->xkb_state, xkb_state_unref);
  g_clear_pointer (&self->xkb_keymap, xkb_keymap_unref);
  g_clear_pointer (&self->xkb_context, xkb_context_unref);

  g_clear_pointer (&self->ei_touch, ei_device_unref);
  g_clear_pointer (&self->ei_keyboard, ei_device_unref);
  g_clear_pointer (&self->ei_pointer, ei_device_unref);
  g_clear_pointer (&self->ei_pointer_abs, ei_device_unref);
  g_clear_pointer (&self->ei_seat, ei_seat_unref);
  g_clear_pointer (&self->ei, ei_unref);
}

/*
 * GInitable
 */
static gboolean
valent_ei_input_initable_init (GInitable     *initable,
                               GCancellable  *cancellable,
                               GError       **error)
{
  ValentEiInput *self = VALENT_EI_INPUT (initable);

  self->ei = ei_new_sender (self);
  ei_configure_name (self->ei, "valent");
  if (ei_setup_backend_fd (self->ei, self->fd) == -1)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to setup libei backend");
      g_clear_pointer (&self->ei, ei_unref);
      return FALSE;
    }

  self->ei_source = valent_ei_input_create_source (self);
  g_source_attach (self->ei_source, NULL);

  return TRUE;
}

static void
g_initable_iface_init (GInitableIface *iface)
{
  iface->init = valent_ei_input_initable_init;
}

/*
 * ValentObject
 */
static void
valent_ei_input_destroy (ValentObject *object)
{
  ValentEiInput *self = VALENT_EI_INPUT (object);

  valent_ei_input_stop (self);

  VALENT_OBJECT_CLASS (valent_ei_input_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_ei_input_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  ValentEiInput *self = VALENT_EI_INPUT (object);

  switch ((ValentEiInputProperty)prop_id)
    {
    case PROP_FD:
      g_value_set_int (value, self->fd);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_ei_input_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  ValentEiInput *self = VALENT_EI_INPUT (object);

  switch ((ValentEiInputProperty)prop_id)
    {
    case PROP_FD:
      self->fd = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_ei_input_class_init (ValentEiInputClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentInputAdapterClass *adapter_class = VALENT_INPUT_ADAPTER_CLASS (klass);

  object_class->get_property = valent_ei_input_get_property;
  object_class->set_property = valent_ei_input_set_property;

  vobject_class->destroy = valent_ei_input_destroy;

  adapter_class->keyboard_keysym = valent_ei_input_keyboard_keysym;
  adapter_class->pointer_axis = valent_ei_input_pointer_axis;
  adapter_class->pointer_button = valent_ei_input_pointer_button;
  adapter_class->pointer_motion = valent_ei_input_pointer_motion;

  /**
   * ValentEiInput:fd:
   *
   * The file descriptor for libei.
   */
  properties [PROP_FD] =
    g_param_spec_int ("fd", NULL, NULL,
                      G_MININT, G_MAXINT,
                      -1,
                      (G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT_ONLY |
                       G_PARAM_EXPLICIT_NOTIFY |
                       G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_ei_input_init (ValentEiInput  *self)
{
}

