// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-notifications"

#include "config.h"

#include <gio/gio.h>

#include "valent-notification.h"

/**
 * SECTION:valentnotification
 * @short_description: An abstraction of notifications
 * @title: ValentNotification
 * @stability: Unstable
 * @include: libvalent-notifications.h
 *
 * The #ValentNotification class is intended to abstract notifications with the intent of allowing
 * them to be presented to the user in different ways. For example as a desktop #GNotification,
 * in-app notification or other method.
 *
 * #ValentNotification is effectively a super-set of #GNotification that includes a few properties
 * normally found in libnotify notifications, while being more read-write friendly than
 * #GNotification.
 */

struct _ValentNotification
{
  GObject                parent_instance;

  char                  *application;
  char                  *id;
  char                  *title;
  char                  *body;
  GIcon                 *icon;
  gint64                 time;
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
  PROP_ICON_NAME,
  PROP_ID,
  PROP_PRIORITY,
  PROP_TIME,
  PROP_TITLE,
  N_PROPERTIES
};

G_DEFINE_TYPE (ValentNotification, valent_notification, G_TYPE_OBJECT)

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
  g_clear_pointer (&self->title, g_free);
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

    case PROP_TITLE:
      g_value_set_string (value, valent_notification_get_title (self));
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

    case PROP_ICON_NAME:
      valent_notification_set_icon_from_string (self, g_value_get_string (value));
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

    case PROP_TITLE:
      valent_notification_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_notification_init (ValentNotification *notification)
{
  notification->id = g_uuid_string_random ();
  notification->title = NULL;
  notification->body = NULL;
  notification->icon = NULL;
  notification->priority = G_NOTIFICATION_PRIORITY_NORMAL;

  notification->default_action = NULL;
  notification->default_action_target = NULL;
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
   * The "action" is a convenience setter for valent_notification_set_action().
   */
  properties [PROP_ACTION] =
    g_param_spec_string ("action",
                         "Action",
                         "Action name for the notification",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotification:application:
   *
   * The "application" property is the name of the notifying application.
   */
  properties [PROP_APPLICATION] =
    g_param_spec_string ("application",
                         "Application",
                         "Application name",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotification:body:
   *
   * The "body" property is the secondary text of the notification.
   */
  properties [PROP_BODY] =
    g_param_spec_string ("body",
                         "Body",
                         "Body for the notification",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotification:icon:
   *
   * The "icon" property is the #GIcon for the notification.
   */
  properties [PROP_ICON] =
    g_param_spec_object ("icon",
                         "Icon",
                         "GIcon for the notification",
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotification:icon-name:
   *
   * The "icon-name" property is an icon name string for the device type.
   */
  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon Name",
                         "Icon name representing the device",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotification:id:
   *
   * The "id" property is a unique string for the device, usually the hostname.
   */
  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "Unique id for the notification",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotification:priority:
   *
   * The "priority" property is a unique string for the device, usually the hostname.
   */
  properties [PROP_PRIORITY] =
    g_param_spec_enum ("priority",
                         "Priority",
                         "Unique id for the notification",
                         G_TYPE_NOTIFICATION_PRIORITY,
                         G_NOTIFICATION_PRIORITY_NORMAL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotification:time:
   *
   * The "time" property is the posting time of the notification in ms.
   */
  properties [PROP_TIME] =
    g_param_spec_int64 ("time",
                        "Time",
                        "Posting time of the notification",
                        G_MININT64, G_MAXINT64,
                        0,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotification:title:
   *
   * The "title" property is the primary text of the notification.
   */
  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "Title for the notification",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

/**
 * valent_notification_new:
 * @title: (nullable): a notification title
 *
 * Create a new #ValentNotification. While a notification without a title (or
 * primary text) is not strictly possible, this is allowed during creation as a
 * convenience for the case where @title is selected from choices later.
 *
 * Returns: (transfer full) (type Valent.Notification): a new notification.
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
 * @notification: a #ValentNotification
 * @action: a detailed action
 *
 * Sets the default action of @notification to @action. @action may be a
 * detailed action as parse by g_action_parse_detailed_name().
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
 * valent_notification_get_application:
 * @notification: a #ValentNotification
 *
 * Get the name of the notifying application for @notification.
 *
 * Returns: (transfer none) (nullable): the notifying application name
 */
const char *
valent_notification_get_application (ValentNotification *notification)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION (notification), NULL);

  return notification->application;
}

/**
 * valent_notification_set_application:
 * @notification: a #ValentNotification
 * @name: (nullable): an body for the notification
 *
 * Set the name of the notifying application for @notification to @name.
 */
void
valent_notification_set_application (ValentNotification *notification,
                                     const char         *name)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));

  if (g_strcmp0 (notification->application, name) == 0)
    return;

  g_clear_pointer (&notification->application, g_free);
  notification->application = g_strdup (name);
  g_object_notify_by_pspec (G_OBJECT (notification), properties [PROP_APPLICATION]);
}

/**
 * valent_notification_get_body:
 * @notification: a #ValentNotification
 *
 * Get the body for @notification.
 *
 * Returns: (transfer none) (nullable): the notification body
 */
const char *
valent_notification_get_body (ValentNotification *notification)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION (notification), NULL);

  return notification->body;
}

/**
 * valent_notification_set_body:
 * @notification: a #ValentNotification
 * @body: (nullable): a body for the notification
 *
 * Set the body for @notification.
 */
void
valent_notification_set_body (ValentNotification *notification,
                              const char         *body)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));

  if (g_strcmp0 (notification->application, body) == 0)
    return;

  g_clear_pointer (&notification->body, g_free);
  notification->body = g_strdup (body);
  g_object_notify_by_pspec (G_OBJECT (notification), properties [PROP_BODY]);
}

/**
 * valent_notification_get_icon:
 * @notification: a #ValentNotification
 *
 * Get the #GIcon for @notification.
 *
 * Returns: (transfer none) (type Gio.Icon): the
 */
GIcon *
valent_notification_get_icon (ValentNotification *notification)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION (notification), NULL);

  return notification->icon;
}

/**
 * valent_notification_set_icon:
 * @notification: a #ValentNotification
 * @icon: (nullable): a #GIcon
 *
 * Set the #GIcon for @notification, taking a reference on @icon if not %NULL.
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
 * valent_notification_set_icon_from_string:
 * @notification: a #ValentNotification
 * @icon_name: (nullable): a themed icon name
 *
 * Set the #GIcon for @notification from a themed icon name.
 */
void
valent_notification_set_icon_from_string (ValentNotification *notification,
                                          const char         *icon_name)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));

  if (icon_name != NULL)
    {
      g_autoptr (GIcon) icon = NULL;

      icon = g_icon_new_for_string (icon_name, NULL);
      g_set_object (&notification->icon, icon);
    }
  else
    g_clear_object (&notification->icon);
}

/**
 * valent_notification_get_id:
 * @notification: a #ValentNotification
 *
 * Get the id for @notification.
 *
 * Returns: (transfer none) (not nullable): the notification id
 */
const char *
valent_notification_get_id (ValentNotification *notification)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION (notification), NULL);
  g_assert (notification->id != NULL);

  return notification->id;
}

/**
 * valent_notification_set_id:
 * @notification: a #ValentNotification
 * @id: (not nullable): an id for the notification
 *
 * Set the id for @notification.
 */
void
valent_notification_set_id (ValentNotification *notification,
                            const char         *id)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));
  g_return_if_fail (id != NULL);

  if (g_str_equal (notification->id, id))
    return;

  g_clear_pointer (&notification->id, g_free);
  notification->id = g_strdup (id);
  g_object_notify_by_pspec (G_OBJECT (notification), properties [PROP_ID]);
}

/**
 * valent_notification_get_priority:
 * @notification: a #ValentNotification
 *
 * Get the priority for @notification.
 *
 * Returns: (type GNotificationPriority): the notification id
 */
GNotificationPriority
valent_notification_get_priority (ValentNotification *notification)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION (notification), 0);

  return notification->priority;
}

/**
 * valent_notification_set_priority:
 * @notification: a #ValentNotification
 * @priority: (type GNotificationPriority): a priority
 *
 * Set the priority for @notification.
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
 * valent_notification_get_time:
 * @notification: a #ValentNotification
 *
 * Get the posted time for @notification in ms.
 *
 * Returns: a UNIX epoch timestamp
 */
gint64
valent_notification_get_time (ValentNotification *notification)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION (notification), 0);

  return notification->time;
}

/**
 * valent_notification_set_time:
 * @notification: a #ValentNotification
 * @time: a millisecond timestamp
 *
 * Set the posted time for @notification in ms.
 */
void
valent_notification_set_time (ValentNotification *notification,
                              gint64              time)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));

  if (notification->time == time)
    return;

  notification->time = time;
  g_object_notify_by_pspec (G_OBJECT (notification), properties [PROP_TIME]);
}

/**
 * valent_notification_get_title:
 * @notification: a #ValentNotification
 *
 * Get the title for @notification.
 *
 * Returns: (transfer none) (not nullable): the notification title
 */
const char *
valent_notification_get_title (ValentNotification *notification)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION (notification), NULL);

  return notification->title;
}

/**
 * valent_notification_set_title:
 * @notification: a #ValentNotification
 * @title: (not nullable): a title for the notification
 *
 * Set the title for @notification.
 */
void
valent_notification_set_title (ValentNotification *notification,
                               const char         *title)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));
  g_return_if_fail (title != NULL);

  if (g_strcmp0 (notification->title, title) == 0)
    return;

  g_clear_pointer (&notification->title, g_free);
  notification->title = g_strdup (title);
  g_object_notify_by_pspec (G_OBJECT (notification), properties [PROP_TITLE]);
}

/**
 * valent_notification_add_button_with_target:
 * @notification: a #ValentNotification
 * @label: a label for the button
 * @action: an action name
 * @target: (type GVariant) (nullable): an action target
 *
 * Add a button to @notification.
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
 * @notification: a #ValentNotification
 * @label: a label for the button
 * @action: an action name
 *
 * Add a button to @notification.
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
 * @notification: a #ValentNotification
 * @action: an action name
 * @target: (nullable): a #GVariant to use as @action's parameter
 *
 * Sets the default action of @notification to @action. If @target is non-%NULL,
 * @action will be activated with @target as its parameter.
 */
void
valent_notification_set_action_and_target (ValentNotification *notification,
                                           const char         *action,
                                           GVariant           *target)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION (notification));
  g_return_if_fail (action != NULL && g_action_name_is_valid (action));

  g_clear_pointer (&notification->default_action, g_free);
  g_clear_pointer (&notification->default_action_target, g_variant_unref);

  notification->default_action = g_strdup (action);

  if (target)
    notification->default_action_target = g_variant_ref_sink (target);
}

/**
 * valent_notification_serialize:
 * @notification: a #ValentNotification
 *
 * Serializes @notification into a floating variant of type a{sv}.
 *
 * Returns: (nullable): a floating #GVariant
 */
GVariant *
valent_notification_serialize (ValentNotification *notification)
{
  GVariantBuilder builder;

  g_return_val_if_fail (VALENT_IS_NOTIFICATION (notification), NULL);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

  if (notification->id)
    g_variant_builder_add (&builder, "{sv}", "id",
                           g_variant_new_string (notification->id));

  if (notification->application)
    g_variant_builder_add (&builder, "{sv}", "application",
                           g_variant_new_string (notification->application));

  if (notification->title)
    g_variant_builder_add (&builder, "{sv}", "title",
                           g_variant_new_string (notification->title));

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

      g_variant_builder_add (&builder, "{sv}", "buttons", g_variant_builder_end (&actions_builder));
    }

  return g_variant_builder_end (&builder);
}

/**
 * valent_notification_deserialize:
 * @variant: a #GVariant
 *
 * Deserializes @variant into a #ValentNotification. Since #ValentNotificaton is
 * effectively a super-set of #GNotification, @variant may be a serialized
 * #GNotification or #ValentNotification.
 *
 * Returns: (transfer full) (nullable): a #ValentNotification
 */
ValentNotification *
valent_notification_deserialize (GVariant *variant)
{
  ValentNotification *notification;
  g_autoptr (GVariant) props = NULL;
  g_autoptr (GVariant) icon = NULL;
  g_autoptr (GVariant) buttons = NULL;
  const char *id, *title, *body, *priority, *application;
  const char *action;

  g_return_val_if_fail (g_variant_check_format_string (variant, "a{sv}", FALSE), NULL);

  notification = valent_notification_new (NULL);

  g_variant_get (variant, "@a{sv}", &props);

  if (g_variant_lookup (props, "id", "&s", &id))
    valent_notification_set_id (notification, id);

  if (g_variant_lookup (props, "application", "&s", &application))
    valent_notification_set_application (notification, application);

  if (g_variant_lookup (props, "title", "&s", &title))
    valent_notification_set_title (notification, title);

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

  if (g_variant_lookup (props, "default-action", "&s", &action))
    {
      g_autoptr (GVariant) target = NULL;

      if ((target = g_variant_lookup_value (props, "default-action-target", NULL)))
        valent_notification_set_action_and_target (notification, action, target);
      else
        valent_notification_set_action (notification, action);
    }

  if (g_variant_lookup (props, "buttons", "@aa{sv}", &buttons))
    {
      GVariantIter iter;
      gsize n_buttons;
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

