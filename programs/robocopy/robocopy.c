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

#define CHAR_MAX 127

/* Use a non-character as a pseudo short option,
   starting with CHAR_MAX + 1.  */
enum
{
    MIRROR_DIRECTORY_TREE = CHAR_MAX + 1,
    NO_JOB_HEADER,
    NO_JOB_SUMMARY,
    NO_FILE_NAME_LOGGED,
    NO_DIRECTORY_NAME_LOGGED,
    COPY_SUBDIRECTORY_INCLUDE_EMPTY,
    NO_LOG_FILE_SIZE,
    NO_LOG_FILE_CLASS,
    NO_LOG_COPY_PROGRESS,
    EXCLUDE_OLDER_FILE,
    RETRY_ON_FAILED,
    WAIT_TIME_BETWEEN_RETRIES,
    EXCLUDE_DIRECTORY,
    EXCLUDE_FILES
};

static struct option const long_opts[] = {
    {"mir", no_argument, NULL, MIRROR_DIRECTORY_TREE},
    {"njh", no_argument, NULL, NO_JOB_HEADER},
    {"njs", no_argument, NULL, NO_JOB_SUMMARY},
    {"nfl", no_argument, NULL, NO_FILE_NAME_LOGGED},
    {"ndl", no_argument, NULL, NO_DIRECTORY_NAME_LOGGED},
    {"e", no_argument, NULL, COPY_SUBDIRECTORY_INCLUDE_EMPTY},
    {"ns", no_argument, NULL, NO_LOG_FILE_SIZE},
    {"nc", no_argument, NULL, NO_LOG_FILE_CLASS},
    {"np", no_argument, NULL, NO_LOG_COPY_PROGRESS},
    {"xo", no_argument, NULL, EXCLUDE_OLDER_FILE},
    {"r", colon_seperated_argument, NULL, RETRY_ON_FAILED},
    {"w", colon_seperated_argument, NULL, WAIT_TIME_BETWEEN_RETRIES},
    {"xd", space_seperated_argument_list, NULL, EXCLUDE_DIRECTORY},
    {"xf", space_seperated_argument_list, NULL, EXCLUDE_FILES}
};

void output_writeconsole(const WCHAR *str, DWORD wlen)
{
    DWORD count, ret;

    ret = WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), str, wlen, &count, NULL);
    if (!ret)
    {
        DWORD len;
        char  *msgA;

        /* On Windows WriteConsoleW() fails if the output is redirected. So fall
         * back to WriteFile(), assuming the console encoding is still the right
         * one in that case.
         */
        len = WideCharToMultiByte(GetConsoleOutputCP(), 0, str, wlen, NULL, 0, NULL, NULL);
        msgA = malloc(len);

        WideCharToMultiByte(GetConsoleOutputCP(), 0, str, wlen, msgA, len, NULL, NULL);
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msgA, len, &count, FALSE);
        free(msgA);
    }
}

static void output_formatstring(const WCHAR *fmt, __ms_va_list va_args)
{
    WCHAR *str;
    DWORD len;

    len = FormatMessageW(FORMAT_MESSAGE_FROM_STRING|FORMAT_MESSAGE_ALLOCATE_BUFFER,
                         fmt, 0, 0, (WCHAR *)&str, 0, &va_args);
    if (len == 0 && GetLastError() != ERROR_NO_WORK_DONE)
    {
        WINE_FIXME("Could not format string: le=%u, fmt=%s\n", GetLastError(), wine_dbgstr_w(fmt));
        return;
    }
    output_writeconsole(str, len);
    LocalFree(str);
}

void WINAPIV output_message(unsigned int id, ...)
{
    WCHAR *fmt = NULL;
    int len;
    __ms_va_list va_args;

    if (!(len = LoadStringW(GetModuleHandleW(NULL), id, (WCHAR *)&fmt, 0)))
    {
        WINE_FIXME("LoadString failed with %d\n", GetLastError());
        return;
    }

    len++;
    fmt = malloc(len * sizeof(WCHAR));
    if (!fmt) return;

    LoadStringW(GetModuleHandleW(NULL), id, fmt, len);

    __ms_va_start(va_args, id);
    output_formatstring(fmt, va_args);
    __ms_va_end(va_args);

    free(fmt);
}

void WINAPIV output_string(const WCHAR *fmt, ...)
{
    __ms_va_list va_args;

    __ms_va_start(va_args, fmt);
    output_formatstring(fmt, va_args);
    __ms_va_end(va_args);
}

static int first_option_index(int argc, WCHAR *argvW[])
{
    /* TODO: Placeholder */
    return 4;
}

int __cdecl wmain(int argc, WCHAR *argvW[])
{
    int c;
    int firstopt = first_option_index(argc, argvW);
    // struct robocopy_options x;

    if (argc < 3) {
        /* TODO: Report error */
    }
    if (argc == 3)
    {
        /* TODO: parse `source`, `destination` and `files` arguments */
        output_message(STRING_USAGE);
        return 1;
    }

    while ((c = getopt_long(argc - firstopt, argvW + firstopt, long_opts))
            != -1)
    {
        switch (c)
        {
            case MIRROR_DIRECTORY_TREE:
                /* Handle /mir */
                /* x.MIR = optarg */
                break;
            case RETRY_ON_FAILED:
                /* Handle /r:[n] */
                /* optarg */
                break;
            case EXCLUDE_DIRECTORY:
                /* Handle /xd <directory>[...] */
                /* argsnum and optargs */
                break;
        }
    }
    

    return 1;
}
