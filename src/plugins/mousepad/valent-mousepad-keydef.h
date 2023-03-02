// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gdk/gdk.h>

G_BEGIN_DECLS

/**
 * valent_mousepad_keycode_to_keyval:
 * @keycode: a special keycode
 *
 * Convert @keycode from a KDE Connect special key to a GDK keyval. Returns `0`
 * if @keycode is not a special key.
 *
 * Returns: a GDK keyval
 */
static inline unsigned int
valent_mousepad_keycode_to_keyval (unsigned int keycode)
{
  static const unsigned int keymap[] = {
    0,                   // 0 (Invalid)
    GDK_KEY_BackSpace,   // 1
    GDK_KEY_Tab,         // 2
    GDK_KEY_Linefeed,    // 3
    GDK_KEY_Left,        // 4
    GDK_KEY_Up,          // 5
    GDK_KEY_Right,       // 6
    GDK_KEY_Down,        // 7
    GDK_KEY_Page_Up,     // 8
    GDK_KEY_Page_Down,   // 9
    GDK_KEY_Home,        // 10
    GDK_KEY_End,         // 11
    GDK_KEY_Return,      // 12
    GDK_KEY_Delete,      // 13
    GDK_KEY_Escape,      // 14
    GDK_KEY_Sys_Req,     // 15
    GDK_KEY_Scroll_Lock, // 16
    0,                   // 17
    0,                   // 18
    0,                   // 19
    0,                   // 20
    GDK_KEY_F1,          // 21
    GDK_KEY_F2,          // 22
    GDK_KEY_F3,          // 23
    GDK_KEY_F4,          // 24
    GDK_KEY_F5,          // 25
    GDK_KEY_F6,          // 26
    GDK_KEY_F7,          // 27
    GDK_KEY_F8,          // 28
    GDK_KEY_F9,          // 29
    GDK_KEY_F10,         // 30
    GDK_KEY_F11,         // 31
    GDK_KEY_F12,         // 32
  };

  if (keycode > 32)
    return 0;

  return keymap[keycode];
}

/**
 * valent_mousepad_keyval_to_keycode:
 * @keyval: a key value
 *
 * Convert @keyval from a GDK keyval to a special key for KDE Connect. Returns
 * `0` if not a special key.
 *
 * Returns: a special key code
 */
static inline unsigned int
valent_mousepad_keyval_to_keycode (unsigned int keyval)
{
  switch (keyval)
    {
    case GDK_KEY_BackSpace:
      return 1;

    case GDK_KEY_Tab:
    case GDK_KEY_ISO_Left_Tab:
      return 2;

    case GDK_KEY_Linefeed:
      return 3;

    case GDK_KEY_Left:
      return 4;

    case GDK_KEY_Up:
      return 5;

    case GDK_KEY_Right:
      return 6;

    case GDK_KEY_Down:
      return 7;

    case GDK_KEY_Page_Up:
      return 8;

    case GDK_KEY_Page_Down:
      return 9;

    case GDK_KEY_Home:
      return 10;

    case GDK_KEY_End:
      return 11;

    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
      return 12;

    case GDK_KEY_Delete:
      return 13;

    case GDK_KEY_Escape:
      return 14;

    case GDK_KEY_Sys_Req:
      return 15;

    case GDK_KEY_Scroll_Lock:
      return 16;

    case GDK_KEY_F1:
      return 21;

    case GDK_KEY_F2:
      return 22;

    case GDK_KEY_F3:
      return 23;

    case GDK_KEY_F4:
      return 24;

    case GDK_KEY_F5:
      return 25;

    case GDK_KEY_F6:
      return 26;

    case GDK_KEY_F7:
      return 27;

    case GDK_KEY_F8:
      return 28;

    case GDK_KEY_F9:
      return 29;

    case GDK_KEY_F10:
      return 30;

    case GDK_KEY_F11:
      return 31;

    case GDK_KEY_F12:
      return 32;

    default:
      return 0;
    }
}

G_END_DECLS

