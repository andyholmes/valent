// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-input-dialog"

#include "config.h"

#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-ui.h>
#include <libvalent-input.h>

#include "valent-input-dialog.h"
#include "valent-mousepad-plugin.h"


struct _ValentInputDialog
{
  GtkDialog             parent_instance;

  ValentMousepadPlugin *plugin;
  ValentDevice         *device;
  GtkEventController   *controller;

  /* Template widgets */
  GtkStack             *stack;
  GtkWidget            *touchpad;
  GtkWidget            *editor;

  GtkWidget            *alt_label;
  GtkWidget            *ctrl_label;
  GtkWidget            *shift_label;
  GtkWidget            *super_label;
};

G_DEFINE_TYPE (ValentInputDialog, valent_input_dialog, GTK_TYPE_DIALOG)


enum {
  PROP_0,
  PROP_PLUGIN,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  TEXT_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


static inline gboolean
is_mod_key (gint key)
{
  switch (key)
    {
    case GDK_KEY_Alt_L:
    case GDK_KEY_Alt_R:
    case GDK_KEY_Caps_Lock:
    case GDK_KEY_Control_L:
    case GDK_KEY_Control_R:
    case GDK_KEY_Meta_L:
    case GDK_KEY_Meta_R:
    case GDK_KEY_Num_Lock:
    case GDK_KEY_Shift_L:
    case GDK_KEY_Shift_R:
    case GDK_KEY_Super_L:
    case GDK_KEY_Super_R:
      return TRUE;

    default:
      return FALSE;
    }
}

// FIXME map
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
  GdkModifierType real_mask = 0;
  guint keyval_lower = 0, special_key;
  gboolean alt, ctrl, shift, super;
  JsonBuilder *builder;
  g_autoptr (JsonNode) request = NULL;

  g_assert (VALENT_IS_INPUT_DIALOG (self));


  keyval_lower = gdk_keyval_to_lower (keyval);
  real_mask = state & gtk_accelerator_get_default_mod_mask ();

  alt = !!(real_mask & GDK_ALT_MASK);
  ctrl = !!(real_mask & GDK_CONTROL_MASK);
  shift = !!(real_mask & GDK_SHIFT_MASK);
  super = !!(real_mask & GDK_SUPER_MASK);

  /* Totally unnecessary modifier indicators */
  gtk_widget_set_sensitive (self->alt_label, is_alt (keyval_lower) || alt);
  gtk_widget_set_sensitive (self->ctrl_label, is_ctrl (keyval_lower) || ctrl);
  gtk_widget_set_sensitive (self->shift_label, is_shift (keyval_lower) || shift);
  gtk_widget_set_sensitive (self->super_label, is_super (keyval_lower) || super);

  // Wait for a real key before sending
  if (is_mod_key (keyval_lower))
    return FALSE;

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

      if (alt)
        {
          json_builder_set_member_name (builder, "alt");
          json_builder_add_boolean_value (builder, alt);
        }

      if (ctrl)
        {
          json_builder_set_member_name (builder, "ctrl");
          json_builder_add_boolean_value (builder, ctrl);
        }

      if (shift)
        {
          json_builder_set_member_name (builder, "shift");
          json_builder_add_boolean_value (builder, shift);
        }

      if (super)
        {
          json_builder_set_member_name (builder, "super");
          json_builder_add_boolean_value (builder, super);
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

static gboolean
on_key_released (GtkEventControllerKey *controller,
                 guint                  keyval,
                 guint                  keycode,
                 GdkModifierType        state,
                 ValentInputDialog     *self)
{
  GdkModifierType real_mask = 0;
  guint keyval_lower = 0;
  gboolean alt, ctrl, shift, super;

  g_assert (VALENT_IS_INPUT_DIALOG (self));

  // TODO check keyboard state
  keyval_lower = gdk_keyval_to_lower (keyval);
  real_mask = state & gtk_accelerator_get_default_mod_mask();

  alt = !!(real_mask & GDK_ALT_MASK);
  ctrl = !!(real_mask & GDK_CONTROL_MASK);
  shift = !!(real_mask & GDK_SHIFT_MASK);
  super = !!(real_mask & GDK_SUPER_MASK);

  /* Totally unnecessary modifier indicators */
  gtk_widget_set_sensitive (self->alt_label, !is_alt (keyval_lower) && alt);
  gtk_widget_set_sensitive (self->ctrl_label, !is_ctrl (keyval_lower) && ctrl);
  gtk_widget_set_sensitive (self->shift_label, !is_shift (keyval_lower) && shift);
  gtk_widget_set_sensitive (self->super_label, !is_super (keyval_lower) && super);

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
 * GObject
 */
static void
valent_input_dialog_constructed (GObject *object)
{
  ValentInputDialog *self = VALENT_INPUT_DIALOG (object);

  g_object_get (self->plugin,
                "device", &self->device,
                NULL);

  g_object_bind_property (self->plugin, "remote-state",
                          self->editor, "sensitive",
                          G_BINDING_SYNC_CREATE);

  G_OBJECT_CLASS (valent_input_dialog_parent_class)->constructed (object);
}

static void
valent_input_dialog_finalize (GObject *object)
{
  ValentInputDialog *self = VALENT_INPUT_DIALOG (object);

  g_clear_object (&self->plugin);
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
    case PROP_PLUGIN:
      g_value_set_object (value, self->plugin);
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
    case PROP_PLUGIN:
      self->plugin = g_value_dup_object (value);
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

  object_class->constructed = valent_input_dialog_constructed;
  object_class->finalize = valent_input_dialog_finalize;
  object_class->get_property = valent_input_dialog_get_property;
  object_class->set_property = valent_input_dialog_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/mousepad/valent-input-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentInputDialog, editor);
  gtk_widget_class_bind_template_child (widget_class, ValentInputDialog, touchpad);

  gtk_widget_class_bind_template_child (widget_class, ValentInputDialog, alt_label);
  gtk_widget_class_bind_template_child (widget_class, ValentInputDialog, ctrl_label);
  gtk_widget_class_bind_template_child (widget_class, ValentInputDialog, shift_label);
  gtk_widget_class_bind_template_child (widget_class, ValentInputDialog, super_label);

  properties [PROP_PLUGIN] =
    g_param_spec_object ("plugin",
                         "Plugin",
                         "The plugin",
                         VALENT_TYPE_MOUSEPAD_PLUGIN,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  signals [TEXT_CHANGED] =
    g_signal_new ("text-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
  g_signal_set_va_marshaller (signals [TEXT_CHANGED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__STRINGv);
}

static void
valent_input_dialog_init (ValentInputDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->controller = gtk_event_controller_key_new ();
  gtk_widget_add_controller (self->editor, self->controller);

  g_signal_connect (self->controller,
                    "key-pressed",
                    G_CALLBACK (on_key_pressed),
                    self);
  g_signal_connect (self->controller,
                    "key-released",
                    G_CALLBACK (on_key_released),
                    self);
}

/**
 * valent_input_dialog_new:
 * @plugin: a #ValentMousepadPlugin
 *
 * Create a new input dialog.
 *
 * Returns: (transfer full): a new #ValentInputDialog
 */
ValentInputDialog *
valent_input_dialog_new (ValentMousepadPlugin *plugin)
{
  GApplication *application;
  GtkWindow *window = NULL;

  application = g_application_get_default ();

  if (application != NULL)
    window = gtk_application_get_active_window (GTK_APPLICATION (application));

  return g_object_new (VALENT_TYPE_INPUT_DIALOG,
                       "use-header-bar", TRUE,
                       "plugin",         plugin,
                       "transient-for",  window,
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

