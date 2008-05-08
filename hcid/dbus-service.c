/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2006-2007  Nokia Corporation
 *  Copyright (C) 2004-2008  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/sdp.h>

#include <glib.h>

#include <dbus/dbus.h>

#include "dbus.h"
#include "dbus-helper.h"
#include "hcid.h"
#include "server.h"
#include "dbus-common.h"
#include "dbus-error.h"
#include "error.h"
#include "manager.h"
#include "adapter.h"
#include "agent.h"
#include "device.h"
#include "dbus-service.h"
#include "dbus-hci.h"

#define SERVICE_INTERFACE "org.bluez.Service"

struct service_uuids {
	char *name;
	char **uuids;
};

struct service_auth {
	service_auth_cb cb;
	void *user_data;
};

static GSList *services = NULL;
static GSList *services_uuids = NULL;

static void service_free(struct service *service)
{
	if (!service)
		return;

	g_free(service->object_path);
	g_free(service->ident);
	g_free(service->name);

	g_free(service);
}

static DBusHandlerResult get_info(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct service *service = data;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
			DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
			DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING
			DBUS_DICT_ENTRY_END_CHAR_AS_STRING, &dict);

	dbus_message_iter_append_dict_entry(&dict, "identifier",
			DBUS_TYPE_STRING, &service->ident);

	dbus_message_iter_append_dict_entry(&dict, "name",
			DBUS_TYPE_STRING, &service->name);

	dbus_message_iter_close_container(&iter, &dict);

	return send_message_and_unref(conn, reply);
}

static DBusHandlerResult get_identifier(DBusConnection *conn,
					DBusMessage *msg, void *data)
{

	struct service *service = data;
	DBusMessage *reply;
	const char *identifier = "";

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	if (service->ident)
		identifier = service->ident;

	dbus_message_append_args(reply, DBUS_TYPE_STRING, &identifier,
							DBUS_TYPE_INVALID);

	return send_message_and_unref(conn, reply);
}

static DBusHandlerResult get_name(DBusConnection *conn,
					DBusMessage *msg, void *data)
{

	struct service *service = data;
	DBusMessage *reply;
	const char *name = "";

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	if (service->name)
		name = service->name;

	dbus_message_append_args(reply, DBUS_TYPE_STRING, &name,
							DBUS_TYPE_INVALID);

	return send_message_and_unref(conn, reply);
}

static DBusHandlerResult get_description(DBusConnection *conn,
						DBusMessage *msg, void *data)
{
	DBusMessage *reply;
	const char *description = "";

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	dbus_message_append_args(reply, DBUS_TYPE_STRING, &description,
							DBUS_TYPE_INVALID);

	return send_message_and_unref(conn, reply);
}

static DBusHandlerResult get_bus_name(DBusConnection *conn,
						DBusMessage *msg, void *data)
{
	DBusMessage *reply;
	const char *busname = "org.bluez";

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	dbus_message_append_args(reply, DBUS_TYPE_STRING, &busname,
							DBUS_TYPE_INVALID);

	return send_message_and_unref(conn, reply);
}

static DBusHandlerResult start(DBusConnection *conn,
				DBusMessage *msg, void *data)
{
	return error_failed_errno(conn, msg, EALREADY);
}

static DBusHandlerResult stop(DBusConnection *conn,
				DBusMessage *msg, void *data)
{
	return error_failed_errno(conn, msg, EPERM);
}

static DBusHandlerResult is_running(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	DBusMessage *reply;
	dbus_bool_t running = TRUE;

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	dbus_message_append_args(reply,
			DBUS_TYPE_BOOLEAN, &running,
			DBUS_TYPE_INVALID);

	return send_message_and_unref(conn, reply);
}

static DBusHandlerResult is_external(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	DBusMessage *reply;
	dbus_bool_t external = TRUE;

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	dbus_message_append_args(reply,
			DBUS_TYPE_BOOLEAN, &external,
			DBUS_TYPE_INVALID);

	return send_message_and_unref(conn, reply);
}

static DBusHandlerResult set_trusted(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct service *service = data;
	DBusMessage *reply;
	const char *address;

	if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID))
		return error_invalid_arguments(conn, msg, NULL);

	if (check_address(address) < 0)
		return error_invalid_arguments(conn, msg, NULL);

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	write_trust(BDADDR_ANY, address, service->ident, TRUE);

	dbus_connection_emit_signal(conn, service->object_path,
					SERVICE_INTERFACE, "TrustAdded",
					DBUS_TYPE_STRING, &address,
					DBUS_TYPE_INVALID);

	return send_message_and_unref(conn, reply);
}

static DBusHandlerResult list_trusted(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct service *service = data;
	DBusMessage *reply;
	GSList *trusts, *l;
	char **addrs;
	int len;

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	trusts = list_trusts(BDADDR_ANY, service->ident);

	addrs = g_new(char *, g_slist_length(trusts));

	for (l = trusts, len = 0; l; l = l->next, len++)
		addrs[len] = l->data;

	dbus_message_append_args(reply,
			DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
			&addrs, len, DBUS_TYPE_INVALID);

	g_free(addrs);
	g_slist_foreach(trusts, (GFunc) g_free, NULL);
	g_slist_free(trusts);

	return send_message_and_unref(conn, reply);
}

static DBusHandlerResult is_trusted(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct service *service = data;
	DBusMessage *reply;
	const char *address;
	dbus_bool_t trusted;

	if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID))
		return error_invalid_arguments(conn, msg, NULL);

	if (check_address(address) < 0)
		return error_invalid_arguments(conn, msg, NULL);

	trusted = read_trust(BDADDR_ANY, address, service->ident);

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	dbus_message_append_args(reply,
				DBUS_TYPE_BOOLEAN, &trusted,
				DBUS_TYPE_INVALID);

	return send_message_and_unref(conn, reply);
}

static DBusHandlerResult remove_trust(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct service *service = data;
	DBusMessage *reply;
	const char *address;

	if (!dbus_message_get_args(msg, NULL,
			DBUS_TYPE_STRING, &address,
			DBUS_TYPE_INVALID))
		return error_invalid_arguments(conn, msg, NULL);

	if (check_address(address) < 0)
		return error_invalid_arguments(conn, msg, NULL);

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	write_trust(BDADDR_ANY, address, service->ident, FALSE);

	dbus_connection_emit_signal(conn, service->object_path,
					SERVICE_INTERFACE, "TrustRemoved",
					DBUS_TYPE_STRING, &address,
					DBUS_TYPE_INVALID);

	return send_message_and_unref(conn, reply);
}

static DBusMethodVTable service_methods[] = {
	{ "GetInfo",		get_info,		"",	"a{sv}"	},
	{ "GetIdentifier",	get_identifier,		"",	"s"	},
	{ "GetName",		get_name,		"",	"s"	},
	{ "GetDescription",	get_description,	"",	"s"	},
	{ "GetBusName",		get_bus_name,		"",	"s"	},
	{ "Start",		start,			"",	""	},
	{ "Stop",		stop,			"",	""	},
	{ "IsRunning",		is_running,		"",	"b"	},
	{ "IsExternal",		is_external,		"",	"b"	},
	{ "SetTrusted",		set_trusted,		"s",	""	},
	{ "IsTrusted",		is_trusted,		"s",	"b"	},
	{ "RemoveTrust",	remove_trust,		"s",	""	},
	{ "ListTrusts",		list_trusted,		"",	"as"	},
	{ NULL, NULL, NULL, NULL }
};

static DBusSignalVTable service_signals[] = {
	{ "Started",		""	},
	{ "Stopped",		""	},
	{ "TrustAdded",		"s"	},
	{ "TrustRemoved",	"s"	},
	{ NULL, NULL }
};

static dbus_bool_t service_init(DBusConnection *conn, const char *path)
{
	return dbus_connection_register_interface(conn, path, SERVICE_INTERFACE,
							service_methods,
							service_signals, NULL);
}

static int service_cmp_path(struct service *service, const char *path)
{
	return strcmp(service->object_path, path);
}

static int service_cmp_ident(struct service *service, const char *ident)
{
	return strcmp(service->ident, ident);
}

static int unregister_service_for_connection(DBusConnection *connection,
						struct service *service)
{
	DBusConnection *conn = get_dbus_connection();

	debug("Unregistering service object: %s", service->object_path);

	if (!conn)
		goto cleanup;

	dbus_connection_emit_signal(conn, service->object_path,
					SERVICE_INTERFACE,
					"Stopped", DBUS_TYPE_INVALID);

	dbus_connection_emit_signal(conn, BASE_PATH, MANAGER_INTERFACE,
					"ServiceRemoved",
					DBUS_TYPE_STRING, &service->object_path,
					DBUS_TYPE_INVALID);

	if (!dbus_connection_destroy_object_path(conn, service->object_path)) {
		error("D-Bus failed to unregister %s object",
						service->object_path);
		return -1;
	}

cleanup:
	services = g_slist_remove(services, service);
	service_free(service);

	return 0;
}

static int do_unregister(struct service *service)
{
	DBusConnection *conn = get_dbus_connection();

	return unregister_service_for_connection(conn, service);
}

void release_services(DBusConnection *conn)
{
	debug("release_services");

	g_slist_foreach(services, (GFunc) do_unregister, NULL);
	g_slist_free(services);
	services = NULL;
}

struct service *search_service(const char *pattern)
{
	GSList *l;
	const char *bus_id;

	/* Workaround for plugins: share the same bus id */
	bus_id = dbus_bus_get_unique_name(get_dbus_connection());
	if (!strcmp(bus_id, pattern))
		return NULL;

	for (l = services; l != NULL; l = l->next) {
		struct service *service = l->data;

		if (service->ident && !strcmp(service->ident, pattern))
			return service;
	}

	return NULL;
}

void append_available_services(DBusMessageIter *array_iter)
{
	GSList *l;

	for (l = services; l != NULL; l = l->next) {
		struct service *service = l->data;

		dbus_message_iter_append_basic(array_iter,
					DBUS_TYPE_STRING, &service->object_path);
	}
}

int service_unregister(DBusConnection *conn, struct service *service)
{
	return unregister_service_for_connection(conn, service);
}

static gint name_cmp(struct service_uuids *su, const char *name)
{
	return strcmp(su->name, name);
}

static gint uuid_cmp(struct service_uuids *su, const char *uuid)
{
	int i;

	for (i = 0; su->uuids[i]; i++) {
		if (!strcasecmp(su->uuids[i], uuid))
			return 0;
	}

	return -1;
}

struct service *search_service_by_uuid(const char *uuid)
{
	struct service_uuids *su;
	struct service *service;
	GSList *l;

	if (!services_uuids)
		return NULL;

	l = g_slist_find_custom(services_uuids, uuid, (GCompareFunc) uuid_cmp);
	if (!l)
		return NULL;

	su = l->data;
	service = search_service(su->name);
	if (!service)
		return NULL;

	return service;
}

static void register_uuids(const char *ident, const char **uuids)
{
	struct service_uuids *su;
	int i;

	if (!ident)
		return;

	su = g_new0(struct service_uuids, 1);
	su->name = g_strdup(ident);

	for (i = 0; uuids[i]; i++);

	su->uuids = g_new0(char *, i + 1);

	for (i = 0; uuids[i]; i++)
		su->uuids[i] = g_strdup(uuids[i]);

	services_uuids = g_slist_append(services_uuids, su);
}

static void service_uuids_free(struct service_uuids *su)
{
	int i;

	if (!su)
		return;

	g_free(su->name);

	for (i = 0; su->uuids[i]; i++)
		g_free(su->uuids[i]);

	g_free(su);
}

static void unregister_uuids(const char *ident)
{
	struct service_uuids *su;
	GSList *l;

	if (!services_uuids)
		return;

	l = g_slist_find_custom(services_uuids, ident, (GCompareFunc) name_cmp);
	if (!l)
		return;

	su = l->data;
	services_uuids = g_slist_remove(services_uuids, su);

	service_uuids_free(su);
}

static struct service *create_external_service(const char *ident)
{
	struct service *service;
	const char *name;

	service = g_try_new0(struct service, 1);
	if (!service) {
		error("OOM while allocating new external service");
		return NULL;
	}

	if (!strcmp(ident, "input"))
		name = "Input service";
	else if (!strcmp(ident, "audio"))
		name = "Audio service";
	else if (!strcmp(ident, "network"))
		name = "Network service";
	else if (!strcmp(ident, "serial"))
		name = "Serial service";
	else
		name = "";

	service->ident = g_strdup(ident);
	service->name = g_strdup(name);

	return service;
}

int register_service(const char *ident, const char **uuids)
{
	DBusConnection *conn = get_dbus_connection();
	struct service *service;
	char obj_path[PATH_MAX];
	int i;

	if (g_slist_find_custom(services, ident,
					(GCompareFunc) service_cmp_ident))
		return -EADDRINUSE;

	snprintf(obj_path, sizeof(obj_path) - 1,
				"/org/bluez/service_%s", ident);

	/* Make the path valid for D-Bus */
	for (i = strlen("/org/bluez/"); obj_path[i]; i++) {
		if (!isalnum(obj_path[i]))
			obj_path[i] = '_';
	}

	if (g_slist_find_custom(services, obj_path,
				(GCompareFunc) service_cmp_path))
		return -EADDRINUSE;

	service = create_external_service(ident);

	debug("Registering service object: %s (%s)",
					service->ident, obj_path);

	if (!dbus_connection_create_object_path(conn, obj_path,
							service, NULL)) {
		error("D-Bus failed to register %s object", obj_path);
		return -1;
	}

	if (!service_init(conn, obj_path)) {
		error("Service init failed");
		return -1;
	}

	service->object_path = g_strdup(obj_path);

	services = g_slist_append(services, service);

	if (uuids)
		register_uuids(ident, uuids);

	dbus_connection_emit_signal(conn, BASE_PATH, MANAGER_INTERFACE,
				"ServiceAdded",
				DBUS_TYPE_STRING, &service->object_path,
				DBUS_TYPE_INVALID);

	dbus_connection_emit_signal(conn, service->object_path,
					SERVICE_INTERFACE,
					"Started", DBUS_TYPE_INVALID);

	return 0;
}

void unregister_service(const char *ident)
{
	unregister_uuids(ident);
}

static struct adapter *ba2adapter(bdaddr_t *src)
{
	DBusConnection *conn = get_dbus_connection();
	struct adapter *adapter = NULL;
	char address[18], path[6];
	int dev_id;

	ba2str(src, address);
	dev_id = hci_devid(address);
	if (dev_id < 0)
		return NULL;

	/* FIXME: id2adapter? Create a list of adapters? */
	snprintf(path, sizeof(path), "/hci%d", dev_id);
	if (dbus_connection_get_object_user_data(conn,
			path, (void *) &adapter) == FALSE)
		return NULL;

	return adapter;
}

static void agent_auth_cb(struct agent *agent, DBusError *derr, void *user_data)
{
	struct service_auth *auth = user_data;

	auth->cb(derr, auth->user_data);

	g_free(auth);
}

int service_req_auth(bdaddr_t *src, bdaddr_t *dst,
		const char *uuid, service_auth_cb cb, void *user_data)
{
	struct service_auth *auth;
	struct adapter *adapter;
	struct device *device;
	struct agent *agent;
	struct service *service;
	char address[18];
	gboolean trusted;

	adapter = ba2adapter(src);
	if (!adapter)
		return -EPERM;

	/* Device connected? */
	if (!g_slist_find_custom(adapter->active_conn,
				dst, active_conn_find_by_bdaddr))
		return -ENOTCONN;

	ba2str(dst, address);
	device = adapter_find_device(adapter, address);
	if (!device)
		return -EPERM;

	service = search_service_by_uuid(uuid);
	if (!service)
		return -EPERM;

	trusted = read_trust(src, address, GLOBAL_TRUST);
	if (!trusted)
		trusted = read_trust(BDADDR_ANY, address, service->ident);

	if (trusted) {
		cb(NULL, user_data);
		return 0;
	}

	agent = (device->agent ? : adapter->agent);
	if (!agent)
		return -EPERM;

	auth = g_try_new0(struct service_auth, 1);
	if (!auth)
		return -ENOMEM;

	auth->cb = cb;
	auth->user_data = user_data;

	return agent_authorize(agent, device->path, uuid, agent_auth_cb, auth);
}

int service_cancel_auth(bdaddr_t *src)
{
	struct adapter *adapter = ba2adapter(src);
	struct device *device;
	struct agent *agent;
	char address[18];

	if (!adapter)
		return -EPERM;

	ba2str(src, address);
	device = adapter_find_device(adapter, address);
	if (!device)
		return -EPERM;

	/*
	 * FIXME: Cancel fails if authorization is requested to adapter's
	 * agent and in the meanwhile CreatePairedDevice is called.
	 */

	agent = (device->agent ? : adapter->agent);
	if (!agent)
		return -EPERM;

	return agent_cancel(agent);
}
