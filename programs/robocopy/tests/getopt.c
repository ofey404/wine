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

#include "robocopy_test.h"

// FIXME: I tried this, but configure script return such error:
//   ../wine-git/programs/robocopy/tests/getopt.c:20: error: #include directive with relative path not allowed
// Shouldn't import c source file, but I just want to test whether relative import works or not.

// #include "../robocopy.h"
// #include "../getopt.c"

START_TEST(test)
{
    /* FIXME: want to write unit test for ../getopt.c, but how can I include it? */
    ok(getopt_long(0, NULL, NULL), "This placeholder should be true.");
    return;
}