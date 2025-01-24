// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-date-label"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-date-label.h"

G_DEFINE_ENUM_TYPE (ValentDateFormat, valent_date_format,
  G_DEFINE_ENUM_VALUE (VALENT_DATE_FORMAT_ADAPTIVE, "adaptive"),
  G_DEFINE_ENUM_VALUE (VALENT_DATE_FORMAT_ADAPTIVE_SHORT, "adaptive-short"),
  G_DEFINE_ENUM_VALUE (VALENT_DATE_FORMAT_TIME, "time"))

struct _ValentDateLabel
{
  GtkWidget         parent_instance;

  GtkWidget        *label;
  int64_t           date;
  ValentDateFormat  mode;
  double            xalign;
};

G_DEFINE_FINAL_TYPE (ValentDateLabel, valent_date_label, GTK_TYPE_WIDGET)

typedef enum {
  PROP_DATE = 1,
  PROP_MODE,
  PROP_XALIGN,
} ValentDateLabelProperty;

static GParamSpec *properties[PROP_XALIGN + 1] = { NULL, };

static GPtrArray *label_cache = NULL;
static unsigned int label_source = 0;


/*< private >
 * valent_date_label_string_adaptive:
 * @timestamp: a UNIX epoch timestamp (ms)
 * @abbreviated: use abbreviations for brevity
 *
 * Create a user friendly date-time string for @timestamp, in a relative format.
 *
 * Examples:
 *     - "Just now"
 *     - "15 minutes"
 *     - "11:45 PM"
 *     - "Yesterday · 11:45 PM"
 *     - "Tuesday"
 *     - "February 29"
 *
 * Abbreviated Examples:
 *     - "Just now"
 *     - "15 mins"
 *     - "11:45 PM"
 *     - "Tue"
 *     - "Feb 29"
 *
 * Returns: (transfer full): a new string
 */
static char *
valent_date_label_string_adaptive (int64_t  timestamp,
                                   gboolean abbreviated)
{
  g_autoptr (GDateTime) dt = NULL;
  g_autoptr (GDateTime) now = NULL;
  g_autofree char *date_str = NULL;
  g_autofree char *time_str = NULL;
  GTimeSpan diff;

  dt = g_date_time_new_from_unix_local (timestamp / 1000);
  now = g_date_time_new_now_local ();
  diff = g_date_time_difference (now, dt);

  /* TRANSLATORS: Less than a minute ago
   */
  if (diff < G_TIME_SPAN_MINUTE)
      return g_strdup (_("Just now"));

  if (diff < G_TIME_SPAN_HOUR)
    {
      unsigned int n_minutes;

      n_minutes = (diff / G_TIME_SPAN_MINUTE);

      if (abbreviated)
        {
          /* TRANSLATORS: Time duration in minutes, abbreviated (eg. 15 mins)
           */
          return g_strdup_printf (ngettext ("%d min", "%d mins", n_minutes),
                                  n_minutes);
        }
      else
        {
          /* TRANSLATORS: Time duration in minutes (eg. 15 minutes)
           */
          return g_strdup_printf (ngettext("%d minute", "%d minutes", n_minutes),
                                  n_minutes);
        }
    }

  time_str = g_date_time_format (dt, "%-l:%M %p");
  if (diff < G_TIME_SPAN_DAY)
    {
      int today = g_date_time_get_day_of_month (now);
      int day = g_date_time_get_day_of_month (dt);

      if (abbreviated || today == day)
        return g_steal_pointer (&time_str);

      /* TRANSLATORS: Yesterday, but less than 24 hours (eg. Yesterday · 11:45 PM)
       */
      return g_strdup_printf (_("Yesterday · %s"), time_str);
    }

  /* Less than a week ago (eg. Tuesday/Tue)
   */
  if (diff < G_TIME_SPAN_DAY * 7)
    {
      if (abbreviated)
        return g_date_time_format (dt, "%a");

      /* TRANSLATORS: Date and time (eg. Tuesday · 23:45:00 PM)
       */
      date_str = g_date_time_format (dt, "%A");
      return g_strdup_printf (_("%s · %s"), date_str, time_str);
    }

  /* More than a week ago (eg. Feb 29)
   */
  if (abbreviated)
    return g_date_time_format (dt, "%b %-e");

  /* TRANSLATORS: Date and time (eg. February 29 · 23:45:00 PM)
   */
  date_str = g_date_time_format(dt, "%B %-e");
  return g_strdup_printf (_("%s · %s"), date_str, time_str);
}

/*< private >
 * valent_date_label_string_absolute:
 * @timestamp: a UNIX epoch timestamp (ms)
 *
 * Create a user friendly time string for @timestamp, in an absolute
 * format.
 *
 * Examples:
 *     - "11:45 PM"
 *
 * Returns: (transfer full): a new string
 */
static char *
valent_date_label_string_time (int64_t timestamp)
{
  g_autoptr (GDateTime) dt = NULL;

  dt = g_date_time_new_from_unix_local (timestamp / 1000);
  return g_date_time_format (dt, "%-l:%M %p");
}

static void
valent_date_label_sync (ValentDateLabel *label)
{
  g_autofree char *text = NULL;
  g_autofree char *tooltip_text = NULL;

  g_return_if_fail (VALENT_IS_DATE_LABEL (label));

  switch (label->mode)
    {
    case VALENT_DATE_FORMAT_ADAPTIVE:
      text = valent_date_label_string_adaptive (label->date, FALSE);
      break;

    case VALENT_DATE_FORMAT_ADAPTIVE_SHORT:
      text = valent_date_label_string_adaptive (label->date, TRUE);
      tooltip_text = valent_date_label_string_adaptive (label->date, FALSE);
      break;

    case VALENT_DATE_FORMAT_TIME:
      text = valent_date_label_string_time (label->date);
      break;

    default:
      break;
    }

  gtk_label_set_label (GTK_LABEL (label->label), text);
  gtk_widget_set_tooltip_text (GTK_WIDGET (label->label), tooltip_text);
  if (tooltip_text != NULL)
    {
      gtk_accessible_update_property (GTK_ACCESSIBLE (label->label),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, tooltip_text,
                                      -1);
    }
}

static gboolean
valent_date_label_sync_func (gpointer user_data)
{
  for (unsigned int i = 0; i < label_cache->len; i++)
    valent_date_label_sync (g_ptr_array_index (label_cache, i));

  return G_SOURCE_CONTINUE;
}

/*
 * GObject
 */
static void
valent_date_label_finalize (GObject *object)
{
  ValentDateLabel *self = VALENT_DATE_LABEL (object);

  /* Remove from update list */
  g_ptr_array_remove (label_cache, self);

  if (label_cache->len == 0)
    {
      g_clear_handle_id (&label_source, g_source_remove);
      g_clear_pointer (&label_cache, g_ptr_array_unref);
    }

  g_clear_pointer (&self->label, gtk_widget_unparent);

  G_OBJECT_CLASS (valent_date_label_parent_class)->finalize (object);
}

static void
valent_date_label_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ValentDateLabel *self = VALENT_DATE_LABEL (object);

  switch ((ValentDateLabelProperty)prop_id)
    {
    case PROP_DATE:
      g_value_set_int64 (value, valent_date_label_get_date (self));
      break;

    case PROP_MODE:
      g_value_set_enum (value, valent_date_label_get_mode (self));
      break;

    case PROP_XALIGN:
      g_value_set_double (value, self->xalign);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_date_label_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ValentDateLabel *self = VALENT_DATE_LABEL (object);

  switch ((ValentDateLabelProperty)prop_id)
    {
    case PROP_DATE:
      valent_date_label_set_date (self, g_value_get_int64 (value));
      break;

    case PROP_MODE:
      valent_date_label_set_mode (self, g_value_get_enum (value));
      break;

    case PROP_XALIGN:
      self->xalign = g_value_get_double (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_date_label_class_init (ValentDateLabelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = valent_date_label_finalize;
  object_class->get_property = valent_date_label_get_property;
  object_class->set_property = valent_date_label_set_property;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "date-label");

  /**
   * ValentDateLabel:date
   *
   * The timestamp this label represents.
   */
  properties [PROP_DATE] =
    g_param_spec_int64 ("date", NULL, NULL,
                        0, G_MAXINT64,
                        0,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentDateLabel:mode
   *
   * The brevity of the label.
   */
  properties [PROP_MODE] =
    g_param_spec_enum ("mode", NULL, NULL,
                       VALENT_TYPE_DATE_FORMAT,
                       VALENT_DATE_FORMAT_ADAPTIVE,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  /**
   * ValentDateLabel:xalign
   *
   * The X alignment of the label.
   */
  properties [PROP_XALIGN] =
    g_param_spec_double ("xalign", NULL, NULL,
                         0.0, 1.0,
                         0.5,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_date_label_init (ValentDateLabel *self)
{
  self->label = gtk_label_new (NULL);
  gtk_widget_insert_after (self->label, GTK_WIDGET (self), NULL);

  g_object_bind_property (self,        "xalign",
                          self->label, "xalign",
                          G_BINDING_BIDIRECTIONAL);

  if (label_cache == NULL)
    {
      label_cache = g_ptr_array_new ();
      label_source = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT_IDLE,
                                                 60,
                                                 valent_date_label_sync_func,
                                                 NULL,
                                                 NULL);
    }

  g_ptr_array_add (label_cache, self);
}

/**
 * valent_date_label_get_date:
 * @label: a `ValentDateLabel`
 *
 * Get the UNIX epoch timestamp (ms) for @label.
 *
 * Returns: the timestamp
 */
int64_t
valent_date_label_get_date (ValentDateLabel *label)
{
  g_return_val_if_fail (VALENT_IS_DATE_LABEL (label), 0);

  return label->date;
}

/**
 * valent_date_label_set_date:
 * @label: a `ValentDateLabel`
 * @date: a UNIX epoch timestamp
 *
 * Set the timestamp for @label to @date.
 */
void
valent_date_label_set_date (ValentDateLabel *label,
                            int64_t          date)
{
  g_return_if_fail (VALENT_IS_DATE_LABEL (label));

  if (label->date == date)
    return;

  label->date = date;
  valent_date_label_sync (label);
  g_object_notify_by_pspec (G_OBJECT (label), properties [PROP_DATE]);
}

/**
 * valent_date_label_get_mode:
 * @label: a `ValentDateLabel`
 *
 * Get the display mode @label.
 *
 * Returns: the display mode
 */
ValentDateFormat
valent_date_label_get_mode (ValentDateLabel *label)
{
  g_return_val_if_fail (VALENT_IS_DATE_LABEL (label), 0);

  return label->mode;
}

/**
 * valent_date_label_set_mode:
 * @label: a `ValentDateLabel`
 * @mode: a mode
 *
 * Set the mode of @label to @mode. Currently the options are `0` and `1`.
 */
void
valent_date_label_set_mode (ValentDateLabel  *label,
                            ValentDateFormat  mode)
{
  g_return_if_fail (VALENT_IS_DATE_LABEL (label));

  if (label->mode == mode)
    return;

  label->mode = mode;
  valent_date_label_sync (label);
  g_object_notify_by_pspec (G_OBJECT (label), properties [PROP_MODE]);
}

