// Exception Handling support header for -*- C++ -*-

// Copyright (C) 2016-2025 Free Software Foundation, Inc.
//
// This file is part of GCC.
//
// GCC is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3, or (at your option)
// any later version.
//
// GCC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// Under Section 7 of GPL version 3, you are granted additional
// permissions described in the GCC Runtime Library Exception, version
// 3.1, as published by the Free Software Foundation.

// You should have received a copy of the GNU General Public License and
// a copy of the GCC Runtime Library Exception along with this program;
// see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
// <http://www.gnu.org/licenses/>.

/** @file bits/exception.h
 *  This is an internal header file, included by other library headers.
 *  Do not attempt to use it directly.
 */

#ifndef __EXCEPTION_H
#define __EXCEPTION_H 1

#ifdef _GLIBCXX_SYSHDR
#pragma GCC system_header
#endif

#include <bits/c++config.h>

extern "C++" {

namespace std _GLIBCXX_VISIBILITY(default)
{
  /**
   * @defgroup exceptions Exceptions
   * @ingroup diagnostics
   * @since C++98
   *
   * Classes and functions for reporting errors via exceptions.
   * @{
   */

  /**
   *  @brief Base class for all library exceptions.
   *
   *  This is the base class for all exceptions thrown by the standard
   *  library, and by certain language expressions.  You are free to derive
   *  your own %exception classes, or use a different hierarchy, or to
   *  throw non-class data (e.g., fundamental types).
   */
  class exception
  {
  public:
    _GLIBCXX26_CONSTEXPR exception() _GLIBCXX_NOTHROW { }
#if __cplusplus >= 202400L
    constexpr virtual ~exception() _GLIBCXX_TXN_SAFE_DYN noexcept {}
#else
    virtual ~exception() _GLIBCXX_TXN_SAFE_DYN _GLIBCXX_NOTHROW;
#endif
#if __cplusplus >= 201103L
    _GLIBCXX26_CONSTEXPR exception(const exception&) = default;
    _GLIBCXX26_CONSTEXPR exception& operator=(const exception&) = default;
    _GLIBCXX26_CONSTEXPR exception(exception&&) = default;
    _GLIBCXX26_CONSTEXPR exception& operator=(exception&&) = default;
#endif

    /** Returns a C-style character string describing the general cause
     *  of the current error.  */
#if __cplusplus >= 202400L
    constexpr virtual const char*
    what() const _GLIBCXX_TXN_SAFE_DYN noexcept { return "std::exception"; }
#else
    virtual const char*
    what() const _GLIBCXX_TXN_SAFE_DYN _GLIBCXX_NOTHROW;
#endif
  };

  /// @}

} // namespace std

}

#endif
