/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * cmd_list.c
 * wlnk list: every wireless interface.
 *
 * The shortest correct use of wlnk_iface_list(), so it doubles as a worked
 * example. sparkcli prints the table straight to stdout.
 */

#include <argtable3.h>
#include <sparkcli.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "wlnk.h"

#include "cmd.h"
#include "cmd_list.h"

/**
 * @brief Print the interfaces as a table.
 * @param res Output, for errors.
 * @param ifaces The interfaces.
 * @param n How many.
 * @return The exit status.
 */
static int print_table(arg_dstr_t res, const wlnk_iface_t *ifaces, size_t n)
{
    /* sparkcli copies what it is given but takes it as char *, so one row
     * of buffers carries the titles and then each interface. */
    char cell[][24] = {"Interface", "Radio",     "Kind", "MAC",
                       "Monitor",   "Frequency", "Width"};
    char *row[] = {cell[0], cell[1], cell[2], cell[3],
                   cell[4], cell[5], cell[6]};
    scli_tbl_t *tbl = scli_tbl_new(sizeof(row) / sizeof(row[0]), row);
    if (tbl == NULL)
        return cmd_oom(res);

    bool ok = true;
    for (size_t i = 0; ok && i < n; i++) {
        const wlnk_iface_t *f = &ifaces[i];
        snprintf(cell[0], sizeof(cell[0]), "%s", f->ifname);
        snprintf(cell[1], sizeof(cell[1]), "phy#%d", f->wiphy);
        snprintf(cell[2], sizeof(cell[2]), "%s", wlnk_ifkind_str(f->kind));
        snprintf(cell[3], sizeof(cell[3]), "%02x:%02x:%02x:%02x:%02x:%02x",
                 f->mac[0], f->mac[1], f->mac[2], f->mac[3], f->mac[4],
                 f->mac[5]);
        snprintf(cell[4], sizeof(cell[4]), "%s",
                 (f->radio.caps & WLNK_CAP_MONITOR) != 0 ? "yes" : "no");
        cell[5][0] = '\0';
        cell[6][0] = '\0';
        if (f->chan.freq_mhz != 0) {
            snprintf(cell[5], sizeof(cell[5]), "%u MHz", f->chan.freq_mhz);
            snprintf(cell[6], sizeof(cell[6]), "%s",
                     wlnk_chwidth_str(f->chan.width));
        }
        ok = scli_tbl_add_row(tbl, row);
    }
    if (ok)
        scli_tbl_render(tbl);
    scli_tbl_free(&tbl);
    return ok ? EXIT_SUCCESS : cmd_oom(res);
}

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
        status = cmd_fail(res, "list interfaces", rc);
    } else if (n == 0) {
        arg_dstr_cat(res, "no wireless interfaces\n");
        status = EXIT_SUCCESS;
    } else {
        status = print_table(res, ifaces, n);
    }
    wlnk_iface_free(ifaces);
    arg_freetable(argtable, nargs);
    return status;
}

void cmd_list_register(void)
{
    arg_cmd_register("list", cmd_list, "list wireless interfaces", NULL);
}
