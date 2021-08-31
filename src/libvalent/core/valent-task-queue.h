// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include <gio/gio.h>

G_BEGIN_DECLS

#define VALENT_IS_TASK_QUEUE(ptr) (ptr != NULL)
#define VALENT_TASK_QUEUE(ptr)    ((ValentTaskQueue *)ptr)

typedef struct    _ValentTaskQueue ValentTaskQueue;

GType             valent_task_queue_get_type   (void) G_GNUC_CONST;


#define VALENT_TYPE_TASK_QUEUE (valent_task_queue_get_type())

ValentTaskQueue * valent_task_queue_new       (void);
void              valent_task_queue_run       (ValentTaskQueue *queue,
                                               GTask           *task,
                                               GTaskThreadFunc  task_func);
void              valent_task_queue_run_check (ValentTaskQueue *queue,
                                               GTask           *task,
                                               GTaskThreadFunc  task_func);
void              valent_task_queue_run_close (ValentTaskQueue *queue,
                                               GTask           *task,
                                               GTaskThreadFunc  task_func);
void              valent_task_queue_run_sync  (ValentTaskQueue *queue,
                                               GTask           *task,
                                               GTaskThreadFunc  task_func);
ValentTaskQueue * valent_task_queue_ref       (ValentTaskQueue *queue);
void              valent_task_queue_unref     (ValentTaskQueue *queue);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ValentTaskQueue, g_atomic_rc_box_release)

G_END_DECLS

