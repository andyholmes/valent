// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-eds-utils"

#include "config.h"

#include <valent.h>

#include "valent-eds-utils.h"


static ESourceRegistry *default_registry = NULL;

/**
 * valent_eds_get_registry:
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Get the global #ESourceRegistry object.
 *
 * Returns: (transfer none): the #ESourceRegistry
 */
ESourceRegistry *
valent_eds_get_registry (GCancellable  *cancellable,
                         GError       **error)
{
  if (default_registry == NULL)
    {
      default_registry = e_source_registry_new_sync (cancellable, error);

      if (default_registry != NULL)
        g_object_add_weak_pointer (G_OBJECT (default_registry), (gpointer) &default_registry);
      else
        return NULL;
    }

  return default_registry;
}

/**
 * valent_eds_register_source:
 * @source: an #ESource
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Register @scratch with the #ESourceRegistry. If a source with the same UID as
 * @scratch is already registered, that source will be returned. If the registry
 * is unavailable or registration fails a new reference of @scratch will be
 * returned.
 *
 * Returns: (transfer full): an #ESource
 */
ESource *
valent_eds_register_source (ESource       *scratch,
                            GCancellable  *cancellable,
                            GError       **error)
{
  ESourceRegistry *registry;
  g_autoptr (ESource) source = NULL;

  g_return_val_if_fail (E_IS_SOURCE (scratch), NULL);

  /* Get the source to the registry */
  registry = valent_eds_get_registry (cancellable, error);

  if (registry == NULL)
    return g_object_ref (scratch);

  /* Check the registry for an existing source */
  source = e_source_registry_ref_source (registry, e_source_get_uid (scratch));

  if (source)
    return g_steal_pointer (&source);

  /* Commit the scratch source to the registry */
  if (!e_source_registry_commit_source_sync (registry, scratch, cancellable, error))
    return NULL;

      /* if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS)) */
      /*   return e_source_registry_ref_source (registry, uid); */
      /* else */
      /*   g_warning ("Failed to register ESource: %s", error->message); */

  return g_object_ref (scratch);
}

