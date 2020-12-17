// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-date-label"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>

#include "valent-date-label.h"


/**
 * SECTION:valent-date-label
 * @short_description: Base widget for rows in Valent
 * @title: ValentDateLabel
 * @stability: Unstable
 *
 * #ValentDateLabel is a convenience widget for a label that displays a timestamp
 * in relative terms. Examples might be "Just Now", "5 minutes", "Mon", etc.
 *
 * Every instance of #ValentDateLabel is updated using a global source that runs
 * once a minute, to ensure the relative labels are always accurate.
 *
 * There are several high-level modes to choose from.
 */

struct _ValentDateLabel
{
  GtkWidget  parent_instance;

  GtkLabel  *label;
  gint64     date;
  guint      mode;
};

G_DEFINE_TYPE (ValentDateLabel, valent_date_label, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_DATE,
  PROP_LENGTH,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static GPtrArray *label_cache = NULL;
static guint      label_source = 0;


static gboolean
update_labels (gpointer user_data)
{
  g_assert (label_cache != NULL);

  for (guint i = 0; i < label_cache->len; i++)
    valent_date_label_update (g_ptr_array_index (label_cache, i));

  return G_SOURCE_CONTINUE;
}

static void
valent_date_label_finalize (GObject *object)
{
  ValentDateLabel *label = VALENT_DATE_LABEL (object);

  /* Remove from the list of labels */
  g_ptr_array_remove (label_cache, label);

  /* If this was the last label, destroy the timer */
  if G_UNLIKELY (label_cache->len == 0)
    {
      g_source_remove (label_source);
      label_source = 0;
      g_clear_pointer (&label_cache, g_ptr_array_unref);
    }

  gtk_widget_unparent (GTK_WIDGET (label->label));

  G_OBJECT_CLASS (valent_date_label_parent_class)->finalize (object);
}

static void
valent_date_label_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ValentDateLabel *self = VALENT_DATE_LABEL (object);

  switch (prop_id)
    {
    case PROP_DATE:
      g_value_set_int64 (value, self->date);
      break;

    case PROP_LENGTH:
      g_value_set_uint (value, self->mode);
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

  switch (prop_id)
    {
    case PROP_DATE:
      valent_date_label_set_date (self, g_value_get_int64 (value));
      break;

    case PROP_LENGTH:
      valent_date_label_set_mode (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_date_label_init (ValentDateLabel *label)
{
  GtkStyleContext *style;

  label->label = GTK_LABEL (gtk_label_new (NULL));
  gtk_widget_insert_after (GTK_WIDGET (label->label), GTK_WIDGET (label), NULL);

  style = gtk_widget_get_style_context (GTK_WIDGET (label->label));
  gtk_style_context_add_class (style, "dim-label");

  /* Ensure the list of labels is ready */
  if G_UNLIKELY (label_cache == NULL)
    {
      label_cache = g_ptr_array_new ();
      label_source = g_timeout_add_seconds_full (G_PRIORITY_DEFAULT_IDLE,
                                                 60,
                                                 update_labels,
                                                 NULL,
                                                 NULL);
    }

  g_ptr_array_add (label_cache, label);
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
    g_param_spec_int64 ("date",
                        "Date",
                        "The timestamp this label represents",
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
  properties [PROP_LENGTH] =
    g_param_spec_uint ("mode",
                       "Length",
                       "The brevity of the label",
                       0, G_MAXUINT32,
                       0,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

/**
 * valent_date_label_long:
 * @date: a UNIX epoch timestamp
 *
 * Return a human-readable timestamp.
 *
 * Returns: (transfer full):  A timestamp similar to what Android Messages uses
 */
char *
valent_date_label_long (gint64 date)
{
  g_autoptr (GDateTime) dt = NULL;
  g_autoptr (GDateTime) now = NULL;
  GTimeSpan diff;

  dt = g_date_time_new_from_unix_local (date / 1000);
  now = g_date_time_new_now_local ();
  diff = g_date_time_difference (now, dt);

  /* TRANSLATORS: Less than a minute ago */
  if (diff < G_TIME_SPAN_MINUTE)
      return g_strdup (_("Just now"));

  /* TRANSLATORS: Time duration in minutes (eg. 15 minutes) */
  if (diff < G_TIME_SPAN_HOUR)
    {
      guint n_minutes;

      n_minutes = (diff / G_TIME_SPAN_MINUTE);
      return g_strdup_printf (ngettext("%d minute", "%d minutes", n_minutes),
                              n_minutes);
    }

  /* TRANSLATORS: Yesterday, but less than 24 hours (eg. Yesterday · 11:29 PM) */
  if (diff < G_TIME_SPAN_DAY)
    {
      g_autofree char *time_str = NULL;
      gint today, day;

      today = g_date_time_get_day_of_month(now);
      day = g_date_time_get_day_of_month(dt);
      time_str = g_date_time_format(dt, "%l:%M %p");

      if (today == day)
        return g_steal_pointer (&time_str);
      else
        return g_strdup_printf (_("Yesterday · %s"), time_str);
    }

  /* Less than a week ago */
  if (diff < (G_TIME_SPAN_DAY * 7))
    return g_date_time_format(dt, "%a");

  /* More than a week ago */
  return g_date_time_format(dt, "%b %e");
}

/**
 * valent_date_label_short:
 * @date: a UNIX epoch timestamp
 *
 * Return a human-readable timestamp.
 *
 * Returns: (transfer full):  A timestamp similar to what Android Messages uses
 */
char *
valent_date_label_short (gint64 date)
{
  g_autoptr (GDateTime) dt = NULL;
  g_autoptr (GDateTime) now = NULL;
  GTimeSpan diff;

  dt = g_date_time_new_from_unix_local (date / 1000);
  now = g_date_time_new_now_local ();
  diff = g_date_time_difference (now, dt);

  // TRANSLATORS: Less than a minute ago
  if (diff < G_TIME_SPAN_MINUTE)
      return g_strdup (_("Just now"));

  // TRANSLATORS: Time duration in minutes (eg. 15 minutes)
  if (diff < G_TIME_SPAN_HOUR)
    {
      guint n_minutes;

      n_minutes = (diff / G_TIME_SPAN_MINUTE);
      return g_strdup_printf (ngettext("%d minute", "%d minutes", n_minutes),
                              n_minutes);
    }


  /* Less than a day ago */
  if (diff < G_TIME_SPAN_DAY)
    return g_date_time_format(dt, "%l:%M %p");

  /* Less than a week ago */
  if (diff < (G_TIME_SPAN_DAY * 7))
    return g_date_time_format(dt, "%a");

  /* More than a week ago */
  return g_date_time_format(dt, "%b %e");
}

/**
 * valent_date_label_new:
 * @date: a UNIX epoch timestamp
 *
 * Create a new #ValentDateLabel for @timestamp.
 *
 * Returns: (transfer full): a #GtkWidget
 */
GtkWidget *
valent_date_label_new (gint64 date)
{
  return g_object_new (VALENT_TYPE_DATE_LABEL,
                       "date", date,
                       NULL);
}

/**
 * valent_date_label_get_date:
 * @label: a #ValentDateLabel
 *
 * Get the UNIX epoch timestamp (ms) for @label.
 *
 * Returns: the timestamp
 */
gint64
valent_date_label_get_date (ValentDateLabel *label)
{
  g_return_val_if_fail (VALENT_IS_DATE_LABEL (label), 0);

  return label->date;
}

/**
 * valent_date_label_set_date:
 * @label: a #ValentDateLabel
 * @date: a UNIX epoch timestamp
 *
 * Set the timestamp for @label to @date.
 */
void
valent_date_label_set_date (ValentDateLabel *label,
                            gint64           date)
{
  g_return_if_fail (VALENT_IS_DATE_LABEL (label));

  if (label->date != date)
    {
      label->date = date;
      valent_date_label_update (label);
      g_object_notify_by_pspec (G_OBJECT (label), properties [PROP_DATE]);
    }
}

/**
 * valent_date_label_get_mode:
 * @label: a #ValentDateLabel
 *
 * Get the mode @label.
 *
 * Returns: the timestamp
 */
guint
valent_date_label_get_mode (ValentDateLabel *label)
{
  g_return_val_if_fail (VALENT_IS_DATE_LABEL (label), 0);

  return label->date;
}

/**
 * valent_date_label_set_mode:
 * @label: a #ValentDateLabel
 * @mode: a mode
 *
 * Set the mode of @label to @mode. Currently the options are `0` and `1`.
 */
void
valent_date_label_set_mode (ValentDateLabel *label,
                            guint            mode)
{
  g_return_if_fail (VALENT_IS_DATE_LABEL (label));

  if (label->mode != mode)
    {
      label->mode = mode;
      valent_date_label_update (label);
      g_object_notify_by_pspec (G_OBJECT (label), properties [PROP_LENGTH]);
    }
}

/**
 * valent_date_label_update:
 * @label: a #ValentDateLabel
 *
 * Update the displayed text of @label.
 */
void
valent_date_label_update (ValentDateLabel *label)
{
  g_autofree char *text = NULL;

  g_return_if_fail (VALENT_IS_DATE_LABEL (label));

  if (label->mode == 0)
    text = valent_date_label_short (label->date);
  else
    text = valent_date_label_long (label->date);

  gtk_label_set_label (label->label, text);
}

