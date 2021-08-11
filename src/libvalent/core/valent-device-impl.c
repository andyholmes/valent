// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>

#include "valent-device.h"
#include "valent-device-impl.h"


struct _ValentDeviceImpl
{
  GDBusInterfaceSkeleton  parent_instance;
  GDBusInterfaceInfo     *info;
  GDBusInterfaceVTable   *vtable;

  ValentDevice           *device;
  GHashTable             *cache;
  GHashTable             *pending;
  unsigned int            flush_id;
};

G_DEFINE_TYPE (ValentDeviceImpl, valent_device_impl, G_TYPE_DBUS_INTERFACE_SKELETON);

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES,
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * ca.andyholmes.Valent.Device Interface
 */
static const GDBusPropertyInfo iface_property_connected = {
  -1,
  "Connected",
  "b",
  G_DBUS_PROPERTY_INFO_FLAGS_READABLE,
  NULL
};

static const GDBusPropertyInfo iface_property_icon_name = {
  -1,
  "IconName",
  "s",
  G_DBUS_PROPERTY_INFO_FLAGS_READABLE,
  NULL
};

static const GDBusPropertyInfo iface_property_id = {
  -1,
  "Id",
  "s",
  G_DBUS_PROPERTY_INFO_FLAGS_READABLE,
  NULL
};

static const GDBusPropertyInfo iface_property_name = {
  -1,
  "Name",
  "s",
  G_DBUS_PROPERTY_INFO_FLAGS_READABLE,
  NULL
};

static const GDBusPropertyInfo iface_property_paired = {
  -1,
  "Paired",
  "b",
  G_DBUS_PROPERTY_INFO_FLAGS_READABLE,
  NULL
};

static const GDBusPropertyInfo iface_property_state = {
  -1,
  "State",
  "u",
  G_DBUS_PROPERTY_INFO_FLAGS_READABLE,
  NULL
};

static const GDBusPropertyInfo iface_property_type = {
  -1,
  "Type",
  "s",
  G_DBUS_PROPERTY_INFO_FLAGS_READABLE,
  NULL
};

static const GDBusPropertyInfo * const iface_properties[] = {
  &iface_property_connected,
  &iface_property_icon_name,
  &iface_property_id,
  &iface_property_name,
  &iface_property_paired,
  &iface_property_state,
  &iface_property_type,
  NULL,
};

static const GDBusInterfaceInfo iface_info = {
  -1,
  "ca.andyholmes.Valent.Device",
  NULL,
  NULL,
  (GDBusPropertyInfo **)&iface_properties,
  NULL
};


/*
 * Helper Functions
 */
static gboolean
flush_idle (gpointer data)
{
  GDBusInterfaceSkeleton *skeleton = G_DBUS_INTERFACE_SKELETON (data);

  g_dbus_interface_skeleton_flush (skeleton);

  return G_SOURCE_REMOVE;
}

static GVariant *
gvalue_to_gvariant (GValue *value)
{
  GVariant *variant;

  switch (G_VALUE_TYPE (value))
    {
    case G_TYPE_BOOLEAN:
      variant = g_variant_new_boolean (g_value_get_boolean (value));
      break;

    case G_TYPE_STRING:
      variant = g_variant_new_string (g_value_get_string (value));
      break;

    default:
      variant = g_variant_new_uint32 (g_value_get_flags (value));
    }

  return g_variant_ref_sink (variant);
}

static void
on_property_changed (GObject          *object,
                     GParamSpec       *pspec,
                     ValentDeviceImpl *self)
{
  g_autoptr (GVariant) variant = NULL;
  GValue value = G_VALUE_INIT;
  const char *gname;
  const char *vname;

  /* Retrieve the property */
  gname = g_param_spec_get_name (pspec);

  if (g_str_equal (gname, "connected"))
    vname = "Connected";
  else if (g_str_equal (gname, "icon-name"))
    vname = "IconName";
  else if (g_str_equal (gname, "id"))
    vname = "Id";
  else if (g_str_equal (gname, "name"))
    vname = "Name";
  else if (g_str_equal (gname, "paired"))
    vname = "Paired";
  else if (g_str_equal (gname, "state"))
    vname = "State";
  else if (g_str_equal (gname, "type"))
    vname = "Type";
  else
    return;

  g_value_init (&value, pspec->value_type);
  g_object_get_property (object, gname, &value);
  variant = gvalue_to_gvariant (&value);
  g_value_unset (&value);

  /* Update the cache */
  g_hash_table_replace (self->cache,
                        g_strdup (vname),
                        g_variant_ref (variant));

  /* Queue an emission of PropertiesChanged */
  g_hash_table_replace (self->pending,
                        g_strdup (vname),
                        g_variant_ref (variant));

  if (!self->flush_id)
    self->flush_id = g_idle_add (flush_idle, self);
}

static void
valent_device_impl_load_properties (ValentDeviceImpl *self)
{
  GValue value = G_VALUE_INIT;
  GVariant *variant;

  g_value_init (&value, G_TYPE_BOOLEAN);
  g_object_get_property (G_OBJECT (self->device), "connected", &value);
  variant = gvalue_to_gvariant (&value);
  g_hash_table_insert (self->cache, g_strdup ("Connected"), variant);
  g_value_unset (&value);

  g_value_init (&value, G_TYPE_STRING);
  g_object_get_property (G_OBJECT (self->device), "icon-name", &value);
  variant = gvalue_to_gvariant (&value);
  g_hash_table_insert (self->cache, g_strdup ("IconName"), variant);
  g_value_unset (&value);

  g_value_init (&value, G_TYPE_STRING);
  g_object_get_property (G_OBJECT (self->device), "id", &value);
  variant = gvalue_to_gvariant (&value);
  g_hash_table_insert (self->cache, g_strdup ("Id"), variant);
  g_value_unset (&value);

  g_value_init (&value, G_TYPE_STRING);
  g_object_get_property (G_OBJECT (self->device), "name", &value);
  variant = gvalue_to_gvariant (&value);
  g_hash_table_insert (self->cache, g_strdup ("Name"), variant);
  g_value_unset (&value);

  g_value_init (&value, G_TYPE_BOOLEAN);
  g_object_get_property (G_OBJECT (self->device), "paired", &value);
  variant = gvalue_to_gvariant (&value);
  g_hash_table_insert (self->cache, g_strdup ("Paired"), variant);
  g_value_unset (&value);

  g_value_init (&value, G_TYPE_FLAGS);
  g_object_get_property (G_OBJECT (self->device), "state", &value);
  variant = gvalue_to_gvariant (&value);
  g_hash_table_insert (self->cache, g_strdup ("State"), variant);
  g_value_unset (&value);

  g_value_init (&value, G_TYPE_STRING);
  g_object_get_property (G_OBJECT (self->device), "type", &value);
  variant = gvalue_to_gvariant (&value);
  g_hash_table_insert (self->cache, g_strdup ("Type"), variant);
  g_value_unset (&value);
}


/*
 * GDBusInterfaceVTable
 */
static void
valent_device_impl_method_call (GDBusConnection       *connection,
                                const char            *sender,
                                const char            *object_path,
                                const char            *interface_name,
                                const char            *method_name,
                                GVariant              *parameters,
                                GDBusMethodInvocation *invocation,
                                void                  *user_data)
{
  ValentDeviceImpl *self = VALENT_DEVICE_IMPL (user_data);

  if (!g_dbus_interface_info_lookup_method (self->info, method_name))
    {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR,
                                             G_DBUS_ERROR_UNKNOWN_METHOD,
                                             "Unknown method %s on %s",
                                             method_name,
                                             interface_name);
      return;
    }

  g_object_unref (invocation);
}

static GVariant *
valent_device_impl_property_get (GDBusConnection  *connection,
                                 const char       *sender,
                                 const char       *object_path,
                                 const char       *interface_name,
                                 const char       *property_name,
                                 GError          **error,
                                 void             *user_data)
{
  ValentDeviceImpl *self = VALENT_DEVICE_IMPL (user_data);
  GVariant *value;

  if ((value = g_hash_table_lookup (self->cache, property_name)) != NULL)
    return g_variant_ref (value);

  g_set_error (error,
               G_DBUS_ERROR,
               G_DBUS_ERROR_FAILED,
               "Failed to read %s property on %s",
               property_name,
               interface_name);

  return NULL;
}

static gboolean
valent_device_impl_property_set (GDBusConnection  *connection,
                                 const char       *sender,
                                 const char       *object_path,
                                 const char       *interface_name,
                                 const char       *property_name,
                                 GVariant         *value,
                                 GError          **error,
                                 void             *user_data)
{
  g_set_error (error,
               G_DBUS_ERROR,
               G_DBUS_ERROR_PROPERTY_READ_ONLY,
               "Read-only property %s on %s",
               property_name,
               interface_name);

  return FALSE;
}

static const GDBusInterfaceVTable iface_vtable = {
  valent_device_impl_method_call,
  valent_device_impl_property_get,
  valent_device_impl_property_set,
};


/*
 * GDBusInterfaceSkeleton
 */
static void
valent_device_impl_flush (GDBusInterfaceSkeleton *skeleton)
{
  ValentDeviceImpl *self = VALENT_DEVICE_IMPL (skeleton);
  g_autolist (GDBusConnection) connections = NULL;
  g_autoptr (GVariant) properties = NULL;
  const char *object_path;
  GVariantBuilder changed_properties;
  GVariantBuilder invalidated_properties;
  GHashTableIter iter;
  gpointer key, value;

  /* Collect the pending property changes */
  g_variant_builder_init (&changed_properties, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_init (&invalidated_properties, G_VARIANT_TYPE_STRING_ARRAY);
  g_hash_table_iter_init (&iter, self->pending);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (value)
        g_variant_builder_add (&changed_properties, "{sv}", key, value);
      else
        g_variant_builder_add (&invalidated_properties, "s", key);
    }

  properties = g_variant_new ("(s@a{sv}@as)",
                              self->info->name,
                              g_variant_builder_end (&changed_properties),
                              g_variant_builder_end (&invalidated_properties));
  g_variant_ref_sink (properties);

  /* Emit PropertiesChanged on each connection */
  connections = g_dbus_interface_skeleton_get_connections (skeleton);
  object_path = g_dbus_interface_skeleton_get_object_path (skeleton);

  for (const GList *iter = connections; iter; iter = iter->next)
    {
      g_dbus_connection_emit_signal (G_DBUS_CONNECTION (iter->data),
                                     NULL,
                                     object_path,
                                     "org.freedesktop.DBus.Properties",
                                     "PropertiesChanged",
                                     properties,
                                     NULL);
    }

  /* Clear the pending state */
  g_hash_table_remove_all (self->pending);
  g_clear_handle_id (&self->flush_id, g_source_remove);
}

static GVariant *
valent_device_impl_get_properties (GDBusInterfaceSkeleton *skeleton)
{
  ValentDeviceImpl *self = VALENT_DEVICE_IMPL (skeleton);
  GVariantBuilder builder;
  GHashTableIter iter;
  gpointer key, value;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
  g_hash_table_iter_init (&iter, self->cache);

  while (g_hash_table_iter_next (&iter, &key, &value))
    g_variant_builder_add (&builder, "{sv}", key, value);

  return g_variant_builder_end (&builder);
}

static GDBusInterfaceInfo *
valent_device_impl_get_info (GDBusInterfaceSkeleton *skeleton)
{
  ValentDeviceImpl* self = VALENT_DEVICE_IMPL (skeleton);

  return self->info;
}

static GDBusInterfaceVTable *
valent_device_impl_get_vtable (GDBusInterfaceSkeleton *skeleton)
{
  ValentDeviceImpl* self = VALENT_DEVICE_IMPL (skeleton);

  return self->vtable;
}


/*
 * GObject
 */
static void
valent_device_impl_constructed (GObject *object)
{
  ValentDeviceImpl *self = VALENT_DEVICE_IMPL (object);

  valent_device_impl_load_properties (self);

  g_signal_connect (self->device,
                    "notify",
                    G_CALLBACK (on_property_changed),
                    self);

  G_OBJECT_CLASS (valent_device_impl_parent_class)->constructed (object);
}

static void
valent_device_impl_dispose (GObject *object)
{
  ValentDeviceImpl *self = VALENT_DEVICE_IMPL (object);

  g_signal_handlers_disconnect_by_data (self->device, self);
  g_clear_handle_id (&self->flush_id, g_source_remove);

  G_OBJECT_CLASS (valent_device_impl_parent_class)->dispose (object);
}

static void
valent_device_impl_finalize (GObject *object)
{
  ValentDeviceImpl *self = VALENT_DEVICE_IMPL (object);

  g_clear_pointer (&self->cache, g_hash_table_unref);
  g_clear_pointer (&self->pending, g_hash_table_unref);

  G_OBJECT_CLASS (valent_device_impl_parent_class)->finalize (object);
}

static void
valent_device_impl_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ValentDeviceImpl *self = VALENT_DEVICE_IMPL (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_impl_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ValentDeviceImpl *self = VALENT_DEVICE_IMPL (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

void
valent_device_impl_class_init (ValentDeviceImplClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GDBusInterfaceSkeletonClass *skeleton_class = G_DBUS_INTERFACE_SKELETON_CLASS (klass);

  object_class->constructed = valent_device_impl_constructed;
  object_class->dispose = valent_device_impl_dispose;
  object_class->finalize = valent_device_impl_finalize;
  object_class->get_property = valent_device_impl_get_property;
  object_class->set_property = valent_device_impl_set_property;

  skeleton_class->get_info = valent_device_impl_get_info;
  skeleton_class->get_vtable = valent_device_impl_get_vtable;
  skeleton_class->get_properties = valent_device_impl_get_properties;
  skeleton_class->flush = valent_device_impl_flush;

  properties[PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "The exported device",
                         VALENT_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_device_impl_init (ValentDeviceImpl *self)
{
  self->info = (GDBusInterfaceInfo *)&iface_info;
  self->vtable = (GDBusInterfaceVTable *)&iface_vtable;
  self->cache = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       (GDestroyNotify)g_variant_unref);
  self->pending = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         (GDestroyNotify)g_variant_unref);
}

/**
 * valent_device_impl_new:
 * @device: a #ValentDevice
 *
 * Create a new #ValentDeviceImpl.
 *
 * Returns: (transfer full): a #GDBusInterfaceSkeleton
 */
GDBusInterfaceSkeleton *
valent_device_impl_new (ValentDevice *device)
{
  return g_object_new (VALENT_TYPE_DEVICE_IMPL,
                       "device", device,
                       NULL);
}

