// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include <gio/gio.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define VALENT_IS_PACKET(packet) (valent_packet_is_valid (packet))
#define VALENT_PACKET_ERROR      (valent_packet_error_quark ())


/**
 * ValentPacketError:
 * @VALENT_PACKET_ERROR_UNKNOWN: unknown error
 * @VALENT_PACKET_ERROR_MALFORMED: a malformed packet
 * @VALENT_PACKET_ERROR_INVALID_FIELD: an expected field is missing or holds an invalid type
 * @VALENT_PACKET_ERROR_UNKNOWN_MEMBER: the `id` field is missing
 * @VALENT_PACKET_ERROR_INVALID_DATA: invalid data
 *
 * Error enumeration for KDE Connect packet validation.
 *
 * This enumeration can be extended at later date
 */
typedef enum {
  VALENT_PACKET_ERROR_UNKNOWN,
  VALENT_PACKET_ERROR_MALFORMED,
  VALENT_PACKET_ERROR_INVALID_FIELD,
  VALENT_PACKET_ERROR_UNKNOWN_MEMBER,
  VALENT_PACKET_ERROR_INVALID_DATA,
} ValentPacketError;


GQuark        valent_packet_error_quark      (void);

/* Builder Helpers */
JsonBuilder * valent_packet_start            (const char     *type);
JsonNode    * valent_packet_finish           (JsonBuilder    *builder);

/* I/O Helpers */
JsonNode    * valent_packet_from_stream      (GInputStream   *stream,
                                              GCancellable   *cancellable,
                                              GError        **error);
gboolean      valent_packet_to_stream        (GOutputStream  *stream,
                                              JsonNode       *packet,
                                              GCancellable   *cancellable,
                                              GError        **error);
char        * valent_packet_serialize        (JsonNode       *packet);
JsonNode    * valent_packet_deserialize      (const char     *json,
                                              GError        **error);

/* Convenience getters */
gint64        valent_packet_get_id           (JsonNode       *packet);
const char  * valent_packet_get_type         (JsonNode       *packet);
JsonObject  * valent_packet_get_body         (JsonNode       *packet);

/* Payloads */
gboolean      valent_packet_has_payload      (JsonNode       *packet);
JsonObject  * valent_packet_get_payload_full (JsonNode       *packet,
                                              gssize         *size,
                                              GError        **error);
void          valent_packet_set_payload_full (JsonNode       *packet,
                                              JsonObject     *info,
                                              gssize          size);
JsonObject  * valent_packet_get_payload_info (JsonNode       *packet);
void          valent_packet_set_payload_info (JsonNode       *packet,
                                              JsonObject     *info);
gssize        valent_packet_get_payload_size (JsonNode       *packet);
void          valent_packet_set_payload_size (JsonNode       *packet,
                                              gssize          size);

/* Identity Packets */
const char  * valent_identity_get_device_id  (JsonNode       *identity);


/**
 * valent_packet_is_valid:
 * @packet: (nullable): a #JsonNode
 *
 * Returns %TRUE if @packet is a valid packet. This can be used in
 * `g_return_if_fail()` checks.
 *
 * Returns: %TRUE of %FALSE
 */
static inline gboolean
valent_packet_is_valid (JsonNode *packet)
{
  JsonObject *root;
  JsonNode *node;

  if G_UNLIKELY (packet == NULL || !JSON_NODE_HOLDS_OBJECT (packet))
    return FALSE;

  root = json_node_get_object (packet);

  /* FIXME: kdeconnect-kde stringifies this in identity packets */
  if G_UNLIKELY ((node = json_object_get_member (root, "id")) == NULL ||
                 (json_node_get_value_type (node) != G_TYPE_INT64 &&
                  json_node_get_value_type (node) != G_TYPE_STRING))
    return FALSE;

  if G_UNLIKELY ((node = json_object_get_member (root, "type")) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_STRING)
    return FALSE;

  if G_UNLIKELY ((node = json_object_get_member (root, "body")) == NULL ||
                 json_node_get_node_type (node) != JSON_NODE_OBJECT)
    return FALSE;

  return TRUE;
}

/**
 * valent_packet_validate:
 * @packet: (nullable): a #JsonNode
 * @error: (nullable): a #GError
 *
 * Returns %TRUE if @packet is a valid packet like valent_packet_is_valid(), but
 * sets @error if returning %FALSE.
 *
 * Returns: %TRUE or %FALSE with @error set
 */
static inline gboolean
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
                           "Packet is null");
      return FALSE;
    }

  if G_UNLIKELY (!JSON_NODE_HOLDS_OBJECT (packet))
    {
      g_set_error_literal (error,
                           VALENT_PACKET_ERROR,
                           VALENT_PACKET_ERROR_MALFORMED,
                           "Root element is not an object");
      return FALSE;
    }

  root = json_node_get_object (packet);

  /* FIXME: kdeconnect-kde stringifies this in identity packets */
  if G_UNLIKELY ((node = json_object_get_member (root, "id")) == NULL ||
                 (json_node_get_value_type (node) != G_TYPE_INT64 &&
                  json_node_get_value_type (node) != G_TYPE_STRING))
    {
      g_set_error_literal (error,
                           VALENT_PACKET_ERROR,
                           VALENT_PACKET_ERROR_MALFORMED,
                           "Expected `id` field");
      return FALSE;
    }

  if G_UNLIKELY ((node = json_object_get_member (root, "type")) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_STRING)
    {
      g_set_error_literal (error,
                           VALENT_PACKET_ERROR,
                           VALENT_PACKET_ERROR_MALFORMED,
                           "Expected `type` field");
      return FALSE;
    }

  if G_UNLIKELY ((node = json_object_get_member (root, "body")) == NULL ||
                 json_node_get_node_type (node) != JSON_NODE_OBJECT)
    {
      g_set_error_literal (error,
                           VALENT_PACKET_ERROR,
                           VALENT_PACKET_ERROR_MALFORMED,
                           "Expected `body` object");
      return FALSE;
    }

  return TRUE;
}

/**
 * valent_packet_check_boolean: (skip)
 * @body: (type Json.Object): a packet body
 * @member: a member name
 *
 * A quick and silent macro for getting a boolean member from a packet body. If
 * the member is missing or holds another type %FALSE is returned.
 *
 * Returns: a boolean
 */
static inline gboolean
(valent_packet_check_boolean) (JsonObject *body,
                               const char *member)
{
  JsonNode *node;

  if G_UNLIKELY ((node = json_object_get_member (body, member)) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_BOOLEAN)
    return FALSE;

  return json_node_get_boolean (node);
}
#define valent_packet_check_boolean(o,m) (valent_packet_check_boolean(o,m))

/**
 * valent_packet_check_double: (skip)
 * @body: (type Json.Object): a packet body
 * @member: a member name
 *
 * A quick and silent macro for getting a double member from a packet body. If
 * the member is missing or holds another type `0.0` is returned.
 *
 * Returns: a double
 */
static inline double
(valent_packet_check_double) (JsonObject *body,
                              const char *member)
{
  JsonNode *node;

  if G_UNLIKELY ((node = json_object_get_member (body, member)) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_DOUBLE)
    return 0.0;

  return json_node_get_double (node);
}
#define valent_packet_check_double(o,m) (valent_packet_check_double(o,m))

/**
 * valent_packet_check_int: (skip)
 * @body: (type Json.Object): a packet body
 * @member: a member name
 *
 * A quick and silent macro for getting an integer member from a packet body. If
 * the member is missing or holds another type `0` is returned.
 *
 * Returns: an integer
 */
static inline gint64
(valent_packet_check_int) (JsonObject *body,
                           const char *member)
{
  JsonNode *node;

  if G_UNLIKELY ((node = json_object_get_member (body, member)) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_INT64)
    return 0;

  return json_node_get_int (node);
}
#define valent_packet_check_int(o,m) (valent_packet_check_int(o,m))

/**
 * valent_packet_check_string: (skip)
 * @body: (type Json.Object): a packet body
 * @member: a member name
 *
 * A quick and silent macro for getting a string member from a packet body. If
 * the member is missing, holds another type or an empty string %NULL is
 * returned.
 *
 * Returns: (nullable): a string
 */
static inline const char *
(valent_packet_check_string) (JsonObject *body,
                              const char *member)
{
  JsonNode *node;
  const char *value;

  if G_UNLIKELY ((node = json_object_get_member (body, member)) == NULL ||
                 json_node_get_value_type (node) != G_TYPE_STRING)
    return NULL;

  value = json_node_get_string (node);

  if G_UNLIKELY (strlen (value) == 0)
    return NULL;

  return value;
}
#define valent_packet_check_string(o,m) (valent_packet_check_string(o,m))

G_END_DECLS
