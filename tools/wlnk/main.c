/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * main.c
 * wlnk, the command-line front end of libwlnk.
 *
 * Usage: wlnk [-h] <subcommand> [<args>]. Global options come before the
 * subcommand; each cmd_*.c holds one subcommand.
 */

#include <argtable3.h>
#include <stdio.h>
#include <stdlib.h>

#include "cmd.h"
#include "cmd_list.h"
#include "cmd_version.h"

/**
 * @brief Print the usage, the global options and the subcommands.
 * @param argtable The global options.
 */
static void print_help(void **argtable)
{
    printf("Usage: wlnk");
    arg_print_syntax(stdout, argtable, " <subcommand> [<args>]\n\n");
    arg_print_glossary(stdout, argtable, "  %-20s %s\n");
    printf("\nSubcommands:\n");
    arg_cmd_itr_t it = arg_cmd_itr_create();
    do {
        arg_cmd_info_t *info = arg_cmd_itr_value(it);
        printf("  %-20s %s\n", info->name, info->description);
    } while (arg_cmd_itr_advance(it));
    arg_cmd_itr_destroy(it);
}

int main(int argc, char **argv)
{
    arg_lit_t *help = arg_lit0("h", "help", "print this help and exit");
    arg_end_t *end = arg_end(10);
    void *argtable[] = {help, end};
    const size_t nargs = sizeof(argtable) / sizeof(argtable[0]);
    if (arg_nullcheck(argtable) != 0) {
        fprintf(stderr, "wlnk: out of memory\n");
        arg_freetable(argtable, nargs);
        return WLNK_EXIT_LIBRARY;
    }

    /* Global options end at the first word that is not one. */
    int first = 1;
    while (first < argc && argv[first][0] == '-')
        first++;

    arg_cmd_init();
    cmd_list_register();
    cmd_version_register();
    int status = EXIT_SUCCESS;
    if (arg_parse(first, argv, argtable) > 0) {
        arg_print_errors(stderr, end, "wlnk");
        fprintf(stderr, "Try 'wlnk --help'.\n");
        status = WLNK_EXIT_USAGE;
    } else if (help->count > 0) {
        print_help(argtable);
    } else if (first == argc) {
        print_help(argtable);
        status = WLNK_EXIT_USAGE;
    } else if (arg_cmd_info(argv[first]) == NULL) {
        fprintf(stderr, "wlnk: unknown subcommand '%s'; try 'wlnk --help'.\n",
                argv[first]);
        status = WLNK_EXIT_USAGE;
    } else {
        arg_dstr_t res = arg_dstr_create();
        status = arg_cmd_dispatch(argv[first], argc - first, argv + first, res);
        fputs(arg_dstr_cstr(res), status == EXIT_SUCCESS ? stdout : stderr);
        arg_dstr_destroy(res);
    }

    arg_cmd_uninit();
    arg_freetable(argtable, nargs);
    return status;
}
