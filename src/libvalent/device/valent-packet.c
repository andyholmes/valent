// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-core"

#include "config.h"

#include <gio/gio.h>
#include <json-glib/json-glib.h>

#include "../core/valent-global.h"
#include "valent-packet.h"


G_DEFINE_QUARK (valent-packet-error, valent_packet_error)


/**
 * valent_packet_new:
 * @type: a KDE Connect packet type
 *
 * A convenience function for creating a new KDE Connect packet with the type
 * field set to @type.
 *
 * Returns: (transfer full): a KDE Connect packet
 *
 * Since: 1.0
 */
JsonNode *
valent_packet_new (const char *type)
{
  g_autoptr (JsonBuilder) builder = NULL;

  g_return_val_if_fail (type != NULL, NULL);

  builder = json_builder_new ();

  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "id");
  json_builder_add_int_value (builder, 0);
  json_builder_set_member_name (builder, "type");
  json_builder_add_string_value (builder, type);
  json_builder_set_member_name (builder, "body");
  json_builder_end_object (json_builder_begin_object (builder));
  json_builder_end_object (builder);

  return json_builder_get_root (builder);
}


/**
 * valent_packet_init: (skip)
 * @builder: a location to initialize a `JsonBuilder`
 * @type: a KDE Connect packet type
 *
 * Initialize a [class@Json.Builder] and KDE Connect packet.
 *
 * Creates a new [class@Json.Builder] and initializes a packet for @type,
 * leaving the builder in the `body` object. Call [func@Valent.packet_end]
 * to finish the packet and get the result.
 *
 * ```c
 * g_autoptr (JsonBuilder) builder = NULL;
 * g_autoptr (JsonNode) packet = NULL;
 *
 * valent_packet_init (&builder, "kdeconnect.ping");
 * json_builder_set_member_name (builder, "message");
 * json_builder_add_string_value (builder, "Ping!");
 * packet = valent_packet_end (&builder);
 * ```
 *
 *
 * Since: 1.0
 */
void
valent_packet_init (JsonBuilder **builder,
                    const char   *type)
{
  g_return_if_fail (builder != NULL && *builder == NULL);
  g_return_if_fail (type != NULL && *type != '\0');

  *builder = json_builder_new ();
  json_builder_begin_object (*builder);
  json_builder_set_member_name (*builder, "id");
  json_builder_add_int_value (*builder, 0);
  json_builder_set_member_name (*builder, "type");
  json_builder_add_string_value (*builder, type);
  json_builder_set_member_name (*builder, "body");

  json_builder_begin_object (*builder);
}

/**
 * valent_packet_end: (skip)
 * @builder: a pointer to a `JsonBuilder`
 *
 * Finish a packet created with [func@Valent.packet_init].
 *
 * This function closes the `body` and root objects, then calls
 * [method@Json.Builder.get_root]. Then the reference count of @builder is
 * decreased and the pointer is set to %NULL, before returning the packet.
 *
 * Returns: (transfer full) (nullable): a KDE Connect packet
 *
 * Since: 1.0
 */
JsonNode *
valent_packet_end (JsonBuilder **builder)
{
  JsonNode *ret = NULL;

  g_return_val_if_fail (builder != NULL && JSON_IS_BUILDER (*builder), NULL);

  /* Finish the `body` object and the root object */
  json_builder_end_object (*builder);
  json_builder_end_object (*builder);

  ret = json_builder_get_root (*builder);
  g_clear_object (builder);

  return g_steal_pointer (&ret);
}

/**
 * valent_packet_get_id:
 * @packet: a KDE Connect packet
 *
 * Convenience function for getting the timestamp of a KDE Connect packet.
 *
 * Returns: a UNIX epoch timestamp
 *
 * Since: 1.0
 */
int64_t
valent_packet_get_id (JsonNode *packet)
{
  JsonObject *root;
  JsonNode *node;

  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (packet), 0);

  root = json_node_get_object (packet);

  if G_UNLIKELY ((node = json_object_get_member (root, "id")) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_INT64)
    g_return_val_if_reached (0);

  return json_node_get_int (node);
}

/**
 * valent_packet_get_type:
 * @packet: a KDE Connect packet
 *
 * Convenience function for getting the capability type of a KDE Connect packet.
 *
 * Returns: (transfer none) (nullable): a KDE Connect capability
 *
 * Since: 1.0
 */
const char *
valent_packet_get_type (JsonNode *packet)
{
  JsonObject *root;
  JsonNode *node;

  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (packet), NULL);

  root = json_node_get_object (packet);

  if G_UNLIKELY ((node = json_object_get_member (root, "type")) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_STRING)
    g_return_val_if_reached (NULL);

  return json_node_get_string (node);
}

/**
 * valent_packet_get_body:
 * @packet: a KDE Connect packet
 *
 * Convenience function for getting the packet body of a KDE Connect packet.
 *
 * Returns: (transfer none) (nullable): a `JsonObject`
 *
 * Since: 1.0
 */
JsonObject *
valent_packet_get_body (JsonNode *packet)
{
  JsonObject *root;
  JsonNode *node;

  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (packet), NULL);

  root = json_node_get_object (packet);

  if G_UNLIKELY ((node = json_object_get_member (root, "body")) == NULL ||
                 json_node_get_node_type (node) != JSON_NODE_OBJECT)
    g_return_val_if_reached (NULL);

  return json_node_get_object (node);
}

/**
 * valent_packet_has_payload:
 * @packet: a KDE Connect packet
 *
 * Return %TRUE if the packet holds valid transfer information. Payload
 * information is considered invalid in the following cases:
 *
 * - The `payloadSize` field is present, but not a %G_TYPE_INT64
 * - The `payloadTransferInfo` field is missing from the root object
 * - The `payloadTransferInfo` field is not a %JSON_NODE_OBJECT
 *
 * Returns: %TRUE if @packet has a payload
 *
 * Since: 1.0
 */
gboolean
valent_packet_has_payload (JsonNode *packet)
{
  JsonObject *root;
  JsonNode *node;

  g_return_val_if_fail (VALENT_IS_PACKET (packet), FALSE);

  root = json_node_get_object (packet);

  if ((node = json_object_get_member (root, "payloadSize")) != NULL &&
      json_node_get_value_type (node) != G_TYPE_INT64)
    return FALSE;

  if ((node = json_object_get_member (root, "payloadTransferInfo")) == NULL ||
      json_node_get_node_type (node) != JSON_NODE_OBJECT)
    return FALSE;

  return TRUE;
}

/**
 * valent_packet_get_payload_full:
 * @packet: a KDE Connect packet
 * @size: (out) (nullable): the payload size
 * @error: (nullable): a `GError`
 *
 * A convenience for retrieving the `payloadTransferInfo` and `payloadSize`
 * fields from @packet.
 *
 * If @packet is malformed or missing payload information, %NULL will be
 * returned with @error set. See valent_packet_has_payload() for validation
 * criteria.
 *
 * Returns: (transfer none) (nullable): a `JsonObject`
 *
 * Since: 1.0
 */
JsonObject *
valent_packet_get_payload_full (JsonNode  *packet,
                                goffset   *size,
                                GError   **error)
{
  JsonObject *root;
  JsonNode *node;

  if (!valent_packet_validate (packet, error))
    return NULL;

  root = json_node_get_object (packet);

  /* The documentation implies that this field could be missing or have a value
   * of `-1` to indicate the length is indefinite (eg. for streaming). */
  if ((node = json_object_get_member (root, "payloadSize")) != NULL &&
      json_node_get_value_type (node) != G_TYPE_INT64)
    {
      g_set_error_literal (error,
                           VALENT_PACKET_ERROR,
                           VALENT_PACKET_ERROR_INVALID_FIELD,
                           "expected \"payloadSize\" field to hold an integer");
      return NULL;
    }

  if (size != NULL)
    *size = node ? json_node_get_int (node) : -1;

  if ((node = json_object_get_member (root, "payloadTransferInfo")) == NULL ||
      json_node_get_node_type (node) != JSON_NODE_OBJECT)
    {
      g_set_error_literal (error,
                           VALENT_PACKET_ERROR,
                           node == NULL
                             ? VALENT_PACKET_ERROR_MISSING_FIELD
                             : VALENT_PACKET_ERROR_INVALID_FIELD,
                           "expected \"payloadTransferInfo\" field holding an object");
      return NULL;
    }

  return json_node_get_object (node);
}

/**
 * valent_packet_set_payload_full:
 * @packet: a KDE Connect packet
 * @info: (transfer full): a `JsonObject`
 * @size: the payload size in bytes
 *
 * A convenience method for setting the `payloadTransferInfo` and `payloadSize`
 * fields on @packet.
 *
 * Since: 1.0
 */
void
valent_packet_set_payload_full (JsonNode   *packet,
                                JsonObject *info,
                                goffset     size)
{
  JsonObject *root;

  g_return_if_fail (VALENT_IS_PACKET (packet));

  root = json_node_get_object (packet);

  json_object_set_object_member (root, "payloadTransferInfo", info);
  json_object_set_int_member (root, "payloadSize", (int64_t)size);
}

/**
 * valent_packet_get_payload_info:
 * @packet: a KDE Connect packet
 *
 * A convenience for retrieve the 'payloadTransferInfo` field from @packet.
 *
 * Returns: (transfer none) (nullable): a `JsonObject`
 *
 * Since: 1.0
 */
JsonObject *
valent_packet_get_payload_info (JsonNode *packet)
{
  JsonNode *node;

  g_return_val_if_fail (VALENT_IS_PACKET (packet), NULL);

  node = json_object_get_member (json_node_get_object (packet),
                                 "payloadTransferInfo");

  if G_UNLIKELY (node == NULL || !JSON_NODE_HOLDS_OBJECT (node))
    g_return_val_if_reached (NULL);

  return json_node_get_object (node);
}

/**
 * valent_packet_set_payload_info:
 * @packet: a KDE Connect packet
 * @info: (transfer full): a `JsonObject`
 *
 * A convenience method for setting the `payloadTransferInfo` field on @packet.
 *
 * Since: 1.0
 */
void
valent_packet_set_payload_info (JsonNode   *packet,
                                JsonObject *info)
{
  JsonObject *root;

  g_return_if_fail (VALENT_IS_PACKET (packet));
  g_return_if_fail (info != NULL);

  root = json_node_get_object (packet);

  json_object_set_object_member (root, "payloadTransferInfo", info);
}

/**
 * valent_packet_get_payload_size:
 * @packet: a KDE Connect packet
 *
 * Get the `payloadSize` field of @packet in bytes.
 *
 * Returns: the payload size
 *
 * Since: 1.0
 */
goffset
valent_packet_get_payload_size (JsonNode *packet)
{
  JsonObject *root;
  JsonNode *node;

  g_return_val_if_fail (VALENT_IS_PACKET (packet), 0);

  root = json_node_get_object (packet);
  node = json_object_get_member (root, "payloadSize");

  if ((node = json_object_get_member (root, "payloadSize")) != NULL &&
      json_node_get_value_type (node) != G_TYPE_INT64)
    g_return_val_if_reached (-1);

  return node ? json_node_get_int (node) : -1;
}

/**
 * valent_packet_set_payload_size:
 * @packet: a KDE Connect packet
 * @size: the payload size in bytes
 *
 * Set the `payloadSize` field of @packet to @size.
 *
 * Since: 1.0
 */
void
valent_packet_set_payload_size (JsonNode *packet,
                                goffset   size)
{
  JsonObject *root;

  g_return_if_fail (VALENT_IS_PACKET (packet));
  g_return_if_fail (size >= -1);

  root = json_node_get_object (packet);

  json_object_set_int_member (root, "payloadSize", (int64_t)size);
}

/**
 * valent_packet_check_field:
 * @packet: a KDE Connect packet
 * @field: (not nullable): field name
 *
 * Check @packet for @field and return %TRUE if present, with two exceptions:
 *
 * 1. If @field is a %G_TYPE_BOOLEAN, its value is returned
 * 2. If @field is a %G_TYPE_STRING, %FALSE is returned if the string is empty.
 *
 * Returns: %TRUE, or %FALSE on failure
 *
 * Since: 1.0
 */
gboolean
valent_packet_check_field (JsonNode   *packet,
                           const char *field)
{
  JsonObject *root;
  JsonObject *body;
  JsonNode *node;

  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (packet), FALSE);
  g_return_val_if_fail (field != NULL && *field != '\0', FALSE);

  root = json_node_get_object (packet);

  if G_UNLIKELY ((node = json_object_get_member (root, "body")) == NULL ||
                 json_node_get_node_type (node) != JSON_NODE_OBJECT)
    return FALSE;

  body = json_node_get_object (node);

  if G_UNLIKELY ((node = json_object_get_member (body, field)) == NULL)
    return FALSE;

  if (json_node_get_value_type (node) == G_TYPE_BOOLEAN)
    return json_node_get_boolean (node);

  if (json_node_get_value_type (node) == G_TYPE_STRING)
    return json_node_get_string (node)[0] != '\0';

  return TRUE;
}

/**
 * valent_packet_get_boolean:
 * @packet: a KDE Connect packet
 * @field: (not nullable): field name
 * @value: (out) (nullable): a boolean
 *
 * Lookup @field in the body of @packet and assign it to @value.
 *
 * If @field is not found or it is not a boolean, %FALSE will be returned and
 * @value will not be set.
 *
 * Returns: %TRUE, or %FALSE on failure
 *
 * Since: 1.0
 */
gboolean
valent_packet_get_boolean (JsonNode   *packet,
                           const char *field,
                           gboolean   *value)
{
  JsonObject *root, *body;
  JsonNode *node;

  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (packet), FALSE);
  g_return_val_if_fail (field != NULL && *field != '\0', FALSE);

  root = json_node_get_object (packet);

  if G_UNLIKELY ((node = json_object_get_member (root, "body")) == NULL ||
                 json_node_get_node_type (node) != JSON_NODE_OBJECT)
    return FALSE;

  body = json_node_get_object (node);

  if G_UNLIKELY ((node = json_object_get_member (body, field)) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_BOOLEAN)
    return FALSE;

  if (value)
    *value = json_node_get_boolean (node);

  return TRUE;
}

/**
 * valent_packet_get_double:
 * @packet: a KDE Connect packet
 * @field: (not nullable): field name
 * @value: (out) (nullable): a double
 *
 * Lookup @field in the body of @packet and assign it to @value.
 *
 * If @field is not found or it is not a double, %FALSE will be returned and
 * @value will not be set.
 *
 * Returns: %TRUE, or %FALSE on failure
 *
 * Since: 1.0
 */
gboolean
valent_packet_get_double (JsonNode   *packet,
                          const char *field,
                          double     *value)
{
  JsonObject *root, *body;
  JsonNode *node;

  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (packet), FALSE);
  g_return_val_if_fail (field != NULL && *field != '\0', FALSE);

  root = json_node_get_object (packet);

  if G_UNLIKELY ((node = json_object_get_member (root, "body")) == NULL ||
                 json_node_get_node_type (node) != JSON_NODE_OBJECT)
    return FALSE;

  body = json_node_get_object (node);

  if G_UNLIKELY ((node = json_object_get_member (body, field)) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_DOUBLE)
    return FALSE;

  if (value)
    *value = json_node_get_double (node);

  return TRUE;
}

/**
 * valent_packet_get_int:
 * @packet: a KDE Connect packet
 * @field: (not nullable): field name
 * @value: (out) (nullable): an int64
 *
 * Lookup @field in the body of @packet and assign it to @value.
 *
 * If @field is not found or it is not an integer, %FALSE will be returned and
 * @value will not be set.
 *
 * Returns: %TRUE, or %FALSE on failure
 *
 * Since: 1.0
 */
gboolean
valent_packet_get_int (JsonNode   *packet,
                       const char *field,
                       int64_t    *value)
{
  JsonObject *root, *body;
  JsonNode *node;

  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (packet), FALSE);
  g_return_val_if_fail (field != NULL && *field != '\0', FALSE);

  root = json_node_get_object (packet);

  if G_UNLIKELY ((node = json_object_get_member (root, "body")) == NULL ||
                 json_node_get_node_type (node) != JSON_NODE_OBJECT)
    return FALSE;

  body = json_node_get_object (node);

  if G_UNLIKELY ((node = json_object_get_member (body, field)) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_INT64)
    return FALSE;

  if (value)
    *value = json_node_get_int (node);

  return TRUE;
}

/**
 * valent_packet_get_string:
 * @packet: a KDE Connect packet
 * @field: (not nullable): field name
 * @value: (out) (nullable): a string
 *
 * Lookup @field in the body of @packet and assign it to @value.
 *
 * If @field is not found or it is not a non-empty string, %FALSE will be
 * returned and @value will not be set.
 *
 * Returns: %TRUE, or %FALSE on failure
 *
 * Since: 1.0
 */
gboolean
valent_packet_get_string (JsonNode    *packet,
                          const char  *field,
                          const char **value)
{
  JsonObject *root, *body;
  JsonNode *node;
  const char *string;

  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (packet), FALSE);
  g_return_val_if_fail (field != NULL && *field != '\0', FALSE);

  root = json_node_get_object (packet);

  if G_UNLIKELY ((node = json_object_get_member (root, "body")) == NULL ||
                 json_node_get_node_type (node) != JSON_NODE_OBJECT)
    return FALSE;

  body = json_node_get_object (node);

  if G_UNLIKELY ((node = json_object_get_member (body, field)) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_STRING)
    return FALSE;

  string = json_node_get_string (node);

  if G_UNLIKELY (*string == '\0')
    return FALSE;

  if (value)
    *value = string;

  return TRUE;
}

/**
 * valent_packet_get_array:
 * @packet: a KDE Connect packet
 * @field: (not nullable): field name
 * @value: (out) (nullable): a `JsonArray`
 *
 * Lookup @field in the body of @packet and assign it to @value.
 *
 * If @field is not found or it is not a `JsonArray`, %FALSE will be returned and
 * @value will not be set.
 *
 * Returns: %TRUE, or %FALSE on failure
 *
 * Since: 1.0
 */
gboolean
valent_packet_get_array (JsonNode    *packet,
                         const char  *field,
                         JsonArray  **value)
{
  JsonObject *root, *body;
  JsonNode *node;

  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (packet), FALSE);
  g_return_val_if_fail (field != NULL && *field != '\0', FALSE);

  root = json_node_get_object (packet);

  if G_UNLIKELY ((node = json_object_get_member (root, "body")) == NULL ||
                 json_node_get_node_type (node) != JSON_NODE_OBJECT)
    return FALSE;

  body = json_node_get_object (node);

  if G_UNLIKELY ((node = json_object_get_member (body, field)) == NULL ||
                 json_node_get_node_type (node) != JSON_NODE_ARRAY)
    return FALSE;

  if (value)
    *value = json_node_get_array (node);

  return TRUE;
}

/**
 * valent_packet_get_object:
 * @packet: a KDE Connect packet
 * @field: (not nullable): field name
 * @value: (out) (nullable): a `JsonObject`
 *
 * Lookup @field in the body of @packet and assign it to @value.
 *
 * If @field is not found or it is not a `JsonObject`, %FALSE will be returned
 * and @value will not be set.
 *
 * Returns: %TRUE, or %FALSE on failure
 *
 * Since: 1.0
 */
gboolean
valent_packet_get_object (JsonNode    *packet,
                          const char  *field,
                          JsonObject **value)
{
  JsonObject *root, *body;
  JsonNode *node;

  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (packet), FALSE);
  g_return_val_if_fail (field != NULL && *field != '\0', FALSE);

  root = json_node_get_object (packet);

  if G_UNLIKELY ((node = json_object_get_member (root, "body")) == NULL ||
                 json_node_get_node_type (node) != JSON_NODE_OBJECT)
    return FALSE;

  body = json_node_get_object (node);

  if G_UNLIKELY ((node = json_object_get_member (body, field)) == NULL ||
                 json_node_get_node_type (node) != JSON_NODE_OBJECT)
    return FALSE;

  if (value)
    *value = json_node_get_object (node);

  return TRUE;
}

/**
 * valent_packet_dup_strv:
 * @packet: a KDE Connect packet
 * @field: (not nullable): field name
 *
 * Lookup @field in the body of @packet and return a newly allocated list of
 * strings.
 *
 * If @field is not found, it is not a `JsonArray` or any of its elements are not
 * strings, %NULL will be returned.
 *
 * Returns: (transfer full) (nullable) (array zero-terminated=1): a list of strings
 *
 * Since: 1.0
 */
GStrv
valent_packet_dup_strv (JsonNode   *packet,
                        const char *field)
{
  JsonObject *root, *body;
  JsonNode *node;
  JsonArray *array;
  g_auto (GStrv) strv = NULL;
  unsigned int n_strings;

  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (packet), NULL);
  g_return_val_if_fail (field != NULL && *field != '\0', NULL);

#ifndef __clang_analyzer__
  root = json_node_get_object (packet);

  if G_UNLIKELY ((node = json_object_get_member (root, "body")) == NULL ||
                 json_node_get_node_type (node) != JSON_NODE_OBJECT)
    return NULL;

  body = json_node_get_object (node);

  if G_UNLIKELY ((node = json_object_get_member (body, field)) == NULL ||
                 json_node_get_node_type (node) != JSON_NODE_ARRAY)
    return NULL;

  array = json_node_get_array (node);
  n_strings = json_array_get_length (array);
  strv = g_new0 (char *, n_strings + 1);

  for (unsigned int i = 0; i < n_strings; i++)
    {
      JsonNode *element = json_array_get_element (array, i);

      if G_UNLIKELY (json_node_get_value_type (element) != G_TYPE_STRING)
        return NULL;

      strv[i] = json_node_dup_string (element);
    }
#endif /* __clang_analyzer__ */

  return g_steal_pointer (&strv);
}

/**
 * valent_packet_validate:
 * @packet: (nullable): a KDE Connect packet
 * @error: (nullable): a `GError`
 *
 * Check if @packet is a well-formed KDE Connect packet.
 *
 * Returns: %TRUE if @packet is valid, or %FALSE with @error set
 *
 * Since: 1.0
 */
gboolean
valent_packet_validate (JsonNode  *packet,
                        GError   **error)
{
  JsonObject *root;
  JsonNode *node;

  if G_UNLIKELY (packet == NULL)
    {
      g_set_error_literal (error,
                           VALENT_PACKET_ERROR,
                           VALENT_PACKET_ERROR_INVALID_DATA,
                           "packet is NULL");
      return FALSE;
    }

  if G_UNLIKELY (!JSON_NODE_HOLDS_OBJECT (packet))
    {
      g_set_error_literal (error,
                           VALENT_PACKET_ERROR,
                           VALENT_PACKET_ERROR_MALFORMED,
                           "expected the root element to be an object");
      return FALSE;
    }

  root = json_node_get_object (packet);

  if G_UNLIKELY ((node = json_object_get_member (root, "type")) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_STRING)
    {
      g_set_error_literal (error,
                           VALENT_PACKET_ERROR,
                           node == NULL
                             ? VALENT_PACKET_ERROR_MISSING_FIELD
                             : VALENT_PACKET_ERROR_INVALID_FIELD,
                           "expected \"type\" field holding a string");
      return FALSE;
    }

  if G_UNLIKELY ((node = json_object_get_member (root, "body")) == NULL ||
                 json_node_get_node_type (node) != JSON_NODE_OBJECT)
    {
      g_set_error_literal (error,
                           VALENT_PACKET_ERROR,
                           node == NULL
                             ? VALENT_PACKET_ERROR_MISSING_FIELD
                             : VALENT_PACKET_ERROR_INVALID_FIELD,
                           "expected \"body\" field holding an object");
      return FALSE;
    }

  /* These two are optional, but have defined value types */
  if G_UNLIKELY ((node = json_object_get_member (root, "payloadSize")) != NULL &&
                 json_node_get_value_type (node) != G_TYPE_INT64)
    {
      g_set_error_literal (error,
                           VALENT_PACKET_ERROR,
                           VALENT_PACKET_ERROR_INVALID_FIELD,
                           "expected \"payloadSize\" field to hold an integer");
      return FALSE;
    }

  if G_UNLIKELY ((node = json_object_get_member (root, "payloadTransferInfo")) != NULL &&
                 json_node_get_node_type (node) != JSON_NODE_OBJECT)
    {
      g_set_error_literal (error,
                           VALENT_PACKET_ERROR,
                           VALENT_PACKET_ERROR_INVALID_FIELD,
                           "expected \"payloadTransferInfo\" field to hold an object");
      return FALSE;
    }

  return TRUE;
}

/**
 * valent_packet_from_stream:
 * @stream: a `GInputStream`
 * @max_len: the maximum number bytes to read, or `-1` for no limit
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Read a KDE Connect packet from an input stream.
 *
 * If reading fails or the packet does not conform to the minimum structure of
 * a KDE Connect packet, %NULL will be returned with @error set.
 *
 * If @max_len is greater than `-1`, then at most @max_len bytes will be read.
 * If @max_len bytes are read without encountering a line-feed character, %NULL
 * will be returned with @error set to %G_IO_ERROR_MESSAGE_TOO_LARGE.
 *
 * Returns: (transfer full): a KDE Connect packet, or %NULL with @error set.
 *
 * Since: 1.0
 */
JsonNode *
valent_packet_from_stream (GInputStream  *stream,
                           gssize         max_len,
                           GCancellable  *cancellable,
                           GError       **error)
{
  g_autoptr (JsonParser) parser = NULL;
  g_autoptr (JsonNode) packet = NULL;
  g_autofree char *line = NULL;
  gssize count = 0;
  gssize size = 4096;

  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

#ifndef __clang_analyzer__
  if (max_len < 0)
    max_len = G_MAXSSIZE;

  line = g_malloc0 (size);

  while (TRUE)
    {
      gssize read = 0;

      if G_UNLIKELY (count == max_len)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_MESSAGE_TOO_LARGE,
                       "Packet too large");
          return NULL;
        }

      if G_UNLIKELY (count == size)
        {
          size = MIN (size * 2, max_len);
          line = g_realloc (line, size);
        }

      read = g_input_stream_read (stream,
                                  line + count,
                                  1,
                                  cancellable,
                                  error);

      if (read > 0)
        count += read;
      else if (read == 0)
        break;
      else
        return NULL;

      if G_UNLIKELY (line[count - 1] == '\n')
        break;
    }

  parser = json_parser_new_immutable ();

  if (!json_parser_load_from_data (parser, line, count, error))
    return NULL;

  packet = json_parser_steal_root (parser);

  if (!valent_packet_validate (packet, error))
    return NULL;
#endif /* __clang_analyzer__ */

  return g_steal_pointer (&packet);
}

/**
 * valent_packet_to_stream:
 * @stream: a `GOutputStream`
 * @packet: a KDE Connect packet
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * A convenience function for writing a packet to a connection.
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 *
 * Since: 1.0
 */
gboolean
valent_packet_to_stream (GOutputStream  *stream,
                         JsonNode       *packet,
                         GCancellable   *cancellable,
                         GError        **error)
{
  g_autoptr (JsonGenerator) generator = NULL;
  JsonObject *root;
  g_autofree char *packet_str = NULL;
  size_t packet_len;
  size_t n_written;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!valent_packet_validate (packet, error))
    return FALSE;

  /* Timestamp the packet (UNIX Epoch ms) */
  root = json_node_get_object (packet);
  json_object_set_int_member (root, "id", valent_timestamp_ms ());

  /* Serialize the packet and replace the trailing NULL with an LF */
  generator = json_generator_new ();
  json_generator_set_root (generator, packet);
  packet_str = json_generator_to_data (generator, &packet_len);
  packet_str[packet_len++] = '\n';

  if (!g_output_stream_write_all (stream,
                                  packet_str,
                                  packet_len,
                                  &n_written,
                                  cancellable,
                                  error))
    return FALSE;

  if (n_written != packet_len)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_CONNECTION_CLOSED,
                   "Channel is closed");
      return FALSE;
    }

  return TRUE;
}

/**
 * valent_packet_serialize:
 * @packet: a complete KDE Connect packet
 *
 * Convenience function that updates the timestamp of a packet before returning
 * a serialized string with newline ending, ready to be written to a stream.
 *
 * Returns: (transfer full) (nullable): the serialized packet.
 *
 * Since: 1.0
 */
char *
valent_packet_serialize (JsonNode *packet)
{
  g_autoptr (JsonGenerator) generator = NULL;
  JsonObject *root;
  g_autofree char *packet_str = NULL;

  g_return_val_if_fail (VALENT_IS_PACKET (packet), NULL);

  /* Timestamp the packet (UNIX Epoch ms) */
  root = json_node_get_object (packet);
  json_object_set_int_member (root, "id", valent_timestamp_ms ());

  /* Stringify the packet and return a newline-terminated string */
  generator = json_generator_new ();
  json_generator_set_root (generator, packet);
  packet_str = json_generator_to_data (generator, NULL);

  return g_strconcat (packet_str, "\n", NULL);
}

/**
 * valent_packet_deserialize:
 * @json: a complete KDE Connect packet
 * @error: (nullable): a `GError`
 *
 * Convenience function that deserializes a KDE Connect packet from a string
 * with basic validation. If @str is empty, this function will return %NULL.
 *
 * If parsing or validation fails, @error will be set and %NULL returned.
 *
 * Returns: (transfer full) (nullable): a KDE Connect packet
 *
 * Since: 1.0
 */
JsonNode *
valent_packet_deserialize (const char  *json,
                           GError     **error)
{
  g_autoptr (JsonParser) parser = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_return_val_if_fail (json != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  parser = json_parser_new_immutable ();

  if (!json_parser_load_from_data (parser, json, -1, error))
    return NULL;

  if ((packet = json_parser_steal_root (parser)) == NULL)
    return NULL;

  if (!valent_packet_validate (packet, error))
    return NULL;

  return g_steal_pointer (&packet);
}

