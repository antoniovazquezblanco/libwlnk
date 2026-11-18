/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * nm.c
 * NetworkManager's hold on interfaces: its device's Managed property, over
 * sd-bus.
 *
 * NetworkManager is never started to be asked: one that is not running
 * manages nothing. Its policy (polkit) authorizes changes, so a refusal
 * arrives as a D-Bus error, not as EPERM.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <systemd/sd-bus.h>
#include "err.h"

#include "err_internal.h"
#include "managers/nm.h"

/** NetworkManager's bus name, also the interface of its main object. */
#define NM_NAME     "org.freedesktop.NetworkManager"
/** Its main object. */
#define NM_PATH     "/org/freedesktop/NetworkManager"
/** The interface of its device objects. */
#define NM_DEVICE   NM_NAME ".Device"
/** Size of a device object path kept, including the terminating NUL. */
#define NM_PATH_MAX 128

/**
 * @brief Record a failed call to NetworkManager.
 * @param e Its D-Bus error.
 * @param brc Its sd-bus return, a negative errno.
 * @param step What was asked.
 * @param ifname Of which interface.
 * @return WLNK_E_PERM when its policy refused, else WLNK_E_IO.
 */
static int bus_fail(const sd_bus_error *e, int brc, const char *step,
                    const char *ifname)
{
    bool denied = sd_bus_error_has_name(e, SD_BUS_ERROR_ACCESS_DENIED) ||
                  sd_bus_error_has_name(e, SD_BUS_ERROR_AUTH_FAILED) ||
                  sd_bus_error_has_name(e, NM_NAME ".PermissionDenied");
    int status = denied ? WLNK_E_PERM : WLNK_E_IO;
    char msg[WLNK_ERR_MSG_MAX];
    if (e->message == NULL) {
        snprintf(msg, sizeof(msg), "NetworkManager: %s %s", step, ifname);
        return wlnk_fail_sys(status, -brc, msg);
    }
    snprintf(msg, sizeof(msg), "NetworkManager: %s %s: %s", step, ifname,
             e->message);
    return wlnk_fail(status, -brc, msg);
}

/**
 * @brief Find NetworkManager's device for an interface.
 * @param ifname Interface name.
 * @param bus Receives the system bus connection, NULL when there is none;
 * the caller unreferences it, also on failure.
 * @param path Receives the device's object path, NM_PATH_MAX long; "" when
 * NetworkManager is not running or does not know the interface.
 * @return WLNK_OK, or the failure.
 */
static int find_device(const char *ifname, sd_bus **bus, char *path)
{
    *bus = NULL;
    path[0] = '\0';
    if (ifname == NULL || ifname[0] == '\0')
        return wlnk_fail(WLNK_E_INVALID_ARG, 0,
                         "NetworkManager: no interface name");

    int brc = sd_bus_open_system(bus);
    if (brc == -ENOENT || brc == -ECONNREFUSED)
        return WLNK_OK; /* no system bus, so no NetworkManager on it */
    if (brc < 0)
        return wlnk_fail_sys(wlnk_errno_status(-brc), -brc,
                             "NetworkManager: system bus");

    sd_bus_message *req = NULL;
    sd_bus_message *reply = NULL;
    sd_bus_error e = SD_BUS_ERROR_NULL;
    brc = sd_bus_message_new_method_call(*bus, &req, NM_NAME, NM_PATH, NM_NAME,
                                         "GetDeviceByIpIface");
    if (brc >= 0)
        brc = sd_bus_message_append(req, "s", ifname);
    if (brc >= 0)
        brc = sd_bus_message_set_auto_start(req, 0);
    if (brc >= 0)
        brc = sd_bus_call(*bus, req, 0, &e, &reply);
    const char *found = NULL;
    if (brc >= 0)
        brc = sd_bus_message_read(reply, "o", &found);

    int rc = WLNK_OK;
    if (brc < 0) {
        /* Not running, or not knowing the interface: nothing managed. */
        if (!sd_bus_error_has_name(&e, SD_BUS_ERROR_SERVICE_UNKNOWN) &&
            !sd_bus_error_has_name(&e, SD_BUS_ERROR_NAME_HAS_NO_OWNER) &&
            !sd_bus_error_has_name(&e, NM_NAME ".UnknownDevice"))
            rc = bus_fail(&e, brc, "find", ifname);
    } else if (snprintf(path, NM_PATH_MAX, "%s", found) >= NM_PATH_MAX) {
        path[0] = '\0';
        rc = wlnk_fail(WLNK_E_IO, 0, "NetworkManager: device path too long");
    }
    sd_bus_error_free(&e);
    sd_bus_message_unref(reply);
    sd_bus_message_unref(req);
    return rc;
}

int wlnk_nm_is_managed(const char *ifname, bool *managed)
{
    if (managed == NULL)
        return wlnk_fail(WLNK_E_INVALID_ARG, 0, "NetworkManager: no output");
    *managed = false;

    sd_bus *bus = NULL;
    char path[NM_PATH_MAX];
    int rc = find_device(ifname, &bus, path);
    if (rc == WLNK_OK && path[0] != '\0') {
        sd_bus_error e = SD_BUS_ERROR_NULL;
        int value = 0;
        int brc = sd_bus_get_property_trivial(bus, NM_NAME, path, NM_DEVICE,
                                              "Managed", &e, 'b', &value);
        if (brc < 0)
            rc = bus_fail(&e, brc, "read", ifname);
        else
            *managed = value != 0;
        sd_bus_error_free(&e);
    }
    sd_bus_unref(bus);
    return rc == WLNK_OK ? wlnk_ok() : rc;
}

int wlnk_nm_set_managed(const char *ifname, bool managed)
{
    sd_bus *bus = NULL;
    char path[NM_PATH_MAX];
    int rc = find_device(ifname, &bus, path);
    if (rc == WLNK_OK && path[0] != '\0') {
        sd_bus_error e = SD_BUS_ERROR_NULL;
        int brc = sd_bus_set_property(bus, NM_NAME, path, NM_DEVICE, "Managed",
                                      &e, "b", managed ? 1 : 0);
        if (brc < 0)
            rc = bus_fail(&e, brc, managed ? "manage" : "unmanage", ifname);
        sd_bus_error_free(&e);
    }
    sd_bus_unref(bus);
    return rc == WLNK_OK ? wlnk_ok() : rc;
}
