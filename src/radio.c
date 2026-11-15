/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * radio.c
 * Radios and their identity.
 *
 * The identity has to survive a driver reload, so it is the bus path of the
 * phy's device rather than the phy name or the wiphy index, both of which
 * are renumbered. One device may register several phys; those get an
 * ordinal, assigned by phy name so it is stable for a given set. A phy
 * with no device link (mac80211_hwsim, some SoC radios) is named after the
 * phy itself.
 */

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "err.h"

#include "err_internal.h"
#include "radio.h"

/* Root of sysfs. Only the tests set it, to a tree of their own. */
#ifndef WLNK_SYSFS
#define WLNK_SYSFS "/sys"
#endif

/** Where the radios are. */
#define SYS_PHY      WLNK_SYSFS "/class/ieee80211"
/** Where the interfaces are. */
#define SYS_NET      WLNK_SYSFS "/class/net"
/** Room for a sysfs path. */
#define PATH_MAX_LEN 256

/* A radio with no device link is named after its phy, whole. */
_Static_assert(WLNK_RADIO_ID_MAX >= WLNK_PHYNAME_MAX,
               "a phy name must fit the radio identity");

/**
 * @brief Read the first line of a sysfs attribute, without its newline.
 * @param path Attribute file.
 * @param out Receives the line.
 * @param cap Size of @p out.
 * @return False when the file is missing or empty.
 */
static bool read_attr(const char *path, char *out, size_t cap)
{
    FILE *fp = fopen(path, "re");
    if (fp == NULL)
        return false;
    bool ok = fgets(out, (int)cap, fp) != NULL;
    fclose(fp);
    if (!ok)
        return false;
    out[strcspn(out, "\n")] = '\0';
    return out[0] != '\0';
}

/**
 * @brief Last component of a symlink's target: the bus name of the device
 * a phy hangs off.
 * @param path The symlink.
 * @param out Receives the name.
 * @param cap Size of @p out.
 * @return False when there is no such link.
 */
static bool link_basename(const char *path, char *out, size_t cap)
{
    char target[PATH_MAX_LEN];
    ssize_t n = readlink(path, target, sizeof(target) - 1);
    if (n <= 0)
        return false;
    target[n] = '\0';
    const char *base = strrchr(target, '/');
    base = base != NULL ? base + 1 : target;
    if (base[0] == '\0')
        return false;
    snprintf(out, cap, "%.*s", (int)cap - 1, base);
    return true;
}

/**
 * @brief Bus name of a phy's device.
 * @param phy Phy name, or any directory entry name.
 * @param out Receives the bus name.
 * @param cap Size of @p out.
 * @return False when the phy has no device link.
 */
static bool phy_bus_name(const char *phy, char *out, size_t cap)
{
    char path[PATH_MAX_LEN];
    /* A directory entry can be longer than any phy name: bound it rather
     * than let the path run over. */
    snprintf(path, sizeof(path), SYS_PHY "/%.*s/device",
             (int)sizeof(path) - (int)sizeof(SYS_PHY) - 8, phy);
    return link_basename(path, out, cap);
}

/**
 * @brief Rank of a phy among the phys sharing its device, by phy name.
 * @param phy Phy name.
 * @param bus Its device's bus name.
 * @return The rank; 0 when it is the only one, so the usual radio keeps a
 * bare bus path as its identity.
 */
static unsigned device_ordinal(const char *phy, const char *bus)
{
    DIR *d = opendir(SYS_PHY);
    if (d == NULL)
        return 0;
    unsigned rank = 0;
    unsigned siblings = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.')
            continue;
        char other[WLNK_RADIO_ID_MAX];
        if (!phy_bus_name(e->d_name, other, sizeof(other)))
            continue;
        if (strcmp(other, bus) != 0)
            continue;
        siblings++;
        if (strcmp(e->d_name, phy) < 0)
            rank++;
    }
    closedir(d);
    return siblings > 1 ? rank : 0;
}

/**
 * @brief Make an identity safe as a file name.
 *
 * Bus paths already are, but nothing guarantees it for every bus the
 * kernel may grow. Anything but the characters buses use becomes '_', and
 * so does a leading dot, so the record never turns into a hidden name or a
 * path component.
 *
 * @param s Identity to change in place.
 */
static void sanitize(char *s)
{
    for (char *p = s; *p != '\0'; p++) {
        bool safe = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                    (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' ||
                    *p == ':' || *p == '.';
        if (!safe)
            *p = '_';
    }
    if (s[0] == '.')
        s[0] = '_';
}

int wlnk_radio_resolve(const char *ifname, wlnk_radio_t *out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->wiphy = -1;
    }
    if (ifname == NULL || out == NULL || ifname[0] == '\0')
        return wlnk_fail(WLNK_E_INVALID_ARG, 0, "radio: no interface");
    if (strlen(ifname) >= WLNK_IFNAME_MAX)
        return wlnk_fail(WLNK_E_INVALID_ARG, 0,
                         "radio: interface name too long");
    snprintf(out->ifname, sizeof(out->ifname), "%s", ifname);

    /* The phy80211 link exists exactly for wireless interfaces, so it is
     * also the "is this a radio?" test. */
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), SYS_NET "/%s/phy80211/name", ifname);
    if (!read_attr(path, out->phy, sizeof(out->phy))) {
        char msg[WLNK_ERR_MSG_MAX];
        snprintf(msg, sizeof(msg), "%s: not a wireless interface", ifname);
        return wlnk_fail(WLNK_E_NOT_FOUND, 0, msg);
    }

    char idx[32];
    snprintf(path, sizeof(path), SYS_NET "/%s/phy80211/index", ifname);
    if (read_attr(path, idx, sizeof(idx)))
        out->wiphy = (int32_t)strtol(idx, NULL, 10);

    char bus[WLNK_RADIO_ID_MAX];
    if (phy_bus_name(out->phy, bus, sizeof(bus))) {
        unsigned ord = device_ordinal(out->phy, bus);
        if (ord == 0)
            snprintf(out->id, sizeof(out->id), "%s", bus);
        else
            /* The ordinal must fit whatever the bus name leaves. */
            snprintf(out->id, sizeof(out->id), "%.*s.%u",
                     (int)(sizeof(out->id) - 12), bus, ord);
    } else {
        /* No device link: the phy name is all there is to go on. It moves
         * across driver reloads, which only costs a stale record a reboot
         * would have cleared anyway. */
        snprintf(out->id, sizeof(out->id), "%s", out->phy);
    }
    sanitize(out->id);
    return wlnk_ok();
}
