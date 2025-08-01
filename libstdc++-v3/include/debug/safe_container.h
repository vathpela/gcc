// Safe container implementation  -*- C++ -*-

// Copyright (C) 2014-2025 Free Software Foundation, Inc.
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

/** @file debug/safe_container.h
 *  This file is a GNU debug extension to the Standard C++ Library.
 */

#ifndef _GLIBCXX_DEBUG_SAFE_CONTAINER_H
#define _GLIBCXX_DEBUG_SAFE_CONTAINER_H 1

#include <ext/alloc_traits.h>

namespace __gnu_debug
{
  /// Safe class dealing with some allocator dependent operations.
  template<typename _SafeContainer,
	   typename _Alloc,
	   template<typename> class _SafeBase,
	   bool _IsCxx11AllocatorAware = true>
    class _Safe_container
    : public _SafeBase<_SafeContainer>
    {
      typedef _SafeBase<_SafeContainer> _Base;

      _GLIBCXX20_CONSTEXPR
      const _SafeContainer&
      _M_cont() const _GLIBCXX_NOEXCEPT
      { return *static_cast<const _SafeContainer*>(this); }

    protected:
#if __cplusplus >= 201103L
      _Safe_container() = default;
      _Safe_container(const _Safe_container&) = default;
      _Safe_container(_Safe_container&&) = default;

    private:
      _GLIBCXX20_CONSTEXPR
      void
      _M_swap_base(const _Safe_container& __x) const noexcept
      { _Base::_M_swap(__x); }

      _GLIBCXX20_CONSTEXPR
      _Safe_container(_Safe_container&& __x, const _Alloc&, std::true_type)
      : _Safe_container(std::move(__x))
      { }

      _GLIBCXX20_CONSTEXPR
      _Safe_container(_Safe_container&& __x, const _Alloc& __a, std::false_type)
      : _Safe_container()
      {
	if (!std::__is_constant_evaluated())
	  {
	    if (__x._M_cont().get_allocator() == __a)
	      _M_swap_base(__x);
	    else
	      __x._M_invalidate_all();
	  }
      }

    protected:
      _GLIBCXX20_CONSTEXPR
      _Safe_container(_Safe_container&& __x, const _Alloc& __a)
      : _Safe_container(std::move(__x), __a,
		      typename std::allocator_traits<_Alloc>::is_always_equal{})
      { }
#endif

      // Copy assignment invalidate all iterators.
      _GLIBCXX20_CONSTEXPR
      _Safe_container&
      operator=(const _Safe_container&) _GLIBCXX_NOEXCEPT
      {
	if (!std::__is_constant_evaluated())
	  this->_M_invalidate_all();
	return *this;
      }

#if __cplusplus >= 201103L
      _GLIBCXX20_CONSTEXPR
      _Safe_container&
      operator=(_Safe_container&& __x) noexcept
      {
	if (std::__is_constant_evaluated())
	  return *this;

	if (std::__addressof(__x) == this)
	  {
	    // Standard containers have a valid but unspecified value after
	    // self-move, so we invalidate all debug iterators even if the
	    // underlying container happens to preserve its contents.
	    this->_M_invalidate_all();
	    return *this;
	  }

	if (_IsCxx11AllocatorAware)
	  {
	    typedef __gnu_cxx::__alloc_traits<_Alloc> _Alloc_traits;

	    bool __xfer_memory = _Alloc_traits::_S_propagate_on_move_assign()
	      || _M_cont().get_allocator() == __x._M_cont().get_allocator();
	    if (__xfer_memory)
	      _M_swap_base(__x);
	    else
	      this->_M_invalidate_all();
	  }
	else
	  _M_swap_base(__x);

	__x._M_invalidate_all();
	return *this;
      }

      _GLIBCXX20_CONSTEXPR
      void
      _M_swap(const _Safe_container& __x) const noexcept
      {
	if (_IsCxx11AllocatorAware)
	  {
	    typedef __gnu_cxx::__alloc_traits<_Alloc> _Alloc_traits;

	    if (!_Alloc_traits::_S_propagate_on_swap())
	      __glibcxx_check_equal_allocs(this->_M_cont()._M_base(),
					   __x._M_cont()._M_base());
	  }

	_M_swap_base(__x);
      }
#else
      void
      _M_swap(const _Safe_container& __x) const throw()
      { _Base::_M_swap(__x); }
#endif
    };

} // namespace __gnu_debug

#endif
