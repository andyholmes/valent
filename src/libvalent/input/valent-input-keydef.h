// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

/**
 * ValentPointerButton:
 * @VALENT_POINTER_UNKNOWN: Unknown button
 * @VALENT_POINTER_PRIMARY: Primary button (usually left)
 * @VALENT_POINTER_MIDDLE: Middle Button
 * @VALENT_POINTER_SECONDARY: Secondary Button (usually right)
 * @VALENT_POINTER_WHEEL_DOWN: Scroll-down
 * @VALENT_POINTER_WHEEL_UP: Scroll-up
 * @VALENT_POINTER_6: Scroll-left
 * @VALENT_POINTER_7: Scroll-right
 *
 * Enumeration of pointer buttons.
 *
 * Since: 1.0
 */
typedef enum
{
  VALENT_POINTER_UNKNOWN,
  VALENT_POINTER_PRIMARY,
  VALENT_POINTER_MIDDLE,
  VALENT_POINTER_SECONDARY,
  VALENT_POINTER_WHEEL_DOWN,
  VALENT_POINTER_WHEEL_UP,
  VALENT_POINTER_6,
  VALENT_POINTER_7,
} ValentPointerButton;

#define VALENT_IS_POINTER_BUTTON(button) (button > 0 && button < 8)

