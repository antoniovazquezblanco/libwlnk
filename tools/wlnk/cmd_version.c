/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * cmd_version.c
 * wlnk version: the library's version.
 */

#include <argtable3.h>
#include <stdlib.h>
#include "wlnk.h"

#include "cmd.h"
#include "cmd_version.h"

/**
 * @brief wlnk version: print the library's version.
 * @param argc Argument count.
 * @param argv Arguments, the subcommand's name first.
 * @param res Output.
 * @param ctx Unused.
 * @return The exit status.
 */
static int cmd_version(int argc, char **argv, arg_dstr_t res, void *ctx)
{
    (void)ctx;
    arg_lit_t *help = arg_lit0("h", "help", "print this help and exit");
    arg_end_t *end = arg_end(10);
    void *argtable[] = {help, end};
    const size_t nargs = sizeof(argtable) / sizeof(argtable[0]);
    int status = cmd_parse(argc, argv, argtable, end, help, res, "version",
                           "Print the version of libwlnk.");
    if (status < 0) {
        arg_dstr_catf(res, "wlnk %s\n", wlnk_version_string());
        status = EXIT_SUCCESS;
    }
    arg_freetable(argtable, nargs);
    return status;
}

void cmd_version_register(void)
{
    arg_cmd_register("version", cmd_version, "print the version", NULL);
}
