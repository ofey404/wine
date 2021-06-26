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

#ifndef __ROBOCOPY_H__
#define __ROBOCOPY_H__

#include <stdlib.h>
#include <windows.h>
#include "resource.h"

#define MAX_SUBKEY_LEN   257

void output_writeconsole(const WCHAR *str, DWORD wlen);
void WINAPIV output_message(unsigned int id, ...);
void WINAPIV output_string(const WCHAR *fmt, ...);

/* getopt.c */
struct option
{
    const char *name;
    int has_arg;
    int *flag;
    int val;
};

int getopt_long(int argc, WCHAR *argvW[], const struct option opts[]);

#define no_argument		0
#define colon_seperated_argument	1
#define space_seperated_argument_list	2

#endif /* __ROBOCOPY_H__ */
