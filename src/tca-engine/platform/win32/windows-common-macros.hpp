// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

/**
 * This header file contains common macros and definitions that are used across the
 * Windows-specific implementation of the engine. It includes necessary Windows headers and defines
 * any macros that are required for the Windows platform, such as WIN32_LEAN_AND_MEAN to reduce the
 * size of the included Windows headers, and NOMINMAX to prevent conflicts with the standard
 * library's min and max functions.
 */

#ifndef _WIN32
#error "This header should only be included on Windows platforms."
#endif // _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN

#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX
