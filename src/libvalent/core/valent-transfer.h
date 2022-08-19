// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include <gio/gio.h>

#include "valent-object.h"

G_BEGIN_DECLS

/**
 * ValentTransferState:
 * @VALENT_TRANSFER_STATE_PENDING: The transfer is pending
 * @VALENT_TRANSFER_STATE_ACTIVE: The transfer is in progress
 * @VALENT_TRANSFER_STATE_COMPLETE: The transfer succeeded
 * @VALENT_TRANSFER_STATE_FAILED: The transfer failed
 *
 * Enumeration of transfer states.
 *
 * Since: 1.0
 */
typedef enum
{
  VALENT_TRANSFER_STATE_PENDING,
  VALENT_TRANSFER_STATE_ACTIVE,
  VALENT_TRANSFER_STATE_COMPLETE,
  VALENT_TRANSFER_STATE_FAILED,
} ValentTransferState;

#define VALENT_TYPE_TRANSFER (valent_transfer_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentTransfer, valent_transfer, VALENT, TRANSFER, ValentObject)

struct _ValentTransferClass
{
  ValentObjectClass   parent_class;

  /* virtual functions */
  void                (*execute)        (ValentTransfer       *transfer,
                                         GCancellable         *cancellable,
                                         GAsyncReadyCallback   callback,
                                         gpointer              user_data);
  gboolean            (*execute_finish) (ValentTransfer       *transfer,
                                         GAsyncResult         *result,
                                         GError              **error);

  /*< private >*/
  gpointer            padding[8];
};

VALENT_AVAILABLE_IN_1_0
char                * valent_transfer_dup_id         (ValentTransfer       *transfer);
VALENT_AVAILABLE_IN_1_0
double                valent_transfer_get_progress   (ValentTransfer       *transfer);
VALENT_AVAILABLE_IN_1_0
void                  valent_transfer_set_progress   (ValentTransfer       *transfer,
                                                      double                progress);
VALENT_AVAILABLE_IN_1_0
ValentTransferState   valent_transfer_get_state      (ValentTransfer       *transfer);
VALENT_AVAILABLE_IN_1_0
void                  valent_transfer_execute        (ValentTransfer       *transfer,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
gboolean              valent_transfer_execute_finish (ValentTransfer       *transfer,
                                                      GAsyncResult         *result,
                                                      GError              **error);
VALENT_AVAILABLE_IN_1_0
void                  valent_transfer_cancel         (ValentTransfer       *transfer);
VALENT_AVAILABLE_IN_1_0
gboolean              valent_transfer_check_status   (ValentTransfer       *transfer,
                                                      GError              **error);

G_END_DECLS

