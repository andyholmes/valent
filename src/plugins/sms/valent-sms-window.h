// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-message.h"
#include "valent-sms-store.h"

G_BEGIN_DECLS

#define VALENT_TYPE_SMS_WINDOW (valent_sms_window_get_type())

G_DECLARE_FINAL_TYPE (ValentSmsWindow, valent_sms_window, VALENT, SMS_WINDOW, AdwApplicationWindow)

ValentContactStore * valent_sms_window_get_contact_store  (ValentSmsWindow    *window);
void                 valent_sms_window_set_contact_store  (ValentSmsWindow    *window,
                                                           ValentContactStore *store);
ValentSmsStore     * valent_sms_window_get_message_store  (ValentSmsWindow    *window);

void                 valent_sms_window_search_contacts    (ValentSmsWindow    *window,
                                                           const char         *query);
void                 valent_sms_window_search_messages    (ValentSmsWindow    *window,
                                                           const char         *query);

void                 valent_sms_window_set_active_address (ValentSmsWindow    *window,
                                                           const char         *address,
                                                           EContact           *contact);

void                 valent_sms_window_set_active_message (ValentSmsWindow    *window,
                                                           ValentMessage      *message);
void                 valent_sms_window_set_active_thread  (ValentSmsWindow    *window,
                                                           gint64              thread_id);

void                 valent_sms_window_reset_search       (ValentSmsWindow    *window);

G_END_DECLS
