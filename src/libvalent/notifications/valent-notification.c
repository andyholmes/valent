// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-notifications"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>

#include "valent-notification.h"

/**
 * ValentNotification:
 *
 * A class representing a notification.
 *
 * `ValentNotification` is a derivable, generic class for a notification.
 *
 * Since: 1.0
 */

struct _ValentNotification
{
  ValentResource         parent_instance;

  char                  *application;
  char                  *id;
  char                  *body;
  GIcon                 *icon;
  int64_t                time;
  char                  *default_action;
  GVariant              *default_action_target;
  GPtrArray             *buttons;
  GNotificationPriority  priority;
};

enum {
  PROP_0,
  PROP_ACTION,
  PROP_APPLICATION,
  PROP_BODY,
  PROP_ICON,
  PROP_ID,
  PROP_PRIORITY,
  PROP_TIME,
  N_PROPERTIES
};

G_DEFINE_FINAL_TYPE (ValentNotification, valent_notification, VALENT_TYPE_RESOURCE)

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * Notification Buttons
 */
typedef struct
{
  char     *label;
  char     *action;
  GVariant *target;
} Button;

static void
notification_button_free (gpointer data)
{
  Button *button = data;

  g_clear_pointer (&button->label, g_free);
  g_clear_pointer (&button->action, g_free);
  g_clear_pointer (&button->target, g_variant_unref);
  g_free (data);
}


/*
 * (De)serializing helpers
 */
static GVariant *
valent_notification_serialize_button (Button *button)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

  g_variant_builder_add (&builder, "{sv}", "label", g_variant_new_string (button->label));
  g_variant_builder_add (&builder, "{sv}", "action", g_variant_new_string (button->action));

  if (button->target)
    g_variant_builder_add (&builder, "{sv}", "target", button->target);

  return g_variant_builder_end (&builder);
}

static GVariant *
valent_notification_get_priority_nick (ValentNotification *notification)
{
  g_autoptr (GEnumClass) enum_class = NULL;
  GEnumValue *enum_value;

  enum_class = g_type_class_ref (G_TYPE_NOTIFICATION_PRIORITY);
  enum_value = g_enum_get_value (enum_class, notification->priority);

  g_assert (enum_value != NULL);

  return g_variant_new_string (enum_value->value_nick);
}

static void
valent_notification_set_priority_nick (ValentNotification *notification,
                                       const char         *nick)
{
  g_autoptr (GEnumClass) enum_class = NULL;
  GEnumValue *enum_value;

  enum_class = g_type_class_ref (G_TYPE_NOTIFICATION_PRIORITY);
  enum_value = g_enum_get_value_by_nick (enum_class, nick);

  g_assert (enum_value != NULL);

  valent_notification_set_priority (notification, enum_value->value);
}


/*
 * GObject
 */
static void
valent_notification_finalize (GObject *object)
{
  ValentNotification *self = VALENT_NOTIFICATION (object);

  g_clear_pointer (&self->application, g_free);
  g_clear_pointer (&self->id, g_free);
  g_clear_pointer (&self->body, g_free);
  g_clear_object (&self->icon);

  g_clear_pointer (&self->default_action, g_free);
  g_clear_pointer (&self->default_action_target, g_variant_unref);
  g_clear_pointer (&self->buttons, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_notification_parent_class)->finalize (object);
}

static void
valent_notification_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentNotification *self = VALENT_NOTIFICATION (object);

  switch (prop_id)
    {
    case PROP_APPLICATION:
      g_value_set_string (value, valent_notification_get_application (self));
      break;

    case PROP_BODY:
      g_value_set_string (value, valent_notification_get_body (self));
      break;

    case PROP_ID:
      g_value_set_string (value, valent_notification_get_id (self));
      break;

    case PROP_ICON:
      g_value_set_object (value, valent_notification_get_icon (self));
      break;

    case PROP_PRIORITY:
      g_value_set_enum (value, valent_notification_get_priority (self));
      break;

    case PROP_TIME:
      g_value_set_int64 (value, valent_notification_get_time (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_notification_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ValentNotification *self = VALENT_NOTIFICATION (object);

  switch (prop_id)
    {
    case PROP_ACTION:
      valent_notification_set_action (self, g_value_get_string (value));
      break;

    case PROP_APPLICATION:
      valent_notification_set_application (self, g_value_get_string (value));
      break;

    case PROP_BODY:
      valent_notification_set_body (self, g_value_get_string(value));
      break;

    case PROP_ICON:
      valent_notification_set_icon (self, g_value_get_object (value));
      break;

    case PROP_ID:
      valent_notification_set_id (self, g_value_get_string (value));
      break;

    case PROP_PRIORITY:
      valent_notification_set_priority (self, g_value_get_enum (value));
      break;

    case PROP_TIME:
      valent_notification_set_time (self, g_value_get_int64 (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_notification_init (ValentNotification *notification)
{
  notification->id = g_uuid_string_random ();
  notification->buttons = g_ptr_array_new_full (3, notification_button_free);
}

static void
valent_notification_class_init (ValentNotificationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_notification_finalize;
  object_class->get_property = valent_notification_get_property;
  object_class->set_property = valent_notification_set_property;

  /**
   * ValentNotification:action:
   *
   * The default notification action.
   *
   * Since: 1.0
   */
  properties [PROP_ACTION] =
    g_param_spec_string ("action", NULL, NULL,
                         NULL,
                         (G_PARAM_WRITABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotification:application: (getter get_application) (setter set_application)
   *
   * The notifying application.
   *
   * The semantics of this property are not well-defined. It may be the
   * application name (i.e. the `appName` argument passed to
   * `org.freedesktop.Notifications.Notify()`), the desktop application ID (i.e.
   * from `org.gtk.Notifications.AddNotification()`) or some other identifying
   * string.
   *
   * Since: 1.0
   */
  properties [PROP_APPLICATION] =
    g_param_spec_string ("application", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotification:body: (getter get_body) (setter set_body)
   *
   * The notification body.
   *
   * Since: 1.0
   */
  properties [PROP_BODY] =
    g_param_spec_string ("body", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotification:icon: (getter get_icon) (setter set_icon)
   *
   * The notification [iface@Gio.Icon].
   *
   * Since: 1.0
   */
  properties [PROP_ICON] =
    g_param_spec_object ("icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotification:id: (getter get_id) (setter set_id)
   *
   * The unique ID of the notification.
   *
   * Since: 1.0
   */
  properties [PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotification:priority: (getter get_priority) (setter set_priority)
   *
   * The notification priority.
   *
   * Since: 1.0
   */
  properties [PROP_PRIORITY] =
    g_param_spec_enum ("priority", NULL, NULL,
                       G_TYPE_NOTIFICATION_PRIORITY,
                       G_NOTIFICATION_PRIORITY_NORMAL,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotification:time: (getter get_time) (setter set_time)
   *
   * The posting time of the notification in milliseconds.
   *
   * Since: 1.0
   */
  properties [PROP_TIME] =
    g_param_spec_int64 ("time", NULL, NULL,
                        0, G_MAXINT64,
                        0,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

/**
 * valent_notification_new:
 * @title: (nullable): a notification title
 *
 * Create a new `ValentNotification`.
 *
 * A notification without a title (or primary text) is not strictly possible,
 * but this is allowed during construction for the case where it is more
 * convenient to set it later.
 *
 * Returns: (transfer full): a `ValentNotification`
 *
 * Since: 1.0
 */
ValentNotification *
valent_notification_new (const char *title)
{
  if (title == NULL)
    return g_object_new (VALENT_TYPE_NOTIFICATION, NULL);
  else
    return g_object_new (VALENT_TYPE_NOTIFICATION,
                         "title", title,
                         NULL);
}

/**
 * valent_notification_set_action:
 * @notification: a `ValentNotification`
 * @action: a detailed action
 *
 * Sets the default notification action.
 *
 * @action may be a detailed action as parsed by
 * [func@Gio.Action.parse_detailed_name].
 *
 * Since: 1.0
 */
void
valent_notification_set_action (ValentNotification *notification,
                                const char         *action)
{
  g_autofree char *aname = NULL;
  g_autoptr (GVariant) atarget = NULL;
  g_autoptr (GError) error = NULL;

  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));

  if (!g_action_parse_detailed_name (action, &aname, &atarget, &error))
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      return;
    }

  valent_notification_set_action_and_target (notification, aname, atarget);
}

/**
 * valent_notification_get_application: (get-property application)
 * @notification: a `ValentNotification`
 *
 * Get the notifying application.
 *
 * Returns: (transfer none) (nullable): the notifying application name
 *
 * Since: 1.0
 */
const char *
valent_notification_get_application (ValentNotification *notification)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION (notification), NULL);

  return notification->application;
}

/**
 * valent_notification_set_application: (set-property application)
 * @notification: a `ValentNotification`
 * @application: (nullable): the notifying application
 *
 * Set the notifying application.
 *
 * Since: 1.0
 */
void
valent_notification_set_application (ValentNotification *notification,
                                     const char         *application)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));

  if (g_set_str (&notification->application, application))
    g_object_notify_by_pspec (G_OBJECT (notification), properties [PROP_APPLICATION]);
}

/**
 * valent_notification_get_body: (get-property body)
 * @notification: a `ValentNotification`
 *
 * Get the notification body.
 *
 * Returns: (transfer none) (nullable): the notification body
 *
 * Since: 1.0
 */
const char *
valent_notification_get_body (ValentNotification *notification)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION (notification), NULL);

  return notification->body;
}

/**
 * valent_notification_set_body: (set-property body)
 * @notification: a `ValentNotification`
 * @body: (nullable): a notification body
 *
 * Set the notification body.
 *
 * Since: 1.0
 */
void
valent_notification_set_body (ValentNotification *notification,
                              const char         *body)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));

  if (g_set_str (&notification->body, body))
    g_object_notify_by_pspec (G_OBJECT (notification), properties [PROP_BODY]);
}

/**
 * valent_notification_get_icon: (set-property icon)
 * @notification: a `ValentNotification`
 *
 * Get the notification icon.
 *
 * Returns: (transfer none) (nullable): a `GIcon`
 *
 * Since: 1.0
 */
GIcon *
valent_notification_get_icon (ValentNotification *notification)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION (notification), NULL);

  return notification->icon;
}

/**
 * valent_notification_set_icon: (set-property icon)
 * @notification: a `ValentNotification`
 * @icon: (nullable): a `GIcon`
 *
 * Set the notification icon.
 *
 * Since: 1.0
 */
void
valent_notification_set_icon (ValentNotification *notification,
                              GIcon              *icon)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));
  g_return_if_fail (icon == NULL || G_IS_ICON (icon));

  if (g_icon_equal (notification->icon, icon))
    return;

  g_set_object (&notification->icon, icon);
  g_object_notify_by_pspec (G_OBJECT (notification), properties [PROP_ICON]);
}

/**
 * valent_notification_get_id: (get-property id)
 * @notification: a `ValentNotification`
 *
 * Get the notification ID.
 *
 * Returns: (transfer none) (not nullable): a unique ID
 *
 * Since: 1.0
 */
const char *
valent_notification_get_id (ValentNotification *notification)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION (notification), NULL);

  return notification->id;
}

/**
 * valent_notification_set_id: (set-property id)
 * @notification: a `ValentNotification`
 * @id: (not nullable): a unique ID
 *
 * Set the notification ID.
 *
 * Since: 1.0
 */
void
valent_notification_set_id (ValentNotification *notification,
                            const char         *id)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));
  g_return_if_fail (id != NULL && *id != '\0');

  if (g_set_str (&notification->id, id))
    g_object_notify_by_pspec (G_OBJECT (notification), properties [PROP_ID]);
}

/**
 * valent_notification_get_priority:
 * @notification: a `ValentNotification`
 *
 * Get the notification priority.
 *
 * Returns: a `GNotificationPriority`
 *
 * Since: 1.0
 */
GNotificationPriority
valent_notification_get_priority (ValentNotification *notification)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION (notification), G_NOTIFICATION_PRIORITY_NORMAL);

  return notification->priority;
}

/**
 * valent_notification_set_priority:
 * @notification: a `ValentNotification`
 * @priority: a `GNotificationPriority`
 *
 * Set the notification priority.
 *
 * Since: 1.0
 */
void
valent_notification_set_priority (ValentNotification    *notification,
                                  GNotificationPriority  priority)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));

  if (notification->priority == priority)
    return;

  notification->priority = priority;
  g_object_notify_by_pspec (G_OBJECT (notification), properties [PROP_PRIORITY]);
}

/**
 * valent_notification_get_time: (get-property time)
 * @notification: a `ValentNotification`
 *
 * Get the notification time.
 *
 * Returns: a UNIX epoch timestamp (ms)
 *
 * Since: 1.0
 */
int64_t
valent_notification_get_time (ValentNotification *notification)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION (notification), 0);

  return notification->time;
}

/**
 * valent_notification_set_time: (set-property time)
 * @notification: a `ValentNotification`
 * @time: a UNIX epoch timestamp (ms)
 *
 * Set the notification time.
 *
 * Since: 1.0
 */
void
valent_notification_set_time (ValentNotification *notification,
                              int64_t             time)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));

  if (notification->time == time)
    return;

  notification->time = time;
  g_object_notify_by_pspec (G_OBJECT (notification), properties [PROP_TIME]);
}

/**
 * valent_notification_add_button_with_target:
 * @notification: a `ValentNotification`
 * @label: a button label
 * @action: an action name
 * @target: (nullable): an action target
 *
 * Add a notification button.
 *
 * Since: 1.0
 */
void
valent_notification_add_button_with_target (ValentNotification *notification,
                                            const char         *label,
                                            const char         *action,
                                            GVariant           *target)
{
  Button *button;

  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));
  g_return_if_fail (label != NULL);
  g_return_if_fail (action != NULL && g_action_name_is_valid (action));
  g_return_if_fail (notification->buttons->len < 3);

  button = g_new0 (Button, 1);
  button->label = g_strdup (label);
  button->action = g_strdup (action);

  if (target)
    button->target = g_variant_ref_sink (target);

  g_ptr_array_add (notification->buttons, button);
}

/**
 * valent_notification_add_button:
 * @notification: a `ValentNotification`
 * @label: a button label
 * @action: an action name
 *
 * Add a notification button.
 *
 * Since: 1.0
 */
void
valent_notification_add_button (ValentNotification *notification,
                                const char         *label,
                                const char         *action)
{
  g_autofree char *name = NULL;
  g_autoptr (GVariant) target = NULL;
  g_autoptr (GError) error = NULL;

  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));
  g_return_if_fail (label != NULL);
  g_return_if_fail (action != NULL);
  g_return_if_fail (notification->buttons->len < 3);

  if (!g_action_parse_detailed_name (action, &name, &target, &error))
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      return;
    }

  valent_notification_add_button_with_target (notification, label, name, target);
}

/**
 * valent_notification_set_action_and_target:
 * @notification: a `ValentNotification`
 * @action: an action name
 * @target: (nullable): a `GVariant` to use as @action's parameter
 *
 * Set the default notification action.
 *
 * If @target is non-%NULL, @action will be activated with @target as its
 * parameter.
 *
 * Since: 1.0
 */
void
valent_notification_set_action_and_target (ValentNotification *notification,
                                           const char         *action,
                                           GVariant           *target)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));
  g_return_if_fail (action != NULL && g_action_name_is_valid (action));

  g_set_str (&notification->default_action, action);
  g_clear_pointer (&notification->default_action_target, g_variant_unref);

  if (target)
    notification->default_action_target = g_variant_ref_sink (target);
}

/**
 * valent_notification_serialize:
 * @notification: a `ValentNotification`
 *
 * Serialize the notification into a variant of type `a{sv}`.
 *
 * Returns: (nullable): a floating `GVariant`
 *
 * Since: 1.0
 */
GVariant *
valent_notification_serialize (ValentNotification *notification)
{
  GVariantBuilder builder;
  const char *title;

  g_return_val_if_fail (VALENT_IS_NOTIFICATION (notification), NULL);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

  if (notification->id)
    g_variant_builder_add (&builder, "{sv}", "id",
                           g_variant_new_string (notification->id));

  if (notification->application)
    g_variant_builder_add (&builder, "{sv}", "application",
                           g_variant_new_string (notification->application));

  title = valent_resource_get_title (VALENT_RESOURCE (notification));
  if (title)
    g_variant_builder_add (&builder, "{sv}", "title",
                           g_variant_new_string (title));

  if (notification->body)
    g_variant_builder_add (&builder, "{sv}", "body",
                           g_variant_new_string (notification->body));

  if (notification->icon)
    {
      g_autoptr (GVariant) serialized_icon = NULL;

      if ((serialized_icon = g_icon_serialize (notification->icon)))
        g_variant_builder_add (&builder, "{sv}", "icon", serialized_icon);
    }

  g_variant_builder_add (&builder, "{sv}", "priority",
                         valent_notification_get_priority_nick (notification));

  if (notification->default_action)
    {
      g_variant_builder_add (&builder, "{sv}", "default-action",
                             g_variant_new_string (notification->default_action));

      if (notification->default_action_target)
        g_variant_builder_add (&builder, "{sv}", "default-action-target",
                               notification->default_action_target);
    }

  if (notification->buttons->len > 0)
    {
      GVariantBuilder actions_builder;

      g_variant_builder_init (&actions_builder, G_VARIANT_TYPE ("aa{sv}"));

      for (unsigned int i = 0; i < notification->buttons->len; i++)
        {
          Button *button;

          button = g_ptr_array_index (notification->buttons, i);
          g_variant_builder_add (&actions_builder, "@a{sv}",
                                 valent_notification_serialize_button (button));
        }

      g_variant_builder_add (&builder, "{sv}", "buttons",
                             g_variant_builder_end (&actions_builder));
    }

  return g_variant_builder_end (&builder);
}

/**
 * valent_notification_deserialize:
 * @variant: a `GVariant`
 *
 * Deserializes @variant into a `ValentNotification`. Since `ValentNotification`
 * is effectively a super-set of `GNotification`, @variant may be a serialized
 * `GNotification` or `ValentNotification`.
 *
 * Returns: (transfer full) (nullable): a `ValentNotification`
 *
 * Since: 1.0
 */
ValentNotification *
valent_notification_deserialize (GVariant *variant)
{
  ValentNotification *notification;
  g_autoptr (GVariant) props = NULL;
  g_autoptr (GVariant) icon = NULL;
  g_autoptr (GVariant) buttons = NULL;
  const char *id, *title, *body, *priority, *application;
  const char *default_action;

  g_return_val_if_fail (g_variant_check_format_string (variant, "a{sv}", FALSE), NULL);

  notification = valent_notification_new (NULL);

  g_variant_get (variant, "@a{sv}", &props);

  if (g_variant_lookup (props, "id", "&s", &id))
    valent_notification_set_id (notification, id);

  if (g_variant_lookup (props, "application", "&s", &application))
    valent_notification_set_application (notification, application);

  if (g_variant_lookup (props, "title", "&s", &title))
    valent_resource_set_title (VALENT_RESOURCE (notification), title);

  if (g_variant_lookup (props, "body", "&s", &body))
    valent_notification_set_body (notification, body);

  if (g_variant_lookup (props, "icon", "@(sv)", &icon))
    {
      g_autoptr (GIcon) gicon = NULL;

      gicon = g_icon_deserialize (icon);
      valent_notification_set_icon (notification, gicon);
    }

  if (g_variant_lookup (props, "priority", "&s", &priority))
    valent_notification_set_priority_nick (notification, priority);

  if (g_variant_lookup (props, "default-action", "&s", &default_action))
    {
      g_autoptr (GVariant) default_action_target = NULL;

      default_action_target = g_variant_lookup_value (props,
                                                      "default-action-target",
                                                      NULL);
      valent_notification_set_action_and_target (notification,
                                                 default_action,
                                                 default_action_target);
    }

  if (g_variant_lookup (props, "buttons", "@aa{sv}", &buttons))
    {
      GVariantIter iter;
      size_t n_buttons;
      GVariant *button;

      n_buttons = g_variant_iter_init (&iter, buttons);
      g_warn_if_fail (n_buttons <= 3);

      while (g_variant_iter_next (&iter, "@a{sv}", &button))
        {
          const char *label, *action;
          g_autoptr (GVariant) target = NULL;

          g_variant_lookup (button, "label", "&s", &label);
          g_variant_lookup (button, "action", "&s", &action);

          if ((target = g_variant_lookup_value (button, "target", NULL)))
            valent_notification_add_button_with_target (notification, label, action, target);
          else
            valent_notification_add_button (notification, label, action);

          g_variant_unref (button);
        }
    }

  return notification;
}

/**
 * valent_notification_hash:
 * @notification: (type Valent.Notification): a `ValentNotification`
 *
 * Converts a notification to a hash value, using g_str_hash() on the ID.
 *
 * Returns: a hash value
 *
 * Since: 1.0
 */
unsigned int
valent_notification_hash (gconstpointer notification)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION ((void *)notification), 0);

  return g_str_hash (((ValentNotification *)notification)->id);
}

/**
 * valent_notification_equal:
 * @notification1: (type Valent.Notification): a `ValentNotification`
 * @notification2: (type Valent.Notification): a `ValentNotification`
 *
 * Compare two notifications for equality by ID.
 *
 * Returns: %TRUE if equal, or %FALSE if not
 *
 * Since: 1.0
 */
gboolean
valent_notification_equal (gconstpointer notification1,
                           gconstpointer notification2)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION ((void *)notification1), FALSE);
  g_return_val_if_fail (VALENT_IS_NOTIFICATION ((void *)notification2), FALSE);

  return g_strcmp0 (((ValentNotification *)notification1)->id,
                    ((ValentNotification *)notification2)->id) == 0;
}

