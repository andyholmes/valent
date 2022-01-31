// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include <gio/gio.h>
#include <json-glib/json-glib.h>

#include "valent-version.h"

G_BEGIN_DECLS


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
  VALENT_PACKET_ERROR_INVALID_DATA,
  VALENT_PACKET_ERROR_MALFORMED,
  VALENT_PACKET_ERROR_INVALID_FIELD,
  VALENT_PACKET_ERROR_MISSING_FIELD,
} ValentPacketError;

VALENT_AVAILABLE_IN_1_0
GQuark   valent_packet_error_quark (void);
#define VALENT_PACKET_ERROR (valent_packet_error_quark ())


/**
 * valent_packet_is_valid:
 * @packet: (nullable): a #JsonNode
 *
 * Check if @packet is a well-formed KDE Connect packet. This can be used in
 * g_return_if_fail() checks.
 *
 * Returns: %TRUE if @packet is valid, or %FALSE if not
 */
static inline gboolean
valent_packet_is_valid (JsonNode *packet)
{
  JsonObject *root;
  JsonNode *node;

  if G_UNLIKELY (packet == NULL || !JSON_NODE_HOLDS_OBJECT (packet))
    return FALSE;

  root = json_node_get_object (packet);

  /* TODO: kdeconnect-kde stringifies this in identity packets
   *       https://invent.kde.org/network/kdeconnect-kde/-/merge_requests/380 */
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

  /* These two are optional, but have defined value types */
  if G_UNLIKELY ((node = json_object_get_member (root, "payloadSize")) != NULL &&
                 json_node_get_value_type (node) != G_TYPE_INT64)
    return FALSE;

  if G_UNLIKELY ((node = json_object_get_member (root, "payloadTransferInfo")) != NULL &&
                 json_node_get_node_type (node) != JSON_NODE_OBJECT)
    return FALSE;

  return TRUE;
}
#define VALENT_IS_PACKET(packet) (valent_packet_is_valid (packet))


/* Packet Helpers */
VALENT_AVAILABLE_IN_1_0
JsonNode    * valent_packet_new              (const char     *type);
VALENT_AVAILABLE_IN_1_0
JsonBuilder * valent_packet_start            (const char     *type);
VALENT_AVAILABLE_IN_1_0
JsonNode    * valent_packet_finish           (JsonBuilder    *builder);
VALENT_AVAILABLE_IN_1_0
gint64        valent_packet_get_id           (JsonNode       *packet);
VALENT_AVAILABLE_IN_1_0
const char  * valent_packet_get_type         (JsonNode       *packet);
VALENT_AVAILABLE_IN_1_0
JsonObject  * valent_packet_get_body         (JsonNode       *packet);
VALENT_AVAILABLE_IN_1_0
gboolean      valent_packet_has_payload      (JsonNode       *packet);
VALENT_AVAILABLE_IN_1_0
JsonObject  * valent_packet_get_payload_full (JsonNode       *packet,
                                              gssize         *size,
                                              GError        **error);
VALENT_AVAILABLE_IN_1_0
void          valent_packet_set_payload_full (JsonNode       *packet,
                                              JsonObject     *info,
                                              gssize          size);
VALENT_AVAILABLE_IN_1_0
JsonObject  * valent_packet_get_payload_info (JsonNode       *packet);
VALENT_AVAILABLE_IN_1_0
void          valent_packet_set_payload_info (JsonNode       *packet,
                                              JsonObject     *info);
VALENT_AVAILABLE_IN_1_0
gssize        valent_packet_get_payload_size (JsonNode       *packet);
VALENT_AVAILABLE_IN_1_0
void          valent_packet_set_payload_size (JsonNode       *packet,
                                              gssize          size);

/* I/O Helpers */
VALENT_AVAILABLE_IN_1_0
gboolean      valent_packet_validate         (JsonNode       *packet,
                                              GError        **error);
VALENT_AVAILABLE_IN_1_0
JsonNode    * valent_packet_from_stream      (GInputStream   *stream,
                                              GCancellable   *cancellable,
                                              GError        **error);
VALENT_AVAILABLE_IN_1_0
gboolean      valent_packet_to_stream        (GOutputStream  *stream,
                                              JsonNode       *packet,
                                              GCancellable   *cancellable,
                                              GError        **error);
VALENT_AVAILABLE_IN_1_0
char        * valent_packet_serialize        (JsonNode       *packet);
VALENT_AVAILABLE_IN_1_0
JsonNode    * valent_packet_deserialize      (const char     *json,
                                              GError        **error);

/* Identity Packets */
VALENT_AVAILABLE_IN_1_0
const char  * valent_identity_get_device_id  (JsonNode       *identity);

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
