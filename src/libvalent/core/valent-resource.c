// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-resource"

#include "config.h"

#include <gio/gio.h>

#include "valent-object.h"

#include "valent-resource.h"

/**
 * ValentResource:
 *
 * `ValentResource` is an interface that represents a resource.
 *
 * It is based on the properties in the elements namespace of the Dublin Core
 * DCMI Metadata Terms, primarily to represent SPARQL resources and runtime
 * objects with similar semantics.
 *
 * See: https://www.dublincore.org/specifications/dublin-core/dcmi-terms/#section-3
 *
 * Since: 1.0
 */

typedef struct
{
  GStrv           contributor;
  char           *coverage;
  char           *creator;
  GDateTime      *date;
  char           *description;
  char           *format;
  char           *identifier;
  char           *iri;
  char           *language;
  char           *publisher;
  GStrv           relation;
  char           *rights;
  ValentResource *source;
  char           *subject;
  char           *title;
  char           *type_hint;
} ValentResourcePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (ValentResource, valent_resource, VALENT_TYPE_OBJECT)

typedef enum
{
  PROP_CONTRIBUTOR = 1,
  PROP_COVERAGE,
  PROP_CREATOR,
  PROP_DATE,
  PROP_DESCRIPTION,
  PROP_FORMAT,
  PROP_IDENTIFIER,
  PROP_IRI,
  PROP_LANGUAGE,
  PROP_PUBLISHER,
  PROP_RELATION,
  PROP_RIGHTS,
  PROP_SOURCE,
  PROP_SUBJECT,
  PROP_TITLE,
  PROP_TYPE_HINT,
} ValentResourceProperty;

static GParamSpec *properties[PROP_TYPE_HINT + 1] = { NULL, };

static void
on_source_destroyed (ValentObject   *object,
                     ValentResource *self)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (self);

  g_assert (VALENT_IS_OBJECT (object));
  g_assert (VALENT_IS_RESOURCE (self));

  priv->source = NULL;
}

/*
 * ValentResource
 */
static void
valent_resource_real_update (ValentResource *resource,
                             ValentResource *update)
{
  g_assert (VALENT_IS_RESOURCE (resource));
  g_assert (VALENT_IS_RESOURCE (update));
}

static void
valent_resource_set_source (ValentResource *resource,
                            ValentResource *source)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_assert (VALENT_IS_RESOURCE (resource));
  g_assert (source == NULL || VALENT_IS_RESOURCE (source));

  if (source != NULL)
    {
      priv->source = source;
      g_signal_connect_object (source,
                               "destroy",
                               G_CALLBACK (on_source_destroyed),
                               resource,
                               G_CONNECT_DEFAULT);
    }
}

/*
 * GObject
 */
static void
valent_resource_finalize (GObject *object)
{
  ValentResource *self = VALENT_RESOURCE (object);
  ValentResourcePrivate *priv = valent_resource_get_instance_private (self);

  g_clear_pointer (&priv->contributor, g_strfreev);
  g_clear_pointer (&priv->coverage, g_free);
  g_clear_pointer (&priv->creator, g_free);
  g_clear_pointer (&priv->date, g_date_time_unref);
  g_clear_pointer (&priv->description, g_free);
  g_clear_pointer (&priv->format, g_free);
  g_clear_pointer (&priv->identifier, g_free);
  g_clear_pointer (&priv->iri, g_free);
  g_clear_pointer (&priv->language, g_free);
  g_clear_pointer (&priv->publisher, g_free);
  g_clear_pointer (&priv->relation, g_strfreev);
  g_clear_pointer (&priv->rights, g_free);
  g_clear_pointer (&priv->subject, g_free);
  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->type_hint, g_free);

  G_OBJECT_CLASS (valent_resource_parent_class)->finalize (object);
}

static void
valent_resource_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  ValentResource *self = VALENT_RESOURCE (object);
  ValentResourcePrivate *priv = valent_resource_get_instance_private (self);

  switch ((ValentResourceProperty)prop_id)
    {
    case PROP_CONTRIBUTOR:
      g_value_set_boxed (value, priv->contributor);
      break;

    case PROP_COVERAGE:
      g_value_set_string (value, priv->coverage);
      break;

    case PROP_CREATOR:
      g_value_set_string (value, priv->creator);
      break;

    case PROP_DATE:
      g_value_set_boxed (value, priv->date);
      break;

    case PROP_DESCRIPTION:
      g_value_set_string (value, priv->description);
      break;

    case PROP_FORMAT:
      g_value_set_string (value, priv->format);
      break;

    case PROP_IDENTIFIER:
      g_value_set_string (value, priv->identifier);
      break;

    case PROP_IRI:
      g_value_set_string (value, priv->iri);
      break;

    case PROP_LANGUAGE:
      g_value_set_string (value, priv->language);
      break;

    case PROP_PUBLISHER:
      g_value_set_string (value, priv->publisher);
      break;

    case PROP_RELATION:
      g_value_set_boxed (value, priv->relation);
      break;

    case PROP_RIGHTS:
      g_value_set_string (value, priv->rights);
      break;

    case PROP_SOURCE:
      g_value_set_object (value, priv->source);
      break;

    case PROP_SUBJECT:
      g_value_set_string (value, priv->subject);
      break;

    case PROP_TITLE:
      g_value_set_string (value, priv->title);
      break;

    case PROP_TYPE_HINT:
      g_value_set_string (value, priv->type_hint);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_resource_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  ValentResource *self = VALENT_RESOURCE (object);
  ValentResourcePrivate *priv = valent_resource_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CONTRIBUTOR:
      valent_resource_set_contributor (self, g_value_get_boxed (value));
      break;

    case PROP_COVERAGE:
      valent_resource_set_coverage (self, g_value_get_string (value));
      break;

    case PROP_CREATOR:
      valent_resource_set_creator (self, g_value_get_string (value));
      break;

    case PROP_DATE:
      valent_resource_set_date (self, g_value_get_boxed (value));
      break;

    case PROP_DESCRIPTION:
      valent_resource_set_description (self, g_value_get_string (value));
      break;

    case PROP_FORMAT:
      valent_resource_set_format (self, g_value_get_string (value));
      break;

    case PROP_IDENTIFIER:
      valent_resource_set_identifier (self, g_value_get_string (value));
      break;

    case PROP_IRI:
      g_assert (priv->iri == NULL);
      priv->iri = g_value_dup_string (value);
      break;

    case PROP_LANGUAGE:
      valent_resource_set_language (self, g_value_get_string (value));
      break;

    case PROP_PUBLISHER:
      valent_resource_set_publisher (self, g_value_get_string (value));
      break;

    case PROP_RELATION:
      valent_resource_set_relation (self, g_value_get_boxed (value));
      break;

    case PROP_RIGHTS:
      valent_resource_set_rights (self, g_value_get_string (value));
      break;

    case PROP_SOURCE:
      valent_resource_set_source (self, g_value_get_object (value));
      break;

    case PROP_SUBJECT:
      valent_resource_set_subject (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      valent_resource_set_title (self, g_value_get_string (value));
      break;

    case PROP_TYPE_HINT:
      valent_resource_set_type_hint (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_resource_class_init (ValentResourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_resource_finalize;
  object_class->get_property = valent_resource_get_property;
  object_class->set_property = valent_resource_set_property;

  klass->update = valent_resource_real_update;

  /**
   * ValentResource:contributor: (getter get_contributor) (setter set_contributor)
   *
   * An entity responsible for making contributions to the resource.
   *
   * The guidelines for using names of persons or organizations as creators also
   * apply to contributors. Typically, the name of a Contributor should be used
   * to indicate the entity.
   *
   * Since: 1.0
   */
  properties[PROP_CONTRIBUTOR] =
    g_param_spec_boxed ("contributor", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentResource:coverage: (getter get_coverage) (setter set_coverage)
   *
   * The spatial or temporal topic of the resource, spatial applicability of
   * the resource, or jurisdiction under which the resource is relevant.
   *
   * Spatial topic and spatial applicability may be a named place or a location
   * specified by its geographic coordinates. Temporal topic may be a named
   * period, date, or date range. A jurisdiction may be a named administrative
   * entity or a geographic place to which the resource applies. Recommended
   * practice is to use a controlled vocabulary such as the Getty Thesaurus of
   * Geographic Names [TGN]. Where appropriate, named places or time periods
   * may be used in preference to numeric identifiers such as sets of
   * coordinates or date ranges.
   *
   * Since: 1.0
   */
  properties[PROP_COVERAGE] =
    g_param_spec_string ("coverage", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentResource:creator: (getter get_creator) (setter set_creator)
   *
   * An entity primarily responsible for making the resource.
   *
   * Examples of a Creator include a person, an organization, or a service.
   * Typically, the name of a Creator should be used to indicate the entity.
   *
   * Since: 1.0
   */
  properties[PROP_CREATOR] =
    g_param_spec_string ("creator", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentResource:date: (getter get_date) (setter set_date)
   *
   * A point or period of time associated with an event in the lifecycle of
   * the resource.
   *
   * Date may be used to express temporal information at any level of
   * granularity. Recommended practice is to express the date, date/time, or
   * period of time according to ISO 8601-1 [ISO 8601-1] or a published profile
   * of the ISO standard, such as the W3C Note on Date and Time Formats [W3CDTF]
   * or the Extended Date/Time Format Specification [EDTF]. If the full date is
   * unknown, month and year (YYYY-MM) or just year (YYYY) may be used. Date
   * ranges may be specified using ISO 8601 period of time specification in
   * which start and end dates are separated by a '/' (slash) character. Either
   * the start or end date may be missing.
   *
   * Since: 1.0
   */
  properties[PROP_DATE] =
    g_param_spec_boxed ("date", NULL, NULL,
                        G_TYPE_DATE_TIME,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentResource:description: (getter get_description) (setter set_description)
   *
   * An account of the resource.
   *
   * Description may include but is not limited to: an abstract, a table of
   * contents, a graphical representation, or a free-text account of the
   * resource.
   *
   * Since: 1.0
   */
  properties[PROP_DESCRIPTION] =
    g_param_spec_string ("description", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentResource:format: (getter get_format) (setter set_format)
   *
   * The file format, physical medium, or dimensions of the resource.
   *
   * Recommended practice is to use a controlled vocabulary where available. For
   * example, for file formats one could use the list of Internet Media Types
   * [MIME](https://www.iana.org/assignments/media-types/media-types.xhtml).
   *
   * Since: 1.0
   */
  properties[PROP_FORMAT] =
    g_param_spec_string ("format", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentResource:identifier: (getter get_identifier) (setter set_identifier)
   *
   * An unambiguous reference to the resource within a given context.
   *
   * Recommended practice is to identify the resource by means of a string
   * conforming to an identification system.
   *
   * Since: 1.0
   */
  properties[PROP_IDENTIFIER] =
    g_param_spec_string ("identifier", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentResource:iri: (getter get_iri)
   *
   * The resource IRI (Internationalized Resource Identifier).
   *
   * Since: 1.0
   */
  properties[PROP_IRI] =
    g_param_spec_string ("iri", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentResource:language: (getter get_language) (setter set_language)
   *
   * A list of related resources from which the described resource is derived.
   *
   * Recommended practice is to use either a non-literal value representing a
   * language from a controlled vocabulary such as ISO 639-2 or ISO 639-3, or a
   * literal value consisting of an IETF Best Current Practice 47 [IETF-BCP47]
   * language tag.
   *
   * Since: 1.0
   */
  properties[PROP_LANGUAGE] =
    g_param_spec_string ("language", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentResource:publisher: (getter get_publisher) (setter set_publisher)
   *
   * An entity responsible for making the resource available.
   *
   * Examples of a Publisher include a person, an organization, or a service.
   * Typically, the name of a Publisher should be used to indicate the entity.
   *
   * Since: 1.0
   */
  properties[PROP_PUBLISHER] =
    g_param_spec_string ("publisher", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentResource:relation: (getter get_relation) (setter set_relation)
   *
   * A related resource.
   *
   * Recommended practice is to identify the related resource by means of a
   * URI. If this is not possible or feasible, a string conforming to a formal
   * identification system may be provided.
   *
   * Since: 1.0
   */
  properties[PROP_RELATION] =
    g_param_spec_boxed ("relation", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentResource:rights: (getter get_rights) (setter set_rights)
   *
   * Information about rights held in and over the resource.
   *
   * Typically, rights information includes a statement about various property
   * rights associated with the resource, including intellectual property
   * rights.
   *
   * Since: 1.0
   */
  properties[PROP_RIGHTS] =
    g_param_spec_string ("rights", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentResource:source: (getter get_source)
   *
   * A related resource from which the described resource is derived.
   *
   * The described resource may be derived from the related resource in whole
   * or in part. Recommended best practice is to identify the related resource
   * by means of a string conforming to a formal identification system.
   *
   * Since: 1.0
   */
  properties[PROP_SOURCE] =
    g_param_spec_object ("source", NULL, NULL,
                         VALENT_TYPE_RESOURCE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentResource:subject: (getter get_subject) (setter set_subject)
   *
   * The topic of the resource.
   *
   * Typically, the subject will be represented using keywords, key phrases, or
   * classification codes. Recommended best practice is to use a controlled
   * vocabulary.
   *
   * Since: 1.0
   */
  properties[PROP_SUBJECT] =
    g_param_spec_string ("subject", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentResource:title: (getter get_title) (setter set_title)
   *
   * A name given to the resource.
   *
   * Since: 1.0
   */
  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentResource:type-hint: (getter get_type_hint) (setter set_type_hint)
   *
   * The nature or genre of the resource.
   *
   * Recommended practice is to use a controlled vocabulary such as the DCMI
   * Type Vocabulary [DCMI-TYPE]. To describe the file format, physical medium,
   * or dimensions of the resource, use [property@Valent.Resource:format].
   *
   * Since: 1.0
   */
  properties[PROP_TYPE_HINT] =
    g_param_spec_string ("type-hint", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_resource_init (ValentResource *self)
{
}

/**
 * valent_resource_get_ancestor:
 * @resource: a `ValentResource`
 * @rtype: a `GType`
 *
 * Get the closest @rtype ancestor of @resource.
 *
 * Returns: (type Valent.Resource) (transfer none) (nullable): the ancestor
 *   of @resource
 *
 * Since: 1.0
 */
gpointer
valent_resource_get_ancestor (ValentResource *resource,
                              GType           rtype)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);
  ValentResource *ancestor = NULL;

  g_return_val_if_fail (VALENT_IS_RESOURCE (resource), NULL);

  while (priv->source != NULL)
    {
      if (g_type_is_a (G_OBJECT_TYPE (priv->source), rtype))
        return priv->source;

      ancestor = priv->source;
      priv = valent_resource_get_instance_private (ancestor);
    }

  return NULL;
}

/**
 * valent_resource_get_contributor: (get-property contributor)
 * @resource: a `ValentResource`
 *
 * Gets the contributor of @resource.
 *
 * Returns: (transfer none) (nullable): the contributor of @resource
 *
 * Since: 1.0
 */
GStrv
valent_resource_get_contributor (ValentResource *resource)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_val_if_fail (VALENT_IS_RESOURCE (resource), NULL);

  return priv->contributor;
}

/**
 * valent_resource_set_contributor: (set-property contributor)
 * @resource: a `ValentResource`
 * @contributor: (nullable): the new contributor
 *
 * Gets the title of @resource.
 *
 * Since: 1.0
 */
void
valent_resource_set_contributor (ValentResource *resource,
                                 GStrv           contributor)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_if_fail (VALENT_IS_RESOURCE (resource));

  if (priv->contributor != contributor)
    {
      g_clear_pointer (&priv->contributor, g_strfreev);
      priv->contributor = g_strdupv (contributor);
      g_object_notify_by_pspec (G_OBJECT (resource), properties[PROP_CONTRIBUTOR]);
    }
}

/**
 * valent_resource_get_coverage: (get-property coverage)
 * @resource: a `ValentResource`
 *
 * Gets the coverage of @resource.
 *
 * Returns: (transfer none) (nullable): the coverage of @resource
 *
 * Since: 1.0
 */
const char *
valent_resource_get_coverage (ValentResource *resource)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_val_if_fail (VALENT_IS_RESOURCE (resource), NULL);

  return priv->coverage;
}

/**
 * valent_resource_set_coverage: (set-property coverage)
 * @resource: a `ValentResource`
 * @coverage: (nullable): the new coverage
 *
 * Set the coverage of @resource to @coverage.
 *
 * Since: 1.0
 */
void
valent_resource_set_coverage (ValentResource *resource,
                              const char     *coverage)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_if_fail (VALENT_IS_RESOURCE (resource));

  if (g_set_str (&priv->coverage, coverage))
    g_object_notify_by_pspec (G_OBJECT (resource), properties[PROP_COVERAGE]);
}

/**
 * valent_resource_get_creator: (get-property creator)
 * @resource: a `ValentResource`
 *
 * Gets the creator of @resource.
 *
 * Returns: (transfer none) (nullable): the creator of @resource
 *
 * Since: 1.0
 */
const char *
valent_resource_get_creator (ValentResource *resource)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_val_if_fail (VALENT_IS_RESOURCE (resource), NULL);

  return priv->creator;
}

/**
 * valent_resource_set_creator: (set-property creator)
 * @resource: a `ValentResource`
 * @creator: (nullable): the new creator
 *
 * Set the creator of @resource to @creator.
 *
 * Since: 1.0
 */
void
valent_resource_set_creator (ValentResource *resource,
                             const char     *creator)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_if_fail (VALENT_IS_RESOURCE (resource));

  if (g_set_str (&priv->creator, creator))
    g_object_notify_by_pspec (G_OBJECT (resource), properties[PROP_CREATOR]);
}

/**
 * valent_resource_get_date: (get-property date)
 * @resource: a `ValentResource`
 *
 * Gets the date of @resource.
 *
 * Returns: (transfer none) (nullable): the date of @resource
 *
 * Since: 1.0
 */
GDateTime *
valent_resource_get_date (ValentResource *resource)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_val_if_fail (VALENT_IS_RESOURCE (resource), NULL);

  return priv->date;
}

/**
 * valent_resource_set_date: (set-property date)
 * @resource: a `ValentResource`
 * @date: (nullable): the new date
 *
 * Set the date of @resource to @date.
 *
 * Since: 1.0
 */
void
valent_resource_set_date (ValentResource *resource,
                          GDateTime      *date)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_if_fail (VALENT_IS_RESOURCE (resource));

  if (priv->date == date)
    return;

  if (priv->date && date && g_date_time_equal (priv->date, date))
    return;

  g_clear_pointer (&priv->date, g_date_time_unref);
  if (date != NULL)
    priv->date = g_date_time_ref (date);

  g_object_notify_by_pspec (G_OBJECT (resource), properties[PROP_DATE]);
}

/**
 * valent_resource_get_description: (get-property description)
 * @resource: a `ValentResource`
 *
 * Gets the description of @resource.
 *
 * Returns: (transfer none) (nullable): the description of @resource
 *
 * Since: 1.0
 */
const char *
valent_resource_get_description (ValentResource *resource)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_val_if_fail (VALENT_IS_RESOURCE (resource), NULL);

  return priv->description;
}

/**
 * valent_resource_set_description: (set-property description)
 * @resource: a `ValentResource`
 * @description: (nullable): the new description
 *
 * Set the description of @resource to @description.
 *
 * Since: 1.0
 */
void
valent_resource_set_description (ValentResource *resource,
                                 const char     *description)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_if_fail (VALENT_IS_RESOURCE (resource));

  if (g_set_str (&priv->description, description))
    g_object_notify_by_pspec (G_OBJECT (resource), properties[PROP_DESCRIPTION]);
}

/**
 * valent_resource_get_format: (get-property format)
 * @resource: a `ValentResource`
 *
 * Gets the format of @resource.
 *
 * Returns: (transfer none) (nullable): the format of @resource
 *
 * Since: 1.0
 */
const char *
valent_resource_get_format (ValentResource *resource)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_val_if_fail (VALENT_IS_RESOURCE (resource), NULL);

  return priv->format;
}

/**
 * valent_resource_set_format: (set-property format)
 * @resource: a `ValentResource`
 * @format: (nullable): the new format
 *
 * Set the format of @resource to @format.
 *
 * Since: 1.0
 */
void
valent_resource_set_format (ValentResource *resource,
                            const char     *format)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_if_fail (VALENT_IS_RESOURCE (resource));

  if (g_set_str (&priv->format, format))
    g_object_notify_by_pspec (G_OBJECT (resource), properties[PROP_FORMAT]);
}

/**
 * valent_resource_get_identifier: (get-property identifier)
 * @resource: a `ValentResource`
 *
 * Gets the identifier of @resource.
 *
 * Returns: (transfer none) (nullable): the identifier of @resource
 *
 * Since: 1.0
 */
const char *
valent_resource_get_identifier (ValentResource *resource)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_val_if_fail (VALENT_IS_RESOURCE (resource), NULL);

  return priv->identifier;
}

/**
 * valent_resource_set_identifier: (set-property identifier)
 * @resource: a `ValentResource`
 * @identifier: (nullable): the new identifier
 *
 * Set the identifier of @resource to @identifier.
 *
 * Since: 1.0
 */
void
valent_resource_set_identifier (ValentResource *resource,
                                const char     *identifier)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_if_fail (VALENT_IS_RESOURCE (resource));

  if (g_set_str (&priv->identifier, identifier))
    g_object_notify_by_pspec (G_OBJECT (resource), properties[PROP_IDENTIFIER]);
}

/**
 * valent_resource_get_iri: (get-property iri)
 * @resource: a `ValentResource`
 *
 * Gets the IRI of @resource.
 *
 * Returns: (transfer none) (nullable): the IRI of @resource
 *
 * Since: 1.0
 */
const char *
valent_resource_get_iri (ValentResource *resource)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_val_if_fail (VALENT_IS_RESOURCE (resource), NULL);

  return priv->iri;
}

/**
 * valent_resource_get_language: (get-property language)
 * @resource: a `ValentResource`
 *
 * Gets the language of @resource.
 *
 * Returns: (transfer none) (nullable): the language of @resource
 *
 * Since: 1.0
 */
const char *
valent_resource_get_language (ValentResource *resource)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_val_if_fail (VALENT_IS_RESOURCE (resource), NULL);

  return priv->language;
}

/**
 * valent_resource_set_language: (set-property language)
 * @resource: a `ValentResource`
 * @language: (nullable): the new language
 *
 * Set the language of @resource to @language.
 *
 * Since: 1.0
 */
void
valent_resource_set_language (ValentResource *resource,
                              const char     *language)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_if_fail (VALENT_IS_RESOURCE (resource));

  if (g_set_str (&priv->language, language))
    g_object_notify_by_pspec (G_OBJECT (resource), properties[PROP_LANGUAGE]);
}

/**
 * valent_resource_get_publisher: (get-property publisher)
 * @resource: a `ValentResource`
 *
 * Gets the publisher of @resource.
 *
 * Returns: (transfer none) (nullable): the publisher of @resource
 *
 * Since: 1.0
 */
const char *
valent_resource_get_publisher (ValentResource *resource)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_val_if_fail (VALENT_IS_RESOURCE (resource), NULL);

  return priv->publisher;
}

/**
 * valent_resource_set_publisher: (set-property publisher)
 * @resource: a `ValentResource`
 * @publisher: (nullable): the new publisher
 *
 * Set the publisher of @resource to @publisher.
 *
 * Since: 1.0
 */
void
valent_resource_set_publisher (ValentResource *resource,
                               const char     *publisher)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_if_fail (VALENT_IS_RESOURCE (resource));

  if (g_set_str (&priv->publisher, publisher))
    g_object_notify_by_pspec (G_OBJECT (resource), properties[PROP_PUBLISHER]);
}

/**
 * valent_resource_get_relation: (get-property relation)
 * @resource: a `ValentResource`
 *
 * Gets the relation of @resource.
 *
 * Returns: (transfer none) (nullable): the relation of @resource
 *
 * Since: 1.0
 */
GStrv
valent_resource_get_relation (ValentResource *resource)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_val_if_fail (VALENT_IS_RESOURCE (resource), NULL);

  return priv->relation;
}

/**
 * valent_resource_set_relation: (set-property relation)
 * @resource: a `ValentResource`
 * @relation: (nullable): the new relation
 *
 * Gets the title of @resource.
 *
 * Since: 1.0
 */
void
valent_resource_set_relation (ValentResource *resource,
                              GStrv           relation)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_if_fail (VALENT_IS_RESOURCE (resource));

  if (priv->relation != relation)
    {
      g_clear_pointer (&priv->relation, g_strfreev);
      priv->contributor = g_strdupv (relation);
      g_object_notify_by_pspec (G_OBJECT (resource), properties[PROP_RELATION]);
    }
}

/**
 * valent_resource_get_rights: (get-property rights)
 * @resource: a `ValentResource`
 *
 * Gets the rights of @resource.
 *
 * Returns: (transfer none) (nullable): the rights of @resource
 *
 * Since: 1.0
 */
const char *
valent_resource_get_rights (ValentResource *resource)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_val_if_fail (VALENT_IS_RESOURCE (resource), NULL);

  return priv->rights;
}

/**
 * valent_resource_set_rights: (set-property rights)
 * @resource: a `ValentResource`
 * @rights: (nullable): the new rights
 *
 * Set the rights of @resource to @rights.
 *
 * Since: 1.0
 */
void
valent_resource_set_rights (ValentResource *resource,
                            const char     *rights)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_if_fail (VALENT_IS_RESOURCE (resource));

  if (g_set_str (&priv->rights, rights))
    g_object_notify_by_pspec (G_OBJECT (resource), properties[PROP_RIGHTS]);
}

/**
 * valent_resource_get_root:
 * @resource: a `ValentResource`
 *
 * Get the root source of @resource.
 *
 * In practice, the root of every resource should be a [class@Valent.DataSource].
 *
 * Returns: (type Valent.Resource) (transfer none) (nullable): the source
 *   of @resource
 *
 * Since: 1.0
 */
gpointer
valent_resource_get_root (ValentResource *resource)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);
  ValentResource *root = resource;

  g_return_val_if_fail (VALENT_IS_RESOURCE (resource), NULL);

  while (priv->source != NULL)
    {
      root = priv->source;
      priv = valent_resource_get_instance_private (root);
    }

  return root;
}

/**
 * valent_resource_get_source: (get-property source)
 * @resource: a `ValentResource`
 *
 * Gets the source of @resource.
 *
 * Returns: (type Valent.Resource) (transfer none) (nullable): the source
 *   of @resource
 *
 * Since: 1.0
 */
gpointer
valent_resource_get_source (ValentResource *resource)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_val_if_fail (VALENT_IS_RESOURCE (resource), NULL);

  return priv->source;
}

/**
 * valent_resource_get_subject: (get-property subject)
 * @resource: a `ValentResource`
 *
 * Gets the subject of @resource.
 *
 * Returns: (transfer none) (nullable): the subject of @resource
 *
 * Since: 1.0
 */
const char *
valent_resource_get_subject (ValentResource *resource)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_val_if_fail (VALENT_IS_RESOURCE (resource), NULL);

  return priv->subject;
}

/**
 * valent_resource_set_subject: (set-property subject)
 * @resource: a `ValentResource`
 * @subject: (nullable): the new subject
 *
 * Set the subject of @resource to @subject.
 *
 * Since: 1.0
 */
void
valent_resource_set_subject (ValentResource *resource,
                             const char     *subject)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_if_fail (VALENT_IS_RESOURCE (resource));

  if (g_set_str (&priv->subject, subject))
    g_object_notify_by_pspec (G_OBJECT (resource), properties[PROP_SUBJECT]);
}

/**
 * valent_resource_get_title: (get-property title)
 * @resource: a `ValentResource`
 *
 * Gets the title of @resource.
 *
 * Returns: (transfer none) (nullable): the title of @resource
 *
 * Since: 1.0
 */
const char *
valent_resource_get_title (ValentResource *resource)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_val_if_fail (VALENT_IS_RESOURCE (resource), NULL);

  return priv->title;
}

/**
 * valent_resource_set_title: (set-property title)
 * @resource: a `ValentResource`
 * @title: (nullable): the new title
 *
 * Set the title of @resource to @title.
 *
 * Since: 1.0
 */
void
valent_resource_set_title (ValentResource *resource,
                           const char     *title)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_if_fail (VALENT_IS_RESOURCE (resource));

  if (g_set_str (&priv->title, title))
    g_object_notify_by_pspec (G_OBJECT (resource), properties[PROP_TITLE]);
}

/**
 * valent_resource_get_type_hint: (get-property type-hint)
 * @resource: a `ValentResource`
 *
 * Gets the type hint of @resource.
 *
 * Returns: (transfer none) (nullable): the nature or genre of @resource
 *
 * Since: 1.0
 */
const char *
valent_resource_get_type_hint (ValentResource *resource)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_val_if_fail (VALENT_IS_RESOURCE (resource), NULL);

  return priv->type_hint;
}

/**
 * valent_resource_set_type_hint: (set-property type-hint)
 * @resource: a `ValentResource`
 * @type_hint: (nullable): the new type_hint
 *
 * Set the nature or genre of @resource to @type_hint.
 *
 * Since: 1.0
 */
void
valent_resource_set_type_hint (ValentResource *resource,
                               const char     *type_hint)
{
  ValentResourcePrivate *priv = valent_resource_get_instance_private (resource);

  g_return_if_fail (VALENT_IS_RESOURCE (resource));

  if (g_set_str (&priv->type_hint, type_hint))
    g_object_notify_by_pspec (G_OBJECT (resource), properties[PROP_TYPE_HINT]);
}

/**
 * valent_resource_update: (virtual update)
 * @resource: a `ValentResource`
 * @update: the resource update
 *
 * Update @resource from @update.
 *
 * Since: 1.0
 */
void
valent_resource_update (ValentResource *resource,
                        ValentResource *update)
{
  g_return_if_fail (VALENT_IS_RESOURCE (resource));
  g_return_if_fail (VALENT_IS_RESOURCE (update));

  VALENT_RESOURCE_GET_CLASS (resource)->update (resource, update);
}

