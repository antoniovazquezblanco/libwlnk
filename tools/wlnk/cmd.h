/*
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2026 Antonio Vázquez Blanco
 *
 * cmd.h
 * What every wlnk subcommand shares.
 */

#ifndef WLNK_TOOL_CMD_H
#define WLNK_TOOL_CMD_H

#include <argtable3.h>

/** Exit status: the library failed. */
#define WLNK_EXIT_LIBRARY 1
/** Exit status: the command line is wrong. */
#define WLNK_EXIT_USAGE   2

/**
 * @brief Report a library failure: its status, then its detail.
 * @param res Output.
 * @param what What failed.
 * @param rc Its status.
 * @return WLNK_EXIT_LIBRARY.
 */
int cmd_fail(arg_dstr_t res, const char *what, int rc);

/**
 * @brief Report running out of memory.
 * @param res Output.
 * @return WLNK_EXIT_LIBRARY.
 */
int cmd_oom(arg_dstr_t res);

/**
 * @brief Parse a subcommand's own arguments, answering --help.
 * @param argc Its argument count.
 * @param argv Its arguments, its name first.
 * @param argtable Its options, ended by @p end.
 * @param end The end of @p argtable.
 * @param help Its --help option.
 * @param res Output.
 * @param name Its name.
 * @param summary What it does.
 * @return -1 to carry on, or the exit status.
 */
int cmd_parse(int argc, char **argv, void **argtable, arg_end_t *end,
              arg_lit_t *help, arg_dstr_t res, const char *name,
              const char *summary);

#endif /* WLNK_TOOL_CMD_H */
