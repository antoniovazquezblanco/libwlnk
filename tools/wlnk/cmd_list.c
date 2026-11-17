/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * cmd_list.c
 * wlnk list: every wireless interface.
 *
 * The shortest correct use of wlnk_iface_list(), so it doubles as a worked
 * example.
 */

#include <argtable3.h>
#include <stdlib.h>
#include "wlnk.h"

#include "cmd.h"
#include "cmd_list.h"

/**
 * @brief wlnk list: every wireless interface, and whether its radio can
 * monitor.
 * @param argc Argument count.
 * @param argv Arguments, the subcommand's name first.
 * @param res Output.
 * @param ctx Unused.
 * @return The exit status.
 */
static int cmd_list(int argc, char **argv, arg_dstr_t res, void *ctx)
{
    (void)ctx;
    arg_lit_t *help = arg_lit0("h", "help", "print this help and exit");
    arg_end_t *end = arg_end(10);
    void *argtable[] = {help, end};
    const size_t nargs = sizeof(argtable) / sizeof(argtable[0]);
    int status = cmd_parse(argc, argv, argtable, end, help, res, "list",
                           "List every wireless interface, and whether its "
                           "radio can do monitor mode.");
    if (status >= 0) {
        arg_freetable(argtable, nargs);
        return status;
    }

    wlnk_iface_t *ifaces = NULL;
    size_t n = 0;
    int rc = wlnk_iface_list(&ifaces, &n);
    if (rc != WLNK_OK) {
        arg_freetable(argtable, nargs);
        return cmd_fail(res, "list interfaces", rc);
    }
    if (n == 0)
        arg_dstr_cat(res, "no wireless interfaces\n");
    for (size_t i = 0; i < n; i++) {
        const wlnk_iface_t *f = &ifaces[i];
        arg_dstr_catf(res, "%-15s phy#%-3d %-8s %02x:%02x:%02x:%02x:%02x:%02x",
                      f->ifname, f->wiphy, wlnk_ifkind_str(f->kind), f->mac[0],
                      f->mac[1], f->mac[2], f->mac[3], f->mac[4], f->mac[5]);
        const char *mon =
            (f->radio.caps & WLNK_CAP_MONITOR) != 0 ? "monitor" : "no monitor";
        if (f->chan.freq_mhz != 0)
            arg_dstr_catf(res, "  %-10s  %u MHz, %s\n", mon, f->chan.freq_mhz,
                          wlnk_chwidth_str(f->chan.width));
        else
            arg_dstr_catf(res, "  %s\n", mon);
    }
    wlnk_iface_free(ifaces);
    arg_freetable(argtable, nargs);
    return EXIT_SUCCESS;
}

void cmd_list_register(void)
{
    arg_cmd_register("list", cmd_list, "list wireless interfaces", NULL);
}
