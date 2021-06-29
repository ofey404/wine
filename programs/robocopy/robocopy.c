/*
 * Copyright 2021 Weiwen Chen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "robocopy.h"
#include <wine/debug.h>

WINE_DEFAULT_DEBUG_CHANNEL(robocopy);

enum opt_indicator
{
    COPY_SUBDIRECTORIES_EXCLUDE_EMPTY,
    COPY_SUBDIRECTORIES_INCLUDE_EMPTY,
    PURGE_NOT_IN_SOURCE,
    MIRROR_DIRECTORY_TREE,
    EXCLUDE_FILES,
    EXCLUDE_DIRECTORY,
    EXCLUDE_OLDER_FILE,
    RETRY_ON_FAILED,
    WAIT_TIME_BETWEEN_RETRIES,
    NO_LOG_FILE_SIZE,
    NO_LOG_FILE_CLASS,
    NO_LOG_COPY_PROGRESS,
    NO_FILE_NAME_LOGGED,
    NO_DIRECTORY_NAME_LOGGED,
    NO_JOB_HEADER,
    NO_JOB_SUMMARY,
};

static gdos_option const opts[] = {
    /* copy options */
    {"s", NO_ARGUMENT, COPY_SUBDIRECTORIES_EXCLUDE_EMPTY},
    {"e", NO_ARGUMENT, COPY_SUBDIRECTORIES_INCLUDE_EMPTY},

    /* FIXME: What means "allows the destination directory
              security settings to not be overwritten." in doc? */
    {"purge", NO_ARGUMENT, PURGE_NOT_IN_SOURCE},
    {"mir", NO_ARGUMENT, MIRROR_DIRECTORY_TREE},

    /* file selection options */
    {"xf", SPACE_SEPERATED_ARGUMENT_LIST, EXCLUDE_FILES},
    {"xd", SPACE_SEPERATED_ARGUMENT_LIST, EXCLUDE_DIRECTORY},
    {"xo", NO_ARGUMENT, EXCLUDE_OLDER_FILE},

    /* retry options */
    {"r", COLON_SEPERATED_ARGUMENT, RETRY_ON_FAILED},
    {"w", COLON_SEPERATED_ARGUMENT, WAIT_TIME_BETWEEN_RETRIES},

    /* logging options */
    {"ns", NO_ARGUMENT, NO_LOG_FILE_SIZE},
    {"nc", NO_ARGUMENT, NO_LOG_FILE_CLASS},
    {"np", NO_ARGUMENT, NO_LOG_COPY_PROGRESS},
    {"nfl", NO_ARGUMENT, NO_FILE_NAME_LOGGED},
    {"ndl", NO_ARGUMENT, NO_DIRECTORY_NAME_LOGGED},
    {"njh", NO_ARGUMENT, NO_JOB_HEADER},
    {"njs", NO_ARGUMENT, NO_JOB_SUMMARY},
    /* job options */
};

int robocopy_init_default_options(robocopy_options *opt) {
    opt->source = NULL;
    opt->destination = NULL;
    opt->files = NULL;

    /* copy options */
    opt->copy_subdir = not_copy_subdir;
    opt->purge_not_in_source = FALSE;

    /* file selection options */
    opt->exclude_file_patterns = NULL;
    opt->num_file_patterns = 0;
    opt->exclude_directory_patterns = NULL;
    opt->num_file_patterns = 0;
    opt->exclude_older_file = FALSE;

    /* retry options */
    opt->num_retries_on_failed_copies = 1000000;
    opt->wait_second_between_retries = 30;

    /* logging options */
    opt->not_log_file_size = FALSE; 
    opt->not_log_file_class = FALSE; 
    opt->not_display_progress = FALSE; 
    opt->not_log_file_names = FALSE; 
    opt->not_log_directory_names = FALSE; 
    opt->no_job_header = FALSE; 
    opt->not_job_summary = FALSE; 

    /* job options */
    return INTERNAL_SUCCESS;
}

int robocopy_parse_options(robocopy_options *opt, int argc, WCHAR *argvW[]) {
    gdos_context ctx;
    int opt;

    /* TODO: handle this properly */
    if (getopt_dos_create_context(&ctx, argc, argvW, opts) != 0)
        return 1;

    /* skip <source> <destination> and <files> */
    while ((opt = getopt_dos_next(&ctx)) == GDOS_NEXT_NOT_OPTION)
        ;

    /* no flag */
    if (opt == GDOS_OPTIONS_END)
        /* TODO */
        return 0;

    if (argc < 3)
    {
        /* TODO: Report error */
    }
    if (argc == 3)
    {
        /* TODO: parse `source`, `destination` and `files` arguments */
        output_message(STRING_USAGE);
        return 1;
    }

    return 0;
}

int __cdecl wmain(int argc, WCHAR *argvW[])
{
    robocopy_options x;
    if (robocopy_init_default_options(&x) != INTERNAL_SUCCESS) {
        return INTERNAL_ERROR_PLACEHOLDER;
    }

    if (robocopy_parse_options(&x, argc, argvW) != INTERNAL_SUCCESS) {
        return INTERNAL_ERROR_PLACEHOLDER;
    }

    return ROBOCOPY_EXIT_SUCCESS;
}
