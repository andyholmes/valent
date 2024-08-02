// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <stdint.h>

#include <glib.h>

G_BEGIN_DECLS

/*< private>
 *
 * These are the X11 keysyms that need to be translated to/from
 * KDE Connect keycodes.
 *
 * See: https://cgit.freedesktop.org/xorg/proto/x11proto/plain/keysymdef.h
 */
#define KEYSYM_ISO_Level3_Shift 0xfe03
#define KEYSYM_ISO_Level3_Latch 0xfe04
#define KEYSYM_ISO_Level5_Shift 0xfe11
#define KEYSYM_ISO_Level5_Latch 0xfe12
#define KEYSYM_ISO_Enter        0xfe34
#define KEYSYM_Overlay1_Enable  0xfe78
#define KEYSYM_Overlay2_Enable  0xfe79
#define KEYSYM_BackSpace        0xff08
#define KEYSYM_Tab              0xff09
#define KEYSYM_Linefeed         0xff0a
#define KEYSYM_Return           0xff0d
#define KEYSYM_Scroll_Lock      0xff14
#define KEYSYM_Sys_Req          0xff15
#define KEYSYM_Escape           0xff1b
#define KEYSYM_Home             0xff50
#define KEYSYM_Left             0xff51
#define KEYSYM_Up               0xff52
#define KEYSYM_Right            0xff53
#define KEYSYM_Down             0xff54
#define KEYSYM_Page_Up          0xff55
#define KEYSYM_Page_Down        0xff56
#define KEYSYM_End              0xff57
#define KEYSYM_Mode_switch      0xff7e
#define KEYSYM_KP_Tab           0xff89
#define KEYSYM_KP_Enter         0xff8d
#define KEYSYM_KP_Home          0xff95
#define KEYSYM_KP_Left          0xff96
#define KEYSYM_KP_Up            0xff97
#define KEYSYM_KP_Right         0xff98
#define KEYSYM_KP_Down          0xff99
#define KEYSYM_KP_Page_Up       0xff9a
#define KEYSYM_KP_Page_Down     0xff9b
#define KEYSYM_KP_End           0xff9c
#define KEYSYM_KP_Delete        0xff9f
#define KEYSYM_F1               0xffbe
#define KEYSYM_F2               0xffbf
#define KEYSYM_F3               0xffc0
#define KEYSYM_F4               0xffc1
#define KEYSYM_F5               0xffc2
#define KEYSYM_F6               0xffc3
#define KEYSYM_F7               0xffc4
#define KEYSYM_F8               0xffc5
#define KEYSYM_F9               0xffc6
#define KEYSYM_F10              0xffc7
#define KEYSYM_F11              0xffc8
#define KEYSYM_F12              0xffc9
#define KEYSYM_Shift_L          0xffe1
#define KEYSYM_Shift_R          0xffe2
#define KEYSYM_Control_L        0xffe3
#define KEYSYM_Control_R        0xffe4
#define KEYSYM_Caps_Lock        0xffe5
#define KEYSYM_Shift_Lock       0xffe6
#define KEYSYM_Meta_L           0xffe7
#define KEYSYM_Meta_R           0xffe8
#define KEYSYM_Alt_L            0xffe9
#define KEYSYM_Alt_R            0xffea
#define KEYSYM_Super_L          0xffeb
#define KEYSYM_Super_R          0xffec
#define KEYSYM_Hyper_L          0xffed
#define KEYSYM_Hyper_R          0xffee

#define KEYSYM_Delete           0xffff

/**
 * KeyModifierType:
 * @KEYMOD_NONE_MASK: no modifiers set
 * @KEYMOD_SHIFT_MASK: the Shift key.
 * @KEYMOD_LOCK_MASK: a Lock key (CapsLock or ShiftLock).
 * @KEYMOD_CONTROL_MASK: the Control key.
 * @KEYMOD_META_MASK: the Meta modifier. Maps to Command on macOS.
 * @KEYMOD_ALT_MASK: the Alt key.
 * @KEYMOD_SUPER_MASK: the Super modifier.
 * @KEYMOD_HYPER_MASK: the Hyper modifier.
 * @KEYMOD_KDE_MASK: a mask for all modifiers supported by KDE Connect.
 * @KEYMOD_ANY_MASK: a mask for all modifiers.
 *
 * Mask flags for keyboard modifiers.
 */
typedef enum
{
  KEYMOD_NONE_MASK    = 0,       // 0x00
  KEYMOD_SHIFT_MASK   = 1 << 0,  // 0x01
  KEYMOD_CONTROL_MASK = 1 << 1,  // 0x02
  KEYMOD_LOCK_MASK    = 1 << 2,  // 0x04
  KEYMOD_META_MASK    = 1 << 3,  // 0x08
  KEYMOD_ALT_MASK     = 1 << 4,  // 0x10
  KEYMOD_SUPER_MASK   = 1 << 5,  // 0x20
  KEYMOD_HYPER_MASK   = 1 << 6,  // 0x40

  KEYMOD_KDE_MASK     = 0x33,
  KEYMOD_ANY_MASK     = 0x7f,
} KeyModifierType;


gboolean   valent_input_keysym_to_modifier   (uint32_t         keysym,
                                              gboolean         state,
                                              KeyModifierType *out_modifier);
uint32_t   valent_input_keysym_to_unicode    (uint32_t         keysym);
uint32_t   valent_input_unicode_to_keysym    (uint32_t         wc);

uint32_t   valent_mousepad_keycode_to_keysym (uint32_t         keycode);
uint32_t   valent_mousepad_keysym_to_keycode (uint32_t         keysym);

G_END_DECLS

