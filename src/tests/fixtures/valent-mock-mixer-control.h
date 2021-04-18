// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_TEST_INSIDE) && !defined (VALENT_TEST_COMPILATION)
# error "Only <libvalent-test.h> can be included directly."
#endif

#include <libvalent-mixer.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_MIXER_CONTROL (valent_mock_mixer_control_get_type ())

G_DECLARE_FINAL_TYPE (ValentMockMixerControl, valent_mock_mixer_control, VALENT, MOCK_MIXER_CONTROL, ValentMixerControl)

G_END_DECLS

