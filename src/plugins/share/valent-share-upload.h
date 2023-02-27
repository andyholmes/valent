// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_SHARE_UPLOAD (valent_share_upload_get_type())

G_DECLARE_FINAL_TYPE (ValentShareUpload, valent_share_upload, VALENT, SHARE_UPLOAD, ValentTransfer)

ValentTransfer * valent_share_upload_new       (ValentDevice      *device);
void             valent_share_upload_add_file  (ValentShareUpload *upload,
                                                GFile             *file);
void             valent_share_upload_add_files (ValentShareUpload *upload,
                                                GListModel        *files);

G_END_DECLS

