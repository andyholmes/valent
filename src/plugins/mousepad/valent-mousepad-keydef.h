// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gdk/gdk.h>

G_BEGIN_DECLS

/**
 * valent_mousepad_keysym_is_modifier:
 * @keysym: a keysym
 *
 * Check if @keysym is a known keyboard modifier (e.g. Shift).
 *
 * Returns: %TRUE, or %FALSE if not a modifier
 */
static inline gboolean
valent_mousepad_keysym_is_modifier (uint32_t keysym)
{
  switch (keysym)
    {
    /* Supported */
    case GDK_KEY_Control_L:
    case GDK_KEY_Control_R:
    case GDK_KEY_Alt_L:
    case GDK_KEY_Alt_R:
    case GDK_KEY_Shift_L:
    case GDK_KEY_Shift_R:
    case GDK_KEY_Super_L:
    case GDK_KEY_Super_R:

    /* Unsupported */
    case GDK_KEY_Overlay1_Enable:
    case GDK_KEY_Overlay2_Enable:
    case GDK_KEY_Caps_Lock:
    case GDK_KEY_Shift_Lock:
    case GDK_KEY_Meta_L:
    case GDK_KEY_Meta_R:
    case GDK_KEY_Hyper_L:
    case GDK_KEY_Hyper_R:
    case GDK_KEY_Mode_switch:
    case GDK_KEY_ISO_Level3_Shift:
    case GDK_KEY_ISO_Level3_Latch:
    case GDK_KEY_ISO_Level5_Shift:
    case GDK_KEY_ISO_Level5_Latch:
      return TRUE;
    default:
      return FALSE;
    }
}

/**
 * valent_mousepad_keycode_to_keyval:
 * @keycode: a special keycode
 *
 * Convert @keycode from a KDE Connect special key to a GDK keyval.
 *
 * Returns: a GDK keyval, or `0` if not found
 */
static inline uint32_t
valent_mousepad_keycode_to_keyval (uint32_t  keycode)
{
  static const uint32_t keymap[] = {
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
    0,                   // 17 (Reserved)
    0,                   // 18 (Reserved)
    0,                   // 19 (Reserved)
    0,                   // 20 (Reserved)
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

  if (keycode >= G_N_ELEMENTS (keymap))
    return 0;

  return keymap[keycode];
}

/**
 * valent_mousepad_keyval_to_keycode:
 * @keyval: a key value
 *
 * Convert @keyval from a GDK keyval to a special key for KDE Connect.
 *
 * Returns: a special key code, or `0` if not found
 */
static inline uint32_t
valent_mousepad_keyval_to_keycode (uint32_t keyval)
{
  switch (keyval)
    {
    case GDK_KEY_BackSpace:
      return 1;

    case GDK_KEY_Tab:
    case GDK_KEY_KP_Tab:
      return 2;

    case GDK_KEY_Linefeed:
      return 3;

    case GDK_KEY_Left:
    case GDK_KEY_KP_Left:
      return 4;

    case GDK_KEY_Up:
    case GDK_KEY_KP_Up:
      return 5;

    case GDK_KEY_Right:
    case GDK_KEY_KP_Right:
      return 6;

    case GDK_KEY_Down:
    case GDK_KEY_KP_Down:
      return 7;

    case GDK_KEY_Page_Up:
    case GDK_KEY_KP_Page_Up:
      return 8;

    case GDK_KEY_Page_Down:
    case GDK_KEY_KP_Page_Down:
      return 9;

    case GDK_KEY_Home:
    case GDK_KEY_KP_Home:
      return 10;

    case GDK_KEY_End:
    case GDK_KEY_KP_End:
      return 11;

    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
    case GDK_KEY_ISO_Enter:
      return 12;

    case GDK_KEY_Delete:
    case GDK_KEY_KP_Delete:
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

