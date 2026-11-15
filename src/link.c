/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * link.c
 * Interface link state, through the classic interface ioctls.
 */

#include <errno.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include "err.h"

#include "err_internal.h"
#include "link.h"

/**
 * @brief Check an interface name and open a control socket for it.
 * @param ifname Interface name.
 * @param fd Receives the socket.
 * @return WLNK_OK, or the failure's status with the detail recorded.
 */
static int open_ctl(const char *ifname, int *fd)
{
    if (ifname == NULL || ifname[0] == '\0')
        return wlnk_fail(WLNK_E_INVALID_ARG, 0, "link: no interface name");
    /* A longer name would be cut short in struct ifreq and could name
     * another interface. */
    if (strlen(ifname) >= IFNAMSIZ)
        return wlnk_fail(WLNK_E_INVALID_ARG, 0,
                         "link: interface name too long");
    *fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (*fd < 0)
        return wlnk_fail_sys(WLNK_E_IO, errno, "link: control socket");
    return WLNK_OK;
}

/**
 * @brief Read an interface's flags.
 * @param fd Control socket.
 * @param ifname Interface name, checked by open_ctl().
 * @param flags Receives the flags.
 * @return WLNK_OK; WLNK_E_NOT_FOUND when there is no such interface.
 */
static int read_flags(int fd, const char *ifname, short *flags)
{
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    memcpy(ifr.ifr_name, ifname, strlen(ifname));
    if (ioctl(fd, SIOCGIFFLAGS, &ifr) != 0) {
        int e = errno;
        char msg[WLNK_ERR_MSG_MAX];
        snprintf(msg, sizeof(msg), "link %s", ifname);
        return wlnk_fail_sys(e == ENODEV ? WLNK_E_NOT_FOUND : WLNK_E_IO, e,
                             msg);
    }
    *flags = ifr.ifr_flags;
    return WLNK_OK;
}

int wlnk_if_is_up(const char *ifname, bool *up)
{
    if (up == NULL)
        return wlnk_fail(WLNK_E_INVALID_ARG, 0, "link: no output");
    *up = false;

    int fd = -1;
    int rc = open_ctl(ifname, &fd);
    if (rc != WLNK_OK)
        return rc;
    short flags;
    rc = read_flags(fd, ifname, &flags);
    close(fd);
    if (rc != WLNK_OK)
        return rc;
    *up = (flags & IFF_UP) != 0;
    return wlnk_ok();
}

int wlnk_if_set_up(const char *ifname, bool up)
{
    int fd = -1;
    int rc = open_ctl(ifname, &fd);
    if (rc != WLNK_OK)
        return rc;
    short flags;
    rc = read_flags(fd, ifname, &flags);
    if (rc != WLNK_OK) {
        close(fd);
        return rc;
    }

    short want = up ? (short)(flags | IFF_UP) : (short)(flags & ~IFF_UP);
    if (want == flags) {
        close(fd);
        return wlnk_ok();
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    memcpy(ifr.ifr_name, ifname, strlen(ifname));
    ifr.ifr_flags = want;
    if (ioctl(fd, SIOCSIFFLAGS, &ifr) != 0) {
        int e = errno;
        close(fd);
        char msg[WLNK_ERR_MSG_MAX];
        int status = wlnk_errno_status(e);
        if (status == WLNK_E_PERM) {
            snprintf(msg, sizeof(msg),
                     "link %s %s: not permitted (CAP_NET_ADMIN)", ifname,
                     up ? "up" : "down");
            return wlnk_fail(status, e, msg);
        }
        snprintf(msg, sizeof(msg), "link %s %s", ifname, up ? "up" : "down");
        return wlnk_fail_sys(status, e, msg);
    }
    close(fd);
    return wlnk_ok();
}
