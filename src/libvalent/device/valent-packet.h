// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <json-glib/json-glib.h>

#include "../core/valent-object.h"

G_BEGIN_DECLS


/**
 * ValentPacketError:
 * @VALENT_PACKET_ERROR_UNKNOWN: an unknown error
 * @VALENT_PACKET_ERROR_INVALID_DATA: the packet is %NULL or not JSON
 * @VALENT_PACKET_ERROR_MALFORMED: the packet structure is malformed
 * @VALENT_PACKET_ERROR_INVALID_FIELD: an expected field holds an invalid type
 * @VALENT_PACKET_ERROR_MISSING_FIELD: an expected field is missing
 *
 * Error enumeration for KDE Connect packet validation.
 *
 * This enumeration can be extended at later date
 *
 * Since: 1.0
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
void          valent_packet_init             (JsonBuilder   **builder,
                                              const char     *type);
VALENT_AVAILABLE_IN_1_0
JsonNode    * valent_packet_end              (JsonBuilder   **builder);
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
                                              goffset        *size,
                                              GError        **error);
VALENT_AVAILABLE_IN_1_0
void          valent_packet_set_payload_full (JsonNode       *packet,
                                              JsonObject     *info,
                                              goffset         size);
VALENT_AVAILABLE_IN_1_0
JsonObject  * valent_packet_get_payload_info (JsonNode       *packet);
VALENT_AVAILABLE_IN_1_0
void          valent_packet_set_payload_info (JsonNode       *packet,
                                              JsonObject     *info);
VALENT_AVAILABLE_IN_1_0
goffset       valent_packet_get_payload_size (JsonNode       *packet);
VALENT_AVAILABLE_IN_1_0
void          valent_packet_set_payload_size (JsonNode       *packet,
                                              goffset         size);

/* Field Helpers */
VALENT_AVAILABLE_IN_1_0
gboolean      valent_packet_check_field      (JsonNode       *packet,
                                              const char     *field);
VALENT_AVAILABLE_IN_1_0
gboolean      valent_packet_get_boolean      (JsonNode       *packet,
                                              const char     *field,
                                              gboolean       *value);
VALENT_AVAILABLE_IN_1_0
gboolean      valent_packet_get_double       (JsonNode       *packet,
                                              const char     *field,
                                              double         *value);
VALENT_AVAILABLE_IN_1_0
gboolean      valent_packet_get_int          (JsonNode       *packet,
                                              const char     *field,
                                              gint64         *value);
VALENT_AVAILABLE_IN_1_0
gboolean      valent_packet_get_string       (JsonNode       *packet,
                                              const char     *field,
                                              const char    **value);
VALENT_AVAILABLE_IN_1_0
gboolean      valent_packet_get_array        (JsonNode       *packet,
                                              const char     *field,
                                              JsonArray     **value);
VALENT_AVAILABLE_IN_1_0
gboolean      valent_packet_get_object       (JsonNode       *packet,
                                              const char     *field,
                                              JsonObject    **value);
VALENT_AVAILABLE_IN_1_0
GStrv         valent_packet_dup_strv         (JsonNode       *packet,
                                              const char     *field);

/* I/O Helpers */
VALENT_AVAILABLE_IN_1_0
gboolean      valent_packet_validate         (JsonNode       *packet,
                                              GError        **error);
VALENT_AVAILABLE_IN_1_0
JsonNode    * valent_packet_from_stream      (GInputStream   *stream,
                                              gssize          max_len,
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

G_END_DECLS
