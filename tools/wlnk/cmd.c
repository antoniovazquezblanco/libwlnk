/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * cmd.c
 * What every wlnk subcommand shares.
 */

#include <argtable3.h>
#include <stdlib.h>
#include "wlnk.h"

#include "cmd.h"

int cmd_fail(arg_dstr_t res, const char *what, int rc)
{
    arg_dstr_catf(res, "wlnk: %s: %s\n", what, wlnk_strerr(rc));
    if (wlnk_err_msg()[0] != '\0')
        arg_dstr_catf(res, "  %s\n", wlnk_err_msg());
    return WLNK_EXIT_LIBRARY;
}

int cmd_parse(int argc, char **argv, void **argtable, arg_end_t *end,
              arg_lit_t *help, arg_dstr_t res, const char *name,
              const char *summary)
{
    if (arg_nullcheck(argtable) != 0) {
        arg_dstr_cat(res, "wlnk: out of memory\n");
        return WLNK_EXIT_LIBRARY;
    }
    if (arg_parse(argc, argv, argtable) > 0) {
        arg_print_errors_ds(res, end, "wlnk");
        arg_dstr_catf(res, "Try 'wlnk %s --help'.\n", name);
        return WLNK_EXIT_USAGE;
    }
    if (help->count > 0) {
        arg_dstr_catf(res, "Usage: wlnk %s", name);
        arg_print_syntax_ds(res, argtable, "\n");
        arg_dstr_catf(res, "\n%s\n\n", summary);
        arg_print_glossary_ds(res, argtable, "  %-20s %s\n");
        return EXIT_SUCCESS;
    }
    return -1;
}
