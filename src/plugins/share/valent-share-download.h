// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_SHARE_DOWNLOAD (valent_share_download_get_type())

G_DECLARE_FINAL_TYPE (ValentShareDownload, valent_share_download, VALENT, SHARE_DOWNLOAD, ValentTransfer)

ValentTransfer * valent_share_download_new      (ValentDevice         *device);
void             valent_share_download_add_file (ValentShareDownload  *download,
                                                 GFile                *file,
                                                 JsonNode             *packet);
void             valent_share_download_update   (ValentShareDownload  *download,
                                                 JsonNode             *packet);

G_END_DECLS

