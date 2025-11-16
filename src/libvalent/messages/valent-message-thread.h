// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include "../core/valent-object.h"

G_BEGIN_DECLS

#define VALENT_TYPE_MESSAGE_THREAD (valent_message_thread_get_type())

G_DECLARE_FINAL_TYPE (ValentMessageThread, valent_message_thread, VALENT, MESSAGE_THREAD, ValentObject)

G_END_DECLS
