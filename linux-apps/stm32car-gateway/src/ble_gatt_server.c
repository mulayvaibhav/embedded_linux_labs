#include "ble_gatt_server.h"

#include <gio/gio.h>
#include <glib.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define BLUEZ_BUS_NAME              "org.bluez"
#define BLUEZ_ADAPTER_PATH          "/org/bluez/hci0"

#define APP_PATH                    "/com/stm32car"
#define SERVICE_PATH                "/com/stm32car/service0"
#define CHAR_PATH                   "/com/stm32car/service0/char0"
#define ADV_PATH                    "/com/stm32car/advertisement0"

#define GATT_MANAGER_IFACE          "org.bluez.GattManager1"
#define GATT_SERVICE_IFACE          "org.bluez.GattService1"
#define GATT_CHARACTERISTIC_IFACE   "org.bluez.GattCharacteristic1"
#define LE_ADV_MANAGER_IFACE        "org.bluez.LEAdvertisingManager1"
#define LE_ADV_IFACE                "org.bluez.LEAdvertisement1"
#define DBUS_PROP_IFACE             "org.freedesktop.DBus.Properties"

#define STM32CAR_SERVICE_UUID       "12345678-1234-5678-1234-56789abcdef0"
#define STM32CAR_COMMAND_UUID       "12345678-1234-5678-1234-56789abcdef1"

#define BLE_WORKER_QUIT_CMD         "__quit__"

static GDBusConnection *g_conn;
static GMainLoop *g_loop;
static GThread *g_ble_thread;

static guint g_obj_manager_id;
static guint g_service_id;
static guint g_service_prop_id;
static guint g_char_id;
static guint g_char_prop_id;
static guint g_adv_id;
static guint g_adv_prop_id;

static ble_command_callback_t g_command_cb;

static GAsyncQueue *g_command_queue;
static GThread *g_command_worker_thread;
static volatile int g_command_worker_running;

static GMutex g_start_mutex;
static GCond g_start_cond;
static int g_start_done;
static int g_start_result;

static const gchar *char_flags[] = {
    "write",
    "write-without-response",
    NULL
};

static const gchar *adv_service_uuids[] = {
    STM32CAR_SERVICE_UUID,
    NULL
};

static const gchar object_manager_xml[] =
    "<node>"
    "  <interface name='org.freedesktop.DBus.ObjectManager'>"
    "    <method name='GetManagedObjects'>"
    "      <arg name='objects' type='a{oa{sa{sv}}}' direction='out'/>"
    "    </method>"
    "  </interface>"
    "</node>";

static const gchar service_xml[] =
    "<node>"
    "  <interface name='org.bluez.GattService1'/>"
    "  <interface name='org.freedesktop.DBus.Properties'>"
    "    <method name='Get'>"
    "      <arg name='interface' type='s' direction='in'/>"
    "      <arg name='name' type='s' direction='in'/>"
    "      <arg name='value' type='v' direction='out'/>"
    "    </method>"
    "    <method name='GetAll'>"
    "      <arg name='interface' type='s' direction='in'/>"
    "      <arg name='props' type='a{sv}' direction='out'/>"
    "    </method>"
    "    <method name='Set'>"
    "      <arg name='interface' type='s' direction='in'/>"
    "      <arg name='name' type='s' direction='in'/>"
    "      <arg name='value' type='v' direction='in'/>"
    "    </method>"
    "  </interface>"
    "</node>";

static const gchar char_xml[] =
    "<node>"
    "  <interface name='org.bluez.GattCharacteristic1'>"
    "    <method name='WriteValue'>"
    "      <arg name='value' type='ay' direction='in'/>"
    "      <arg name='options' type='a{sv}' direction='in'/>"
    "    </method>"
    "  </interface>"
    "  <interface name='org.freedesktop.DBus.Properties'>"
    "    <method name='Get'>"
    "      <arg name='interface' type='s' direction='in'/>"
    "      <arg name='name' type='s' direction='in'/>"
    "      <arg name='value' type='v' direction='out'/>"
    "    </method>"
    "    <method name='GetAll'>"
    "      <arg name='interface' type='s' direction='in'/>"
    "      <arg name='props' type='a{sv}' direction='out'/>"
    "    </method>"
    "    <method name='Set'>"
    "      <arg name='interface' type='s' direction='in'/>"
    "      <arg name='name' type='s' direction='in'/>"
    "      <arg name='value' type='v' direction='in'/>"
    "    </method>"
    "  </interface>"
    "</node>";

static const gchar adv_xml[] =
    "<node>"
    "  <interface name='org.bluez.LEAdvertisement1'>"
    "    <method name='Release'/>"
    "  </interface>"
    "  <interface name='org.freedesktop.DBus.Properties'>"
    "    <method name='Get'>"
    "      <arg name='interface' type='s' direction='in'/>"
    "      <arg name='name' type='s' direction='in'/>"
    "      <arg name='value' type='v' direction='out'/>"
    "    </method>"
    "    <method name='GetAll'>"
    "      <arg name='interface' type='s' direction='in'/>"
    "      <arg name='props' type='a{sv}' direction='out'/>"
    "    </method>"
    "    <method name='Set'>"
    "      <arg name='interface' type='s' direction='in'/>"
    "      <arg name='name' type='s' direction='in'/>"
    "      <arg name='value' type='v' direction='in'/>"
    "    </method>"
    "  </interface>"
    "</node>";

static void signal_start_result(int result)
{
    g_mutex_lock(&g_start_mutex);
    g_start_result = result;
    g_start_done = 1;
    g_cond_signal(&g_start_cond);
    g_mutex_unlock(&g_start_mutex);
}

static void trim_command(char *s)
{
    size_t len;

    while (*s && isspace((unsigned char)*s)) {
        memmove(s, s + 1, strlen(s));
    }

    len = strlen(s);

    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
}

static gpointer ble_command_worker_main(gpointer arg)
{
    (void)arg;

    while (g_command_worker_running) {
        char *command = g_async_queue_pop(g_command_queue);

        if (command == NULL) {
            continue;
        }

        if (strcmp(command, BLE_WORKER_QUIT_CMD) == 0) {
            g_free(command);
            break;
        }

        printf("BLE worker handling: %s\n", command);
        fflush(stdout);

        if (g_command_cb != NULL) {
            int ret = g_command_cb(command);
            if (ret != 0) {
                printf("BLE worker: command failed: %s\n", command);
                fflush(stdout);
            }
        }

        g_free(command);
    }

    return NULL;
}

static void add_service_properties(GVariantBuilder *props)
{
    g_variant_builder_add(props, "{sv}", "UUID",
                          g_variant_new_string(STM32CAR_SERVICE_UUID));

    g_variant_builder_add(props, "{sv}", "Primary",
                          g_variant_new_boolean(TRUE));

    g_variant_builder_add(props, "{sv}", "Includes",
                          g_variant_new_objv(NULL, 0));
}

static void add_char_properties(GVariantBuilder *props)
{
    g_variant_builder_add(props, "{sv}", "UUID",
                          g_variant_new_string(STM32CAR_COMMAND_UUID));

    g_variant_builder_add(props, "{sv}", "Service",
                          g_variant_new_object_path(SERVICE_PATH));

    g_variant_builder_add(props, "{sv}", "Flags",
                          g_variant_new_strv(char_flags, -1));
}

static void add_adv_properties(GVariantBuilder *props)
{
    g_variant_builder_add(props, "{sv}", "Type",
                          g_variant_new_string("peripheral"));

    g_variant_builder_add(props, "{sv}", "ServiceUUIDs",
                          g_variant_new_strv(adv_service_uuids, -1));

    g_variant_builder_add(props, "{sv}", "LocalName",
                          g_variant_new_string("stm32car"));
}

static GVariant *build_service_properties(void)
{
    GVariantBuilder props;

    g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
    add_service_properties(&props);

    return g_variant_builder_end(&props);
}

static GVariant *build_char_properties(void)
{
    GVariantBuilder props;

    g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
    add_char_properties(&props);

    return g_variant_builder_end(&props);
}

static GVariant *build_adv_properties(void)
{
    GVariantBuilder props;

    g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
    add_adv_properties(&props);

    return g_variant_builder_end(&props);
}

static GVariant *get_property_value(const char *object_path,
                                    const char *iface,
                                    const char *name)
{
    if (g_strcmp0(object_path, SERVICE_PATH) == 0 &&
        g_strcmp0(iface, GATT_SERVICE_IFACE) == 0) {

        if (g_strcmp0(name, "UUID") == 0) {
            return g_variant_new_string(STM32CAR_SERVICE_UUID);
        }

        if (g_strcmp0(name, "Primary") == 0) {
            return g_variant_new_boolean(TRUE);
        }

        if (g_strcmp0(name, "Includes") == 0) {
            return g_variant_new_objv(NULL, 0);
        }
    }

    if (g_strcmp0(object_path, CHAR_PATH) == 0 &&
        g_strcmp0(iface, GATT_CHARACTERISTIC_IFACE) == 0) {

        if (g_strcmp0(name, "UUID") == 0) {
            return g_variant_new_string(STM32CAR_COMMAND_UUID);
        }

        if (g_strcmp0(name, "Service") == 0) {
            return g_variant_new_object_path(SERVICE_PATH);
        }

        if (g_strcmp0(name, "Flags") == 0) {
            return g_variant_new_strv(char_flags, -1);
        }
    }

    if (g_strcmp0(object_path, ADV_PATH) == 0 &&
        g_strcmp0(iface, LE_ADV_IFACE) == 0) {

        if (g_strcmp0(name, "Type") == 0) {
            return g_variant_new_string("peripheral");
        }

        if (g_strcmp0(name, "ServiceUUIDs") == 0) {
            return g_variant_new_strv(adv_service_uuids, -1);
        }

        if (g_strcmp0(name, "LocalName") == 0) {
            return g_variant_new_string("stm32car");
        }
    }

    return NULL;
}

static GVariant *build_managed_objects(void)
{
    GVariantBuilder objects;

    g_variant_builder_init(&objects, G_VARIANT_TYPE("a{oa{sa{sv}}}"));

    {
        GVariantBuilder ifaces;
        GVariantBuilder props;

        g_variant_builder_init(&ifaces, G_VARIANT_TYPE("a{sa{sv}}"));
        g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));

        add_service_properties(&props);

        g_variant_builder_add(&ifaces,
                              "{sa{sv}}",
                              GATT_SERVICE_IFACE,
                              &props);

        g_variant_builder_add(&objects,
                              "{oa{sa{sv}}}",
                              SERVICE_PATH,
                              &ifaces);
    }

    {
        GVariantBuilder ifaces;
        GVariantBuilder props;

        g_variant_builder_init(&ifaces, G_VARIANT_TYPE("a{sa{sv}}"));
        g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));

        add_char_properties(&props);

        g_variant_builder_add(&ifaces,
                              "{sa{sv}}",
                              GATT_CHARACTERISTIC_IFACE,
                              &props);

        g_variant_builder_add(&objects,
                              "{oa{sa{sv}}}",
                              CHAR_PATH,
                              &ifaces);
    }

    return g_variant_builder_end(&objects);
}

static void properties_method_call(GDBusMethodInvocation *invocation,
                                   const gchar *object_path,
                                   const gchar *method_name,
                                   GVariant *parameters)
{
    const gchar *iface;
    const gchar *name;

    if (g_strcmp0(method_name, "GetAll") == 0) {
        g_variant_get(parameters, "(&s)", &iface);

        if (g_strcmp0(object_path, SERVICE_PATH) == 0 &&
            g_strcmp0(iface, GATT_SERVICE_IFACE) == 0) {

            g_dbus_method_invocation_return_value(
                invocation,
                g_variant_new("(@a{sv})", build_service_properties()));
            return;
        }

        if (g_strcmp0(object_path, CHAR_PATH) == 0 &&
            g_strcmp0(iface, GATT_CHARACTERISTIC_IFACE) == 0) {

            g_dbus_method_invocation_return_value(
                invocation,
                g_variant_new("(@a{sv})", build_char_properties()));
            return;
        }

        if (g_strcmp0(object_path, ADV_PATH) == 0 &&
            g_strcmp0(iface, LE_ADV_IFACE) == 0) {

            g_dbus_method_invocation_return_value(
                invocation,
                g_variant_new("(@a{sv})", build_adv_properties()));
            return;
        }

        g_dbus_method_invocation_return_value(
            invocation,
            g_variant_new("(a{sv})", NULL));
        return;
    }

    if (g_strcmp0(method_name, "Get") == 0) {
        GVariant *value;

        g_variant_get(parameters, "(&s&s)", &iface, &name);

        value = get_property_value(object_path, iface, name);
        if (value == NULL) {
            g_dbus_method_invocation_return_dbus_error(
                invocation,
                "org.stm32car.Error.UnknownProperty",
                "Unknown property");
            return;
        }

        g_dbus_method_invocation_return_value(
            invocation,
            g_variant_new("(v)", value));
        return;
    }

    if (g_strcmp0(method_name, "Set") == 0) {
        g_dbus_method_invocation_return_dbus_error(
            invocation,
            "org.stm32car.Error.NotSupported",
            "Set is not supported");
        return;
    }

    g_dbus_method_invocation_return_dbus_error(
        invocation,
        "org.stm32car.Error.UnknownMethod",
        "Unknown Properties method");
}

static void object_manager_method_call(GDBusConnection *connection,
                                       const gchar *sender,
                                       const gchar *object_path,
                                       const gchar *interface_name,
                                       const gchar *method_name,
                                       GVariant *parameters,
                                       GDBusMethodInvocation *invocation,
                                       gpointer user_data)
{
    (void)connection;
    (void)sender;
    (void)object_path;
    (void)interface_name;
    (void)parameters;
    (void)user_data;

    if (g_strcmp0(method_name, "GetManagedObjects") == 0) {
        GVariant *objects;
        gchar *dump;

        printf("BLE: GetManagedObjects called\n");

        objects = build_managed_objects();

        dump = g_variant_print(objects, TRUE);
        printf("BLE: ManagedObjects = %s\n", dump);
        g_free(dump);

        fflush(stdout);

        g_dbus_method_invocation_return_value(
            invocation,
            g_variant_new("(@a{oa{sa{sv}}})", objects));
        return;
    }

    g_dbus_method_invocation_return_dbus_error(
        invocation,
        "org.stm32car.Error.UnknownMethod",
        "Unknown ObjectManager method");
}

static void service_method_call(GDBusConnection *connection,
                                const gchar *sender,
                                const gchar *object_path,
                                const gchar *interface_name,
                                const gchar *method_name,
                                GVariant *parameters,
                                GDBusMethodInvocation *invocation,
                                gpointer user_data)
{
    (void)connection;
    (void)sender;
    (void)user_data;

    if (g_strcmp0(interface_name, DBUS_PROP_IFACE) == 0) {
        properties_method_call(invocation, object_path, method_name, parameters);
        return;
    }

    g_dbus_method_invocation_return_dbus_error(
        invocation,
        "org.stm32car.Error.UnknownMethod",
        "Unknown service method");
}

static void char_method_call(GDBusConnection *connection,
                             const gchar *sender,
                             const gchar *object_path,
                             const gchar *interface_name,
                             const gchar *method_name,
                             GVariant *parameters,
                             GDBusMethodInvocation *invocation,
                             gpointer user_data)
{
    (void)connection;
    (void)sender;
    (void)user_data;

    if (g_strcmp0(interface_name, DBUS_PROP_IFACE) == 0) {
        properties_method_call(invocation, object_path, method_name, parameters);
        return;
    }

    if (g_strcmp0(interface_name, GATT_CHARACTERISTIC_IFACE) == 0 &&
        g_strcmp0(method_name, "WriteValue") == 0) {

        GVariant *value;
        GVariant *options;
        const guint8 *bytes;
        gsize len;
        char command[64];

        g_variant_get(parameters, "(@ay@a{sv})", &value, &options);

        bytes = g_variant_get_fixed_array(value, &len, sizeof(guint8));

        if (len == 0 || len >= sizeof(command)) {
            g_variant_unref(value);
            g_variant_unref(options);

            g_dbus_method_invocation_return_dbus_error(
                invocation,
                "org.stm32car.Error.InvalidCommand",
                "Invalid BLE command length");
            return;
        }

        memcpy(command, bytes, len);
        command[len] = '\0';
        trim_command(command);

        printf("BLE RX: %s\n", command);
        fflush(stdout);

        if (g_command_queue != NULL) {
            g_async_queue_push(g_command_queue, g_strdup(command));
        }

        g_variant_unref(value);
        g_variant_unref(options);

        /*
         * Important:
         * Return success to BlueZ immediately.
         * Do not wait for M33 RPMsg ACK inside this D-Bus WriteValue handler.
         */
        g_dbus_method_invocation_return_value(invocation, NULL);
        return;
    }

    g_dbus_method_invocation_return_dbus_error(
        invocation,
        "org.stm32car.Error.UnknownMethod",
        "Unknown characteristic method");
}

static void adv_method_call(GDBusConnection *connection,
                            const gchar *sender,
                            const gchar *object_path,
                            const gchar *interface_name,
                            const gchar *method_name,
                            GVariant *parameters,
                            GDBusMethodInvocation *invocation,
                            gpointer user_data)
{
    (void)connection;
    (void)sender;
    (void)parameters;
    (void)user_data;

    if (g_strcmp0(interface_name, DBUS_PROP_IFACE) == 0) {
        properties_method_call(invocation, object_path, method_name, parameters);
        return;
    }

    if (g_strcmp0(interface_name, LE_ADV_IFACE) == 0 &&
        g_strcmp0(method_name, "Release") == 0) {

        printf("BLE: advertisement released by BlueZ\n");
        fflush(stdout);

        g_dbus_method_invocation_return_value(invocation, NULL);
        return;
    }

    g_dbus_method_invocation_return_dbus_error(
        invocation,
        "org.stm32car.Error.UnknownMethod",
        "Unknown advertisement method");
}

static const GDBusInterfaceVTable object_manager_vtable = {
    .method_call = object_manager_method_call,
};

static const GDBusInterfaceVTable service_vtable = {
    .method_call = service_method_call,
};

static const GDBusInterfaceVTable char_vtable = {
    .method_call = char_method_call,
};

static const GDBusInterfaceVTable adv_vtable = {
    .method_call = adv_method_call,
};

static int export_interface(const gchar *object_path,
                            const gchar *xml,
                            guint interface_index,
                            const GDBusInterfaceVTable *vtable,
                            guint *registration_id,
                            GError **error)
{
    GDBusNodeInfo *node_info;

    node_info = g_dbus_node_info_new_for_xml(xml, error);
    if (node_info == NULL) {
        return -1;
    }

    *registration_id = g_dbus_connection_register_object(
        g_conn,
        object_path,
        node_info->interfaces[interface_index],
        vtable,
        NULL,
        NULL,
        error);

    g_dbus_node_info_unref(node_info);

    if (*registration_id == 0) {
        return -1;
    }

    return 0;
}

static int export_objects(GError **error)
{
    if (export_interface(APP_PATH,
                         object_manager_xml,
                         0,
                         &object_manager_vtable,
                         &g_obj_manager_id,
                         error) != 0) {
        return -1;
    }

    if (export_interface(SERVICE_PATH,
                         service_xml,
                         0,
                         &service_vtable,
                         &g_service_id,
                         error) != 0) {
        return -1;
    }

    if (export_interface(SERVICE_PATH,
                         service_xml,
                         1,
                         &service_vtable,
                         &g_service_prop_id,
                         error) != 0) {
        return -1;
    }

    if (export_interface(CHAR_PATH,
                         char_xml,
                         0,
                         &char_vtable,
                         &g_char_id,
                         error) != 0) {
        return -1;
    }

    if (export_interface(CHAR_PATH,
                         char_xml,
                         1,
                         &char_vtable,
                         &g_char_prop_id,
                         error) != 0) {
        return -1;
    }

    if (export_interface(ADV_PATH,
                         adv_xml,
                         0,
                         &adv_vtable,
                         &g_adv_id,
                         error) != 0) {
        return -1;
    }

    if (export_interface(ADV_PATH,
                         adv_xml,
                         1,
                         &adv_vtable,
                         &g_adv_prop_id,
                         error) != 0) {
        return -1;
    }

    return 0;
}

static void unregister_local_objects(void)
{
    if (g_conn == NULL) {
        return;
    }

    if (g_adv_prop_id != 0) {
        g_dbus_connection_unregister_object(g_conn, g_adv_prop_id);
        g_adv_prop_id = 0;
    }

    if (g_adv_id != 0) {
        g_dbus_connection_unregister_object(g_conn, g_adv_id);
        g_adv_id = 0;
    }

    if (g_char_prop_id != 0) {
        g_dbus_connection_unregister_object(g_conn, g_char_prop_id);
        g_char_prop_id = 0;
    }

    if (g_char_id != 0) {
        g_dbus_connection_unregister_object(g_conn, g_char_id);
        g_char_id = 0;
    }

    if (g_service_prop_id != 0) {
        g_dbus_connection_unregister_object(g_conn, g_service_prop_id);
        g_service_prop_id = 0;
    }

    if (g_service_id != 0) {
        g_dbus_connection_unregister_object(g_conn, g_service_id);
        g_service_id = 0;
    }

    if (g_obj_manager_id != 0) {
        g_dbus_connection_unregister_object(g_conn, g_obj_manager_id);
        g_obj_manager_id = 0;
    }
}

static void advertisement_registered_cb(GObject *source_object,
                                        GAsyncResult *res,
                                        gpointer user_data)
{
    GError *error = NULL;
    GVariant *reply;

    (void)user_data;

    reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source_object),
                                          res,
                                          &error);
    if (reply == NULL) {
        fprintf(stderr, "BLE: RegisterAdvertisement failed: %s\n",
                error ? error->message : "unknown error");
        g_clear_error(&error);

        signal_start_result(-1);

        if (g_loop != NULL) {
            g_main_loop_quit(g_loop);
        }

        return;
    }

    g_variant_unref(reply);

    printf("BLE GATT server started\n");
    printf("  Name: stm32car\n");
    printf("  Service: %s\n", STM32CAR_SERVICE_UUID);
    printf("  Command characteristic: %s\n", STM32CAR_COMMAND_UUID);
    fflush(stdout);

    signal_start_result(0);
}

static void gatt_application_registered_cb(GObject *source_object,
                                           GAsyncResult *res,
                                           gpointer user_data)
{
    GError *error = NULL;
    GVariant *reply;
    GVariantBuilder options;

    (void)user_data;

    reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source_object),
                                          res,
                                          &error);
    if (reply == NULL) {
        fprintf(stderr, "BLE: RegisterApplication failed: %s\n",
                error ? error->message : "unknown error");
        g_clear_error(&error);

        signal_start_result(-1);

        if (g_loop != NULL) {
            g_main_loop_quit(g_loop);
        }

        return;
    }

    g_variant_unref(reply);

    printf("BLE: GATT application registered\n");
    fflush(stdout);

    g_variant_builder_init(&options, G_VARIANT_TYPE("a{sv}"));

    g_dbus_connection_call(g_conn,
                           BLUEZ_BUS_NAME,
                           BLUEZ_ADAPTER_PATH,
                           LE_ADV_MANAGER_IFACE,
                           "RegisterAdvertisement",
                           g_variant_new("(oa{sv})", ADV_PATH, &options),
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           advertisement_registered_cb,
                           NULL);
}

static void register_gatt_application_async(void)
{
    GVariantBuilder options;

    g_variant_builder_init(&options, G_VARIANT_TYPE("a{sv}"));

    g_dbus_connection_call(g_conn,
                           BLUEZ_BUS_NAME,
                           BLUEZ_ADAPTER_PATH,
                           GATT_MANAGER_IFACE,
                           "RegisterApplication",
                           g_variant_new("(oa{sv})", APP_PATH, &options),
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           gatt_application_registered_cb,
                           NULL);
}

static gpointer ble_thread_main(gpointer arg)
{
    GError *error = NULL;

    (void)arg;

    g_conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (g_conn == NULL) {
        fprintf(stderr, "BLE: failed to connect to system bus: %s\n",
                error ? error->message : "unknown error");
        g_clear_error(&error);

        signal_start_result(-1);
        return NULL;
    }

    if (export_objects(&error) != 0) {
        fprintf(stderr, "BLE: failed to export D-Bus objects: %s\n",
                error ? error->message : "unknown error");
        g_clear_error(&error);

        signal_start_result(-1);
        return NULL;
    }

    /*
     * Important:
     * Start the GLib main loop before BlueZ needs to call back into us.
     * RegisterApplication is async to avoid D-Bus deadlock.
     */
    g_loop = g_main_loop_new(NULL, FALSE);

    register_gatt_application_async();

    g_main_loop_run(g_loop);

    unregister_local_objects();

    return NULL;
}

int ble_gatt_server_start(ble_command_callback_t callback)
{
    int result;

    g_command_cb = callback;

    g_command_queue = g_async_queue_new();
    if (g_command_queue == NULL) {
        return -1;
    }

    g_command_worker_running = 1;

    g_command_worker_thread = g_thread_new("stm32car-ble-cmd-worker",
                                           ble_command_worker_main,
                                           NULL);
    if (g_command_worker_thread == NULL) {
        g_command_worker_running = 0;
        g_async_queue_unref(g_command_queue);
        g_command_queue = NULL;
        return -1;
    }

    g_mutex_lock(&g_start_mutex);
    g_start_done = 0;
    g_start_result = -1;
    g_mutex_unlock(&g_start_mutex);

    g_ble_thread = g_thread_new("stm32car-ble", ble_thread_main, NULL);
    if (g_ble_thread == NULL) {
        g_command_worker_running = 0;
        g_async_queue_push(g_command_queue, g_strdup(BLE_WORKER_QUIT_CMD));
        g_thread_join(g_command_worker_thread);
        g_command_worker_thread = NULL;
        g_async_queue_unref(g_command_queue);
        g_command_queue = NULL;
        return -1;
    }

    g_mutex_lock(&g_start_mutex);
    while (!g_start_done) {
        g_cond_wait(&g_start_cond, &g_start_mutex);
    }

    result = g_start_result;
    g_mutex_unlock(&g_start_mutex);

    if (result != 0) {
        if (g_ble_thread != NULL) {
            g_thread_join(g_ble_thread);
            g_ble_thread = NULL;
        }

        g_command_worker_running = 0;

        if (g_command_queue != NULL) {
            g_async_queue_push(g_command_queue, g_strdup(BLE_WORKER_QUIT_CMD));
        }

        if (g_command_worker_thread != NULL) {
            g_thread_join(g_command_worker_thread);
            g_command_worker_thread = NULL;
        }

        if (g_command_queue != NULL) {
            g_async_queue_unref(g_command_queue);
            g_command_queue = NULL;
        }
    }

    return result;
}

void ble_gatt_server_stop(void)
{
    GError *error = NULL;

    if (g_conn != NULL) {
        g_dbus_connection_call_sync(
            g_conn,
            BLUEZ_BUS_NAME,
            BLUEZ_ADAPTER_PATH,
            LE_ADV_MANAGER_IFACE,
            "UnregisterAdvertisement",
            g_variant_new("(o)", ADV_PATH),
            NULL,
            G_DBUS_CALL_FLAGS_NONE,
            3000,
            NULL,
            &error);
        g_clear_error(&error);

        g_dbus_connection_call_sync(
            g_conn,
            BLUEZ_BUS_NAME,
            BLUEZ_ADAPTER_PATH,
            GATT_MANAGER_IFACE,
            "UnregisterApplication",
            g_variant_new("(o)", APP_PATH),
            NULL,
            G_DBUS_CALL_FLAGS_NONE,
            3000,
            NULL,
            &error);
        g_clear_error(&error);
    }

    if (g_loop != NULL) {
        g_main_loop_quit(g_loop);
    }

    if (g_ble_thread != NULL) {
        g_thread_join(g_ble_thread);
        g_ble_thread = NULL;
    }

    if (g_loop != NULL) {
        g_main_loop_unref(g_loop);
        g_loop = NULL;
    }

    if (g_conn != NULL) {
        g_object_unref(g_conn);
        g_conn = NULL;
    }

    g_command_worker_running = 0;

    if (g_command_queue != NULL) {
        g_async_queue_push(g_command_queue, g_strdup(BLE_WORKER_QUIT_CMD));
    }

    if (g_command_worker_thread != NULL) {
        g_thread_join(g_command_worker_thread);
        g_command_worker_thread = NULL;
    }

    if (g_command_queue != NULL) {
        g_async_queue_unref(g_command_queue);
        g_command_queue = NULL;
    }
}