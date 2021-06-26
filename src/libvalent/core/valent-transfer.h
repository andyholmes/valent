// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif


#include <gio/gio.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

/**
 * ValentTransferState:
 * @VALENT_TRANSFER_STATE_NONE: None
 * @VALENT_TRANSFER_STATE_ACTIVE: Transfer is in progress
 * @VALENT_TRANSFER_STATE_COMPLETE: Transfer is complete
 *
 * Enumeration of transfer states.
 */
typedef enum
{
  VALENT_TRANSFER_STATE_NONE,
  VALENT_TRANSFER_STATE_ACTIVE,
  VALENT_TRANSFER_STATE_COMPLETE
} ValentTransferState;

#define VALENT_TYPE_TRANSFER (valent_transfer_get_type())

G_DECLARE_DERIVABLE_TYPE (ValentTransfer, valent_transfer, VALENT, TRANSFER, GObject)

struct _ValentTransferClass
{
  GObjectClass parent_class;
};

ValentTransfer      * valent_transfer_new            (ValentDevice         *device);

void                  valent_transfer_add_bytes      (ValentTransfer       *transfer,
                                                      JsonNode             *packet,
                                                      GBytes               *bytes);
void                  valent_transfer_add_file       (ValentTransfer       *transfer,
                                                      JsonNode             *packet,
                                                      GFile                *file);
void                  valent_transfer_add_stream     (ValentTransfer       *transfer,
                                                      JsonNode             *packet,
                                                      GInputStream         *source,
                                                      GOutputStream        *target,
                                                      gssize                size);
GFile *               valent_transfer_cache_file     (ValentTransfer       *transfer,
                                                      JsonNode             *packet,
                                                      const char           *name);

void                  valent_transfer_cancel         (ValentTransfer       *transfer);
void                  valent_transfer_execute        (ValentTransfer       *transfer,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
gboolean              valent_transfer_execute_finish (ValentTransfer       *transfer,
                                                      GAsyncResult         *result,
                                                      GError              **error);

ValentDevice        * valent_transfer_get_device     (ValentTransfer       *transfer);
const char          * valent_transfer_get_id         (ValentTransfer       *transfer);
void                  valent_transfer_set_id         (ValentTransfer       *transfer,
                                                      const char           *id);
ValentTransferState   valent_transfer_get_state      (ValentTransfer       *transfer);

G_END_DECLS
