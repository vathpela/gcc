// functional_hash.h header -*- C++ -*-

// Copyright (C) 2007-2025 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Under Section 7 of GPL version 3, you are granted additional
// permissions described in the GCC Runtime Library Exception, version
// 3.1, as published by the Free Software Foundation.

// You should have received a copy of the GNU General Public License and
// a copy of the GCC Runtime Library Exception along with this program;
// see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
// <http://www.gnu.org/licenses/>.

/** @file bits/functional_hash.h
 *  This is an internal header file, included by other library headers.
 *  Do not attempt to use it directly. @headername{functional}
 */

#ifndef _FUNCTIONAL_HASH_H
#define _FUNCTIONAL_HASH_H 1

#ifdef _GLIBCXX_SYSHDR
#pragma GCC system_header
#endif

#include <type_traits>
#include <bits/hash_bytes.h>

namespace std _GLIBCXX_VISIBILITY(default)
{
_GLIBCXX_BEGIN_NAMESPACE_VERSION

  /** @defgroup hashes Hashes
   *  @ingroup functors
   *
   *   Hashing functors taking a variable type and returning a @c std::size_t.
   *
   *  @{
   */

  template<typename _Result, typename _Arg>
    struct __hash_base
    {
#if __cplusplus < 202002L
      typedef _Result     result_type _GLIBCXX17_DEPRECATED;
      typedef _Arg      argument_type _GLIBCXX17_DEPRECATED;
#endif
    };

#if ! _GLIBCXX_INLINE_VERSION
  // Some std::hash specializations inherit this for ABI compatibility reasons.
  template<typename _Tp> struct __hash_empty_base { };
#endif

  /// Primary class template hash.
  template<typename _Tp>
    struct hash;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wc++14-extensions"
  template<typename _Tp, typename = void>
    constexpr bool __is_hash_enabled_for = false;

  template<typename _Tp>
    constexpr bool
    __is_hash_enabled_for<_Tp,
			  __void_t<decltype(hash<_Tp>()(declval<_Tp>()))>>
      = true;
#pragma GCC diagnostic pop

  // Helper struct for defining disabled specializations of std::hash.
  template<typename _Tp>
    struct __hash_not_enabled
    {
      __hash_not_enabled(__hash_not_enabled&&) = delete;
      ~__hash_not_enabled() = delete;
    };

  // Helper struct for hash with enum types.
  template<typename _Tp, bool = true>
    struct __hash_enum : public __hash_base<size_t, _Tp>
    {
      size_t
      operator()(_Tp __val) const noexcept
      {
       using __type = typename underlying_type<_Tp>::type;
       return hash<__type>{}(static_cast<__type>(__val));
      }
    };

  /// Primary class template hash, usable for enum types only.
  template<typename _Tp>
    struct hash
    : __conditional_t<__is_enum(_Tp), __hash_enum<_Tp>, __hash_not_enabled<_Tp>>
    { };

  /// Partial specializations for pointer types.
  template<typename _Tp>
    struct hash<_Tp*> : public __hash_base<size_t, _Tp*>
    {
      size_t
      operator()(_Tp* __p) const noexcept
      { return reinterpret_cast<size_t>(__p); }
    };

  // Explicit specializations for integer types.
#define _Cxx_hashtable_define_trivial_hash(_Tp) 	\
  template<>						\
    struct hash<_Tp> : public __hash_base<size_t, _Tp>  \
    {                                                   \
      size_t                                            \
      operator()(_Tp __val) const noexcept              \
      { return static_cast<size_t>(__val); }            \
    };

  /// Explicit specialization for bool.
  _Cxx_hashtable_define_trivial_hash(bool)

  /// Explicit specialization for char.
  _Cxx_hashtable_define_trivial_hash(char)

  /// Explicit specialization for signed char.
  _Cxx_hashtable_define_trivial_hash(signed char)

  /// Explicit specialization for unsigned char.
  _Cxx_hashtable_define_trivial_hash(unsigned char)

  /// Explicit specialization for wchar_t.
  _Cxx_hashtable_define_trivial_hash(wchar_t)

#ifdef _GLIBCXX_USE_CHAR8_T
  /// Explicit specialization for char8_t.
  _Cxx_hashtable_define_trivial_hash(char8_t)
#endif

  /// Explicit specialization for char16_t.
  _Cxx_hashtable_define_trivial_hash(char16_t)

  /// Explicit specialization for char32_t.
  _Cxx_hashtable_define_trivial_hash(char32_t)

  /// Explicit specialization for short.
  _Cxx_hashtable_define_trivial_hash(short)

  /// Explicit specialization for int.
  _Cxx_hashtable_define_trivial_hash(int)

  /// Explicit specialization for long.
  _Cxx_hashtable_define_trivial_hash(long)

  /// Explicit specialization for long long.
  _Cxx_hashtable_define_trivial_hash(long long)

  /// Explicit specialization for unsigned short.
  _Cxx_hashtable_define_trivial_hash(unsigned short)

  /// Explicit specialization for unsigned int.
  _Cxx_hashtable_define_trivial_hash(unsigned int)

  /// Explicit specialization for unsigned long.
  _Cxx_hashtable_define_trivial_hash(unsigned long)

  /// Explicit specialization for unsigned long long.
  _Cxx_hashtable_define_trivial_hash(unsigned long long)

#ifdef __GLIBCXX_TYPE_INT_N_0
  __extension__
  _Cxx_hashtable_define_trivial_hash(__GLIBCXX_TYPE_INT_N_0)
  __extension__
  _Cxx_hashtable_define_trivial_hash(__GLIBCXX_TYPE_INT_N_0 unsigned)
#endif
#ifdef __GLIBCXX_TYPE_INT_N_1
  __extension__
  _Cxx_hashtable_define_trivial_hash(__GLIBCXX_TYPE_INT_N_1)
  __extension__
  _Cxx_hashtable_define_trivial_hash(__GLIBCXX_TYPE_INT_N_1 unsigned)
#endif
#ifdef __GLIBCXX_TYPE_INT_N_2
  __extension__
  _Cxx_hashtable_define_trivial_hash(__GLIBCXX_TYPE_INT_N_2)
  __extension__
  _Cxx_hashtable_define_trivial_hash(__GLIBCXX_TYPE_INT_N_2 unsigned)
#endif
#ifdef __GLIBCXX_TYPE_INT_N_3
  __extension__
  _Cxx_hashtable_define_trivial_hash(__GLIBCXX_TYPE_INT_N_3)
  __extension__
  _Cxx_hashtable_define_trivial_hash(__GLIBCXX_TYPE_INT_N_3 unsigned)
#endif

#if defined __STRICT_ANSI__ && defined __SIZEOF_INT128__
  // In strict modes __GLIBCXX_TYPE_INT_N_0 is not defined for __int128,
  // but we want to always treat signed/unsigned __int128 as integral types.
  __extension__
  _Cxx_hashtable_define_trivial_hash(__int128)
  __extension__
  _Cxx_hashtable_define_trivial_hash(__int128 unsigned)
#endif

#undef _Cxx_hashtable_define_trivial_hash

  struct _Hash_impl
  {
    static size_t
    hash(const void* __ptr, size_t __clength,
	 size_t __seed = static_cast<size_t>(0xc70f6907UL))
    { return _Hash_bytes(__ptr, __clength, __seed); }

    template<typename _Tp>
      static size_t
      hash(const _Tp& __val)
      { return hash(&__val, sizeof(__val)); }

    template<typename _Tp>
      static size_t
      __hash_combine(const _Tp& __val, size_t __hash)
      { return hash(&__val, sizeof(__val), __hash); }
  };

  // A hash function similar to FNV-1a (see PR59406 for how it differs).
  struct _Fnv_hash_impl
  {
    static size_t
    hash(const void* __ptr, size_t __clength,
	 size_t __seed = static_cast<size_t>(2166136261UL))
    { return _Fnv_hash_bytes(__ptr, __clength, __seed); }

    template<typename _Tp>
      static size_t
      hash(const _Tp& __val)
      { return hash(&__val, sizeof(__val)); }

    template<typename _Tp>
      static size_t
      __hash_combine(const _Tp& __val, size_t __hash)
      { return hash(&__val, sizeof(__val), __hash); }
  };

  /// Specialization for float.
  template<>
    struct hash<float> : public __hash_base<size_t, float>
    {
      size_t
      operator()(float __val) const noexcept
      {
	// 0 and -0 both hash to zero.
	return __val != 0.0f ? std::_Hash_impl::hash(__val) : 0;
      }
    };

  /// Specialization for double.
  template<>
    struct hash<double> : public __hash_base<size_t, double>
    {
      size_t
      operator()(double __val) const noexcept
      {
	// 0 and -0 both hash to zero.
	return __val != 0.0 ? std::_Hash_impl::hash(__val) : 0;
      }
    };

  /// Specialization for long double.
  template<>
    struct hash<long double>
    : public __hash_base<size_t, long double>
    {
      _GLIBCXX_PURE size_t
      operator()(long double __val) const noexcept;
    };

#if __cplusplus >= 201703L
  template<>
    struct hash<nullptr_t> : public __hash_base<size_t, nullptr_t>
    {
      size_t
      operator()(nullptr_t) const noexcept
      { return 0; }
    };
#endif

  /// @} group hashes

  /** Hint about performance of hash functions.
   *
   * If a given hash function object is not fast, the hash-based containers
   * will cache the hash code.
   * The default behavior is to consider that hashers are fast unless specified
   * otherwise.
   *
   * Users can specialize this for their own hash functions in order to force
   * caching of hash codes in unordered containers. Specializing this trait
   * affects the ABI of the unordered containers, so use it carefully.
   */
  template<typename _Hash>
    struct __is_fast_hash : public std::true_type
    { };

  template<>
    struct __is_fast_hash<hash<long double>> : public std::false_type
    { };

_GLIBCXX_END_NAMESPACE_VERSION
} // namespace

#endif // _FUNCTIONAL_HASH_H
