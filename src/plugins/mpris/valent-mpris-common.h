// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

/**
 * VALENT_MPRIS_DBUS_NAME: (value "org.mpris.MediaPlayer2.Valent")
 *
 * The well-known name Valent exports its MPRIS player on.
 */
#define VALENT_MPRIS_DBUS_NAME "org.mpris.MediaPlayer2.Valent"


/*
 * DBus Interfaces
 */
static const char mpris_xml[] =
  "<node name='/org/mpris/MediaPlayer2'>"
  "  <interface name='org.mpris.MediaPlayer2'>"
  "    <method name='Raise'/>"
  "    <method name='Quit'/>"
  "    <property name='CanQuit' type='b' access='read'/>"
  "    <property name='Fullscreen' type='b' access='readwrite'/>"
  "    <property name='CanSetFullscreen' type='b' access='read'/>"
  "    <property name='CanRaise' type='b' access='read'/>"
  "    <property name='HasTrackList' type='b' access='read'/>"
  "    <property name='Identity' type='s' access='read'/>"
  "    <property name='DesktopEntry' type='s' access='read'/>"
  "    <property name='SupportedUriSchemes' type='as' access='read'/>"
  "    <property name='SupportedMimeTypes' type='as' access='read'/>"
  "  </interface>"
  "  <interface name='org.mpris.MediaPlayer2.Player'>"
  "    <method name='Next'/>"
  "    <method name='Previous'/>"
  "    <method name='Pause'/>"
  "    <method name='PlayPause'/>"
  "    <method name='Stop'/>"
  "    <method name='Play'/>"
  "    <method name='Seek'>"
  "      <arg direction='in' type='x' name='Offset'/>"
  "    </method>"
  "    <method name='SetPosition'>"
  "      <arg direction='in' type='o' name='TrackId'/>"
  "      <arg direction='in' type='x' name='Position'/>"
  "    </method>"
  "    <method name='OpenUri'>"
  "      <arg direction='in' type='s' name='Uri'/>"
  "    </method>"
  "    <property name='PlaybackStatus' type='s' access='read'/>"
  "    <property name='LoopStatus' type='s' access='readwrite'/>"
  "    <property name='Rate' type='d' access='readwrite'/>"
  "    <property name='Shuffle' type='b' access='readwrite'/>"
  "    <property name='Metadata' type='a{sv}' access='read'/>"
  "    <property name='Volume' type='d' access='readwrite'/>"
  "    <property name='Position' type='x' access='read'/>"
  "    <property name='MinimumRate' type='d' access='read'/>"
  "    <property name='MaximumRate' type='d' access='read'/>"
  "    <property name='CanGoNext' type='b' access='read'/>"
  "    <property name='CanGoPrevious' type='b' access='read'/>"
  "    <property name='CanPlay' type='b' access='read'/>"
  "    <property name='CanPause' type='b' access='read'/>"
  "    <property name='CanSeek' type='b' access='read'/>"
  "    <property name='CanControl' type='b' access='read'/>"
  "    <signal name='Seeked'>"
  "      <arg name='Position' type='x'/>"
  "    </signal>"
  "  </interface>"
  "</node>";

static GDBusNodeInfo *mpris_info = NULL;


static inline void
valent_mpris_init (void)
{
  static gsize guard = 0;

  if (g_once_init_enter (&guard))
    {
      mpris_info = g_dbus_node_info_new_for_xml (mpris_xml, NULL);
      g_dbus_interface_info_cache_build (mpris_info->interfaces[0]);
      g_dbus_interface_info_cache_build (mpris_info->interfaces[1]);

      g_once_init_leave (&guard, 1);
    }
}

/**
 * valent_mpris_get_node_info:
 *
 * Get the MPRIS #GDBusNodeInfo.
 *
 * Returns: (transfer none): a #GDBusNodeInfo
 */
static inline GDBusNodeInfo *
valent_mpris_get_node_info (void)
{
  valent_mpris_init ();

  return mpris_info;
}

/**
 * valent_mpris_get_application_iface:
 *
 * Get the org.mpris.MediaPlayer2 #GDBusInterfaceInfo.
 *
 * Returns: (transfer none): a #GDBusInterfaceInfo
 */
static inline GDBusInterfaceInfo *
valent_mpris_get_application_iface (void)
{
  valent_mpris_init ();

  return mpris_info->interfaces[0];
}

/**
 * valent_mpris_get_player_iface:
 *
 * Get the org.mpris.MediaPlayer2.Player #GDBusInterfaceInfo.
 *
 * Returns: (transfer none): a #GDBusInterfaceInfo
 */
static inline GDBusInterfaceInfo *
valent_mpris_get_player_iface (void)
{
  valent_mpris_init ();

  return mpris_info->interfaces[1];
}

