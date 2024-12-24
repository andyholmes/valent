// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-plugin"

#include "config.h"

#include <gio/gio.h>
#include <libpeas.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "valent-debug.h"
#include "valent-data-source.h"
#include "valent-macros.h"
#include "valent-object.h"

#include "valent-plugin.h"

#define VALENT_PLUGIN_SETTINGS_SCHEMA "ca.andyholmes.Valent.Plugin"

/**
 * ValentPlugin:
 *
 * A container for [class@Valent.Extension] instances.
 *
 * `ValentPlugin` is a meta-object and container for [class@Valent.Extension]
 * instances. It is used to manage the creation and destruction of extension
 * instances, as well as the configured state.
 *
 * Since: 1.0
 */

struct _ValentPlugin
{
  ValentObject     parent_instance;

  gboolean         enabled;
  ValentExtension *extension;
  char            *extension_domain;
  PeasPluginInfo  *plugin_info;
  GType            plugin_type;
  GSettings       *settings;
  ValentResource  *source;
};

G_DEFINE_FINAL_TYPE (ValentPlugin, valent_plugin, VALENT_TYPE_OBJECT)

typedef enum {
  PROP_ENABLED = 1,
  PROP_EXTENSION,
  PROP_EXTENSION_DOMAIN,
  PROP_PLUGIN_INFO,
  PROP_PLUGIN_TYPE,
  PROP_SOURCE,
} ValentPluginProperty;

static GParamSpec *properties[PROP_SOURCE + 1] = { NULL, };

/*< private >
 * valent_plugin_get_settings_path:
 * @plugin_info: a `PeasPluginInfo`
 * @extension_domain: (nullable): a context for an extension
 *
 * Get a GSettings path for @plugin_info in @extension_domain.
 *
 * If @extension_domain is %NULL, the path will represent the plugin itself,
 * rather than any of its extensions.
 *
 * If given, @extension domain should be representative of the extension type
 * (e.g. [class@Valent.DevicePlugin] and `"device"`), to avoid conflicts in
 * plugins with more than one extension.
 *
 * Returns: (transfer full): a settings path
 */
static char *
valent_plugin_get_settings_path (PeasPluginInfo *plugin_info,
                                 const char     *extension_domain)
{
  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (extension_domain == NULL || *extension_domain != '\0');

  if (extension_domain != NULL)
    {
      return g_strdup_printf ("/ca/andyholmes/valent/plugin/%s/extension/%s/",
                              peas_plugin_info_get_module_name (plugin_info),
                              extension_domain);
    }
  else
    {
      return g_strdup_printf ("/ca/andyholmes/valent/plugin/%s/",
                              peas_plugin_info_get_module_name (plugin_info));
    }
}

/*< private >
 * valent_plugin_get_settings_schema:
 * @plugin_info: a `PeasPluginInfo`
 * @schema_id: a `GSettingsSchema` ID
 *
 * Load the [type@Gio.SettingsSchema] for @schema_id.
 *
 * Lookup @schema_id in the default source, and fallback to compiling GSettings
 * schemas in the module directory for @plugin_info.
 *
 * Returns: (transfer full) (nullable): a settings schema, or %NULL if not found
 */
static GSettingsSchema *
valent_plugin_get_settings_schema (PeasPluginInfo *plugin_info,
                                   const char     *schema_id)
{
  GSettingsSchemaSource *default_source;
  g_autoptr (GSettingsSchema) schema = NULL;

  g_assert (PEAS_IS_PLUGIN_INFO (plugin_info));
  g_assert (schema_id == NULL || *schema_id != '\0');

  default_source = g_settings_schema_source_get_default ();
  schema = g_settings_schema_source_lookup (default_source, schema_id, TRUE);

  /* Adapted from `peas-plugin-info.c` (LGPL-2.1-or-later)
   */
  if (schema == NULL)
    {
      g_autoptr (GSettingsSchemaSource) schema_source = NULL;
      g_autoptr (GFile) gschemas_compiled = NULL;
      const char *module_dir = NULL;

      module_dir = peas_plugin_info_get_module_dir (plugin_info);
      gschemas_compiled = g_file_new_build_filename (module_dir,
                                                     "gschemas.compiled",
                                                     NULL);

      if (!g_file_query_exists (gschemas_compiled, NULL))
        {
          const char *argv[] = {
            "glib-compile-schemas",
            "--targetdir", module_dir,
            module_dir,
            NULL
          };

          g_spawn_sync (NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH,
                        NULL, NULL, NULL, NULL, NULL, NULL);
        }

      schema_source = g_settings_schema_source_new_from_directory (module_dir,
                                                                   default_source,
                                                                   FALSE,
                                                                   NULL);

      if (schema_source != NULL)
        schema = g_settings_schema_source_lookup (schema_source, schema_id, TRUE);
    }

  return g_steal_pointer (&schema);
}

/*
 * GObject
 */
static void
valent_plugin_constructed (GObject *object)
{
  ValentPlugin *self = VALENT_PLUGIN (object);
  g_autoptr (GSettingsSchema) schema = NULL;
  g_autofree char *path = NULL;

  G_OBJECT_CLASS (valent_plugin_parent_class)->constructed (object);

  g_assert (VALENT_IS_DATA_SOURCE (self->source));
  g_assert (PEAS_IS_PLUGIN_INFO (self->plugin_info));
  g_assert (self->extension_domain != NULL && *self->extension_domain != '\0');

  path = valent_plugin_get_settings_path (self->plugin_info,
                                          self->extension_domain);
  schema = valent_plugin_get_settings_schema (self->plugin_info,
                                              VALENT_PLUGIN_SETTINGS_SCHEMA);

  self->settings = valent_data_source_get_settings_full (VALENT_DATA_SOURCE (self->source),
                                                         schema,
                                                         path);
  g_settings_bind (self->settings, "enabled",
                   self,           "enabled",
                   G_SETTINGS_BIND_DEFAULT);
  self->enabled = g_settings_get_boolean (self->settings, "enabled");
}

static void
valent_plugin_destroy (ValentObject *object)
{
  ValentPlugin *self = VALENT_PLUGIN (object);

  if (VALENT_IS_OBJECT (self->extension))
    {
      valent_object_destroy (VALENT_OBJECT (self->extension));
      g_clear_object (&self->extension);
    }

  g_clear_pointer (&self->extension_domain, g_free);
  g_clear_object (&self->plugin_info);
  g_clear_object (&self->source);
  g_clear_object (&self->settings);

  VALENT_OBJECT_CLASS (valent_plugin_parent_class)->destroy (object);
}

static void
valent_plugin_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ValentPlugin *self = VALENT_PLUGIN (object);

  switch ((ValentPluginProperty)prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, self->enabled);
      break;

    case PROP_EXTENSION_DOMAIN:
      g_value_set_string (value, self->extension_domain);
      break;

    case PROP_EXTENSION:
      g_value_set_object (value, self->extension);
      break;

    case PROP_PLUGIN_INFO:
      g_value_set_object (value, self->plugin_info);
      break;

    case PROP_PLUGIN_TYPE:
      g_value_set_gtype (value, self->plugin_type);
      break;

    case PROP_SOURCE:
      g_value_set_object (value, self->source);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_plugin_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  ValentPlugin *self = VALENT_PLUGIN (object);

  switch ((ValentPluginProperty)prop_id)
    {
    case PROP_ENABLED:
      valent_plugin_set_enabled (self, g_value_get_boolean (value));
      break;

    case PROP_EXTENSION:
      valent_plugin_set_extension (self, g_value_get_object (value));
      break;

    case PROP_EXTENSION_DOMAIN:
      self->extension_domain = g_value_dup_string (value);
      break;

    case PROP_PLUGIN_INFO:
      self->plugin_info = g_value_dup_object (value);
      break;

    case PROP_PLUGIN_TYPE:
      self->plugin_type = g_value_get_gtype (value);
      break;

    case PROP_SOURCE:
      self->source = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_plugin_class_init (ValentPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);

  object_class->constructed = valent_plugin_constructed;
  object_class->get_property = valent_plugin_get_property;
  object_class->set_property = valent_plugin_set_property;

  vobject_class->destroy = valent_plugin_destroy;

  /**
   * ValentPlugin:enabled: (getter get_enabled) (setter set_enabled)
   *
   * Whether the extension is enabled.
   *
   * Since: 1.0
   */
  properties [PROP_ENABLED] =
    g_param_spec_boolean ("enabled", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentPlugin:extension: (getter get_extension) (setter set_extension)
   *
   * The [class@Valent.Extension].
   *
   * Since: 1.0
   */
  properties [PROP_EXTENSION] =
    g_param_spec_object ("extension", NULL, NULL,
                         VALENT_TYPE_EXTENSION,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentPlugin:extension-domain:
   *
   * The domain of the extension this plugin will instantiate.
   *
   * Since: 1.0
   */
  properties [PROP_EXTENSION_DOMAIN] =
    g_param_spec_string ("extension-domain", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentPlugin:plugin-info:
   *
   * The source plugin.
   *
   * Since: 1.0
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_object ("plugin-info", NULL, NULL,
                         VALENT_TYPE_RESOURCE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentPlugin:plugin-type:
   *
   * The extension point [alias@GObject.Type].
   *
   * Since: 1.0
   */
  properties [PROP_PLUGIN_TYPE] =
    g_param_spec_gtype ("plugin-type", NULL, NULL,
                        G_TYPE_NONE,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentPlugin:source:
   *
   * The [class@Valent.DataSource] for the plugin.
   *
   * Since: 1.0
   */
  properties [PROP_SOURCE] =
    g_param_spec_object ("source", NULL, NULL,
                         VALENT_TYPE_RESOURCE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_plugin_init (ValentPlugin *self)
{
}

/**
 * valent_plugin_new:
 * @source: (nullable): a source plugin
 * @plugin_info: a `PeasPluginInfo`
 * @plugin_type: a `GType`
 * @extension_domain: a domain
 *
 * Create a new `ValentPlugin`.
 *
 * If @source is %NULL, the default local [class@Valent.DataSource] will be
 * used.
 *
 * If given, @domain should be an identifier describing the scope of the
 * plugins that will share it.
 *
 * Returns: (transfer full): a new `ValentPlugin`.
 *
 * Since: 1.0
 */
ValentPlugin *
valent_plugin_new (ValentResource *source,
                   PeasPluginInfo *plugin_info,
                   GType           plugin_type,
                   const char     *extension_domain)
{
  g_return_val_if_fail (source == NULL || VALENT_IS_RESOURCE (source), NULL);
  g_return_val_if_fail (PEAS_IS_PLUGIN_INFO (plugin_info), NULL);
  g_return_val_if_fail (extension_domain != NULL && *extension_domain != '\0', NULL);

  if (source == NULL)
    source = valent_data_source_get_local_default ();

  return g_object_new (VALENT_TYPE_PLUGIN,
                       "extension-domain", extension_domain,
                       "plugin-info",      plugin_info,
                       "plugin-type",      plugin_type,
                       "source",           source,
                       NULL);
}

/**
 * valent_plugin_create_extension:
 * @plugin: a `ValentPlugin`
 *
 * Create an instance of the target extension type.
 *
 * Returns: (type Valent.Extension) (transfer full): a new extension
 *
 * Since: 1.0
 */
gpointer
valent_plugin_create_extension (ValentPlugin *plugin)
{
  GObject *ret = NULL;
  const char *module = NULL;
  g_autofree char *iri = NULL;

  g_return_val_if_fail (VALENT_IS_PLUGIN (plugin), NULL);

  module = peas_plugin_info_get_module_name (plugin->plugin_info);
  iri = tracker_sparql_escape_uri_printf ("urn:valent:%s:%s",
                                          plugin->extension_domain,
                                          module);
  ret = peas_engine_create_extension (valent_get_plugin_engine (),
                                      plugin->plugin_info,
                                      plugin->plugin_type,
                                      "iri",           iri,
                                      "source",        plugin->source,
                                      "plugin-domain", plugin->extension_domain,
                                      NULL);
  //g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (ret), plugin->extension_type), NULL);
  g_return_val_if_fail (VALENT_IS_EXTENSION (ret), NULL);

  return g_steal_pointer (&ret);
}

/**
 * valent_plugin_get_enabled: (get-property enabled)
 * @plugin: a `ValentPlugin`
 *
 * Get whether the extension is enabled.
 *
 * Returns: %TRUE if enabled, %FALSE otherwise
 *
 * Since: 1.0
 */
gboolean
valent_plugin_get_enabled (ValentPlugin *plugin)
{
  g_return_val_if_fail (VALENT_IS_PLUGIN (plugin), FALSE);

  return plugin->enabled;
}

/**
 * valent_plugin_set_enabled: (set-property enabled)
 * @plugin: a `ValentPlugin`
 * @state: %TRUE to enable, %FALSE to disable
 *
 * Set whether the extension is enabled.
 *
 * Since: 1.0
 */
void
valent_plugin_set_enabled (ValentPlugin *plugin,
                           gboolean      state)
{
  g_return_if_fail (VALENT_IS_PLUGIN (plugin));

  state = !!state;
  if (plugin->enabled != state)
    {
      plugin->enabled = state;
      g_object_notify_by_pspec (G_OBJECT (plugin), properties[PROP_ENABLED]);
    }
}

/**
 * valent_plugin_get_extension: (get-property extension)
 * @plugin: a `ValentPlugin`
 *
 * Get the [class@Valent.Extension] for @plugin.
 *
 * Returns: (type Valent.Extension) (transfer none) (nullable): the extension
 *
 * Since: 1.0
 */
gpointer
valent_plugin_get_extension (ValentPlugin *plugin)
{
  g_return_val_if_fail (VALENT_IS_PLUGIN (plugin), NULL);

  return plugin->extension;
}

static void
on_extension_destroy (ValentExtension *extension,
                      ValentPlugin    *self)
{
  if (self->extension == extension)
    g_clear_object (&self->extension);
}

/**
 * valent_plugin_set_extension: (set-property extension)
 * @plugin: a `ValentPlugin`
 * @extension: (nullable): a `ValentExtension`
 *
 * Set @extension as the [class@Valent.Extension] for @plugin.
 *
 * Since: 1.0
 */
void
valent_plugin_set_extension (ValentPlugin    *plugin,
                             ValentExtension *extension)
{
  g_return_if_fail (VALENT_IS_PLUGIN (plugin));
  g_return_if_fail (extension == NULL || VALENT_IS_EXTENSION (extension));

  if (plugin->extension == extension)
    return;

  if (plugin->extension != NULL)
    {
      g_signal_handlers_disconnect_by_func (plugin->extension,
                                            plugin,
                                            on_extension_destroy);
      valent_object_destroy (VALENT_OBJECT (plugin->extension));
      g_clear_object (&plugin->extension);
    }

  if (g_set_object (&plugin->extension, extension))
    {
      g_signal_connect_object (plugin->extension,
                               "destroy",
                               G_CALLBACK (on_extension_destroy),
                               plugin,
                               G_CONNECT_DEFAULT);
      g_object_notify_by_pspec (G_OBJECT (plugin), properties[PROP_EXTENSION]);
    }
}

/**
 * valent_plugin_get_plugin_info: (get-property plugin-info)
 * @plugin: a `ValentPlugin`
 *
 * Get the [class@Peas.PluginInfo] for @plugin.
 *
 * Returns: (transfer none): the plugin info
 *
 * Since: 1.0
 */
PeasPluginInfo *
valent_plugin_get_plugin_info (ValentPlugin *plugin)
{
  g_return_val_if_fail (VALENT_IS_PLUGIN (plugin), NULL);

  return plugin->plugin_info;
}

/**
 * valent_plugin_get_source: (get-property source)
 * @plugin: a `ValentPlugin`
 *
 * Get the [class@Valent.DataSource] for @plugin.
 *
 * Returns: (transfer none): the resource
 *
 * Since: 1.0
 */
ValentResource *
valent_plugin_get_source (ValentPlugin *plugin)
{
  g_return_val_if_fail (VALENT_IS_PLUGIN (plugin), NULL);

  return plugin->source;
}

