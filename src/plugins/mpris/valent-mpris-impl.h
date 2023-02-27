// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MPRIS_IMPL (valent_mpris_impl_get_type())

G_DECLARE_FINAL_TYPE (ValentMPRISImpl, valent_mpris_impl, VALENT, MPRIS_IMPL, GObject)

ValentMPRISImpl * valent_mpris_impl_new           (ValentMediaPlayer    *player);
gboolean          valent_mpris_impl_export        (ValentMPRISImpl      *impl,
                                                   GDBusConnection      *connection,
                                                   GError              **error);
void              valent_mpris_impl_export_full   (ValentMPRISImpl      *impl,
                                                   const char           *bus_name,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              user_data);
gboolean          valent_mpris_impl_export_finish (ValentMPRISImpl      *impl,
                                                   GAsyncResult         *result,
                                                   GError              **error);
void              valent_mpris_impl_unexport      (ValentMPRISImpl      *impl);

G_END_DECLS

