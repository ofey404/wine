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

/* Index in ARGV of the next element to be scanned.
   This must be 1 before any call.  */
int optind = 1;

/* Single argument. */
char *optarg;

/* Argument list. */
int argsnum;
char **optargs;

/* getopt implementation that accepts slash as flag indicator.
   Accepted option types:
   - Without arguments,             eg: /x
   - Colon and single argument,     eg: /x:10
   - Space seperated argument list, eg: /x arg1 arg2
*/
int getopt_long(int argc, WCHAR *argvW[], const struct option opts[])
{
    return 1;
}