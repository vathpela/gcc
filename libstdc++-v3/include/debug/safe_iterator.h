// Safe iterator implementation  -*- C++ -*-

// Copyright (C) 2003-2025 Free Software Foundation, Inc.
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

/** @file debug/safe_iterator.h
 *  This file is a GNU debug extension to the Standard C++ Library.
 */

#ifndef _GLIBCXX_DEBUG_SAFE_ITERATOR_H
#define _GLIBCXX_DEBUG_SAFE_ITERATOR_H 1

#include <debug/assertions.h>
#include <debug/macros.h>
#include <debug/functions.h>
#include <debug/safe_base.h>
#include <bits/stl_pair.h>
#include <ext/type_traits.h>
#if __cplusplus > 201703L
# include <compare>
#endif

#define _GLIBCXX_DEBUG_VERIFY_OPERANDS(_Lhs, _Rhs, _BadMsgId, _DiffMsgId) \
  if (!std::__is_constant_evaluated()) {				\
  _GLIBCXX_DEBUG_VERIFY((!_Lhs._M_singular() && !_Rhs._M_singular())	\
			|| (_Lhs._M_value_initialized()			\
			    && _Rhs._M_value_initialized()),		\
			_M_message(_BadMsgId)				\
			._M_iterator(_Lhs, #_Lhs)			\
			._M_iterator(_Rhs, #_Rhs));			\
  _GLIBCXX_DEBUG_VERIFY(_Lhs._M_can_compare(_Rhs),			\
			_M_message(_DiffMsgId)				\
			._M_iterator(_Lhs, #_Lhs)			\
			._M_iterator(_Rhs, #_Rhs));			\
  }

#define _GLIBCXX_DEBUG_VERIFY_EQ_OPERANDS(_Lhs, _Rhs)			\
  _GLIBCXX_DEBUG_VERIFY_OPERANDS(_Lhs, _Rhs, __msg_iter_compare_bad,	\
				 __msg_compare_different)

#define _GLIBCXX_DEBUG_VERIFY_REL_OPERANDS(_Lhs, _Rhs)		\
  _GLIBCXX_DEBUG_VERIFY_OPERANDS(_Lhs, _Rhs, __msg_iter_order_bad,	\
				 __msg_order_different)

#define _GLIBCXX_DEBUG_VERIFY_DIST_OPERANDS(_Lhs, _Rhs)			\
  _GLIBCXX_DEBUG_VERIFY_OPERANDS(_Lhs, _Rhs, __msg_distance_bad,	\
				 __msg_distance_different)

// This pair of macros helps with writing valid C++20 constexpr functions that
// contain a non-constexpr code path that defines a non-literal variable, which
// was otherwise disallowed until P2242R3 for C++23.  We use them below around
// __gnu_cxx::__scoped_lock variables so that the containing functions are still
// considered valid C++20 constexpr functions.

#if __cplusplus >= 202002L && __cpp_constexpr < 202110L
# define _GLIBCXX20_CONSTEXPR_NON_LITERAL_SCOPE_BEGIN [&]() -> void
# define _GLIBCXX20_CONSTEXPR_NON_LITERAL_SCOPE_END ();
#else
# define _GLIBCXX20_CONSTEXPR_NON_LITERAL_SCOPE_BEGIN
# define _GLIBCXX20_CONSTEXPR_NON_LITERAL_SCOPE_END
#endif

namespace __gnu_debug
{
  /** Helper struct to deal with sequence offering a before_begin
   *  iterator.
   **/
  template<typename _Sequence>
    struct _BeforeBeginHelper
    {
      template<typename _Iterator, typename _Category>
	static bool
	_S_Is(const _Safe_iterator<_Iterator, _Sequence, _Category>&)
	{ return false; }

      template<typename _Iterator, typename _Category>
	static bool
	_S_Is_Beginnest(const _Safe_iterator<_Iterator, _Sequence, _Category>& __it)
	{ return __it.base() == __it._M_get_sequence()->_M_base().begin(); }
    };

  /** Sequence traits giving the size of a container if possible. */
  template<typename _Sequence>
    struct _Sequence_traits
    {
      typedef _Distance_traits<typename _Sequence::iterator> _DistTraits;

      static typename _DistTraits::__type
      _S_size(const _Sequence& __seq)
      { return std::make_pair(__seq.size(), __dp_exact); }
    };

  /** \brief Safe iterator wrapper.
   *
   *  The class template %_Safe_iterator is a wrapper around an
   *  iterator that tracks the iterator's movement among sequences and
   *  checks that operations performed on the "safe" iterator are
   *  legal. In additional to the basic iterator operations (which are
   *  validated, and then passed to the underlying iterator),
   *  %_Safe_iterator has member functions for iterator invalidation,
   *  attaching/detaching the iterator from sequences, and querying
   *  the iterator's state.
   *
   *  Note that _Iterator must be the first base class so that it gets
   *  initialized before the iterator is being attached to the container's list
   *  of iterators and it is being detached before _Iterator get
   *  destroyed. Otherwise it would result in a data race.
   */
  template<typename _Iterator, typename _Sequence, typename _Category
	   = typename std::iterator_traits<_Iterator>::iterator_category>
    class _Safe_iterator
    : private _Iterator,
      public _Safe_iterator_base
    {
      typedef _Iterator _Iter_base;
      typedef _Safe_iterator_base _Safe_base;

      typedef std::iterator_traits<_Iterator> _Traits;

    protected:
      typedef std::__are_same<typename _Sequence::_Base::const_iterator,
			      _Iterator> _IsConstant;

      typedef typename __gnu_cxx::__conditional_type<
	_IsConstant::__value,
	typename _Sequence::_Base::iterator,
	typename _Sequence::_Base::const_iterator>::__type _OtherIterator;

      struct _Unchecked { };

      _GLIBCXX20_CONSTEXPR
      _Safe_iterator(const _Safe_iterator& __x, _Unchecked) _GLIBCXX_NOEXCEPT
      : _Iter_base(__x.base()), _Safe_base()
      {
	if (!std::__is_constant_evaluated())
	  _M_attach(__x._M_sequence);
      }

    public:
      typedef _Iterator					iterator_type;
      typedef typename _Traits::iterator_category	iterator_category;
      typedef typename _Traits::value_type		value_type;
      typedef typename _Traits::difference_type		difference_type;
      typedef typename _Traits::reference		reference;
      typedef typename _Traits::pointer			pointer;

#if __cplusplus > 201703L && __cpp_lib_concepts
      using iterator_concept = std::__detail::__iter_concept<_Iterator>;
#endif

      /// @post the iterator is singular and unattached
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator() _GLIBCXX_NOEXCEPT : _Iter_base() { }

      /**
       * @brief Safe iterator construction from an unsafe iterator and
       * its sequence.
       *
       * @pre @p seq is not NULL
       * @post this is not singular
       */
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator(_Iterator __i, const _Safe_sequence_base* __seq)
      _GLIBCXX_NOEXCEPT
      : _Iter_base(__i), _Safe_base(__seq, _S_constant())
      { }

      /**
       * @brief Copy construction.
       */
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator(const _Safe_iterator& __x) _GLIBCXX_NOEXCEPT
      : _Iter_base(__x.base()), _Safe_base()
      {
	if (std::__is_constant_evaluated())
	  return;

	// _GLIBCXX_RESOLVE_LIB_DEFECTS
	// DR 408. Is vector<reverse_iterator<char*> > forbidden?
	_GLIBCXX_DEBUG_VERIFY(!__x._M_singular()
			      || __x._M_value_initialized(),
			      _M_message(__msg_init_copy_singular)
			      ._M_iterator(*this, "this")
			      ._M_iterator(__x, "other"));
	_M_attach(__x._M_sequence);
      }

#if __cplusplus >= 201103L
      /**
       * @brief Move construction.
       * @post __x is singular and unattached
       */
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator(_Safe_iterator&& __x) noexcept
      : _Iter_base()
      {
	if (std::__is_constant_evaluated())
	  {
	    base() = __x.base();
	    return;
	  }

	_GLIBCXX_DEBUG_VERIFY(!__x._M_singular()
			      || __x._M_value_initialized(),
			      _M_message(__msg_init_copy_singular)
			      ._M_iterator(*this, "this")
			      ._M_iterator(__x, "other"));
	const _Safe_sequence_base* __seq = __x._M_sequence;
	__x._M_detach();
	std::swap(base(), __x.base());
	_M_attach(__seq);
      }
#endif

      /**
       *  @brief Converting constructor from a mutable iterator to a
       *  constant iterator.
      */
      template<typename _MutableIterator>
	_GLIBCXX20_CONSTEXPR
	_Safe_iterator(
	  const _Safe_iterator<_MutableIterator, _Sequence,
	    typename __gnu_cxx::__enable_if<_IsConstant::__value &&
	      std::__are_same<_MutableIterator, _OtherIterator>::__value,
			       _Category>::__type>& __x)
	_GLIBCXX_NOEXCEPT
	: _Iter_base(__x.base())
	{
	  if (std::__is_constant_evaluated())
	    return;

	  // _GLIBCXX_RESOLVE_LIB_DEFECTS
	  // DR 408. Is vector<reverse_iterator<char*> > forbidden?
	  _GLIBCXX_DEBUG_VERIFY(!__x._M_singular()
				|| __x._M_value_initialized(),
				_M_message(__msg_init_const_singular)
				._M_iterator(*this, "this")
				._M_iterator(__x, "other"));
	  _M_attach(__x._M_sequence);
	}

      /**
       * @brief Copy assignment.
       */
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator&
      operator=(const _Safe_iterator& __x) _GLIBCXX_NOEXCEPT
      {
	if (std::__is_constant_evaluated())
	  {
	    base() = __x.base();
	    return *this;
	  }

	// _GLIBCXX_RESOLVE_LIB_DEFECTS
	// DR 408. Is vector<reverse_iterator<char*> > forbidden?
	_GLIBCXX_DEBUG_VERIFY(!__x._M_singular()
			      || __x._M_value_initialized(),
			      _M_message(__msg_copy_singular)
			      ._M_iterator(*this, "this")
			      ._M_iterator(__x, "other"));

	if (this->_M_sequence && this->_M_sequence == __x._M_sequence)
	  _GLIBCXX20_CONSTEXPR_NON_LITERAL_SCOPE_BEGIN {
	    __gnu_cxx::__scoped_lock __l(this->_M_get_mutex());
	    base() = __x.base();
	    _M_version = __x._M_sequence->_M_version;
	  } _GLIBCXX20_CONSTEXPR_NON_LITERAL_SCOPE_END
	else
	  {
	    _M_detach();
	    base() = __x.base();
	    _M_attach(__x._M_sequence);
	  }

	return *this;
      }

#if __cplusplus >= 201103L
      /**
       * @brief Move assignment.
       * @post __x is singular and unattached
       */
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator&
      operator=(_Safe_iterator&& __x) noexcept
      {
	if (std::__is_constant_evaluated())
	  {
	    base() = __x.base();
	    return *this;
	  }

	_GLIBCXX_DEBUG_VERIFY(!__x._M_singular()
			      || __x._M_value_initialized(),
			      _M_message(__msg_copy_singular)
			      ._M_iterator(*this, "this")
			      ._M_iterator(__x, "other"));

	if (std::__addressof(__x) == this)
	  return *this;

	if (this->_M_sequence && this->_M_sequence == __x._M_sequence)
	  _GLIBCXX20_CONSTEXPR_NON_LITERAL_SCOPE_BEGIN {
	    __gnu_cxx::__scoped_lock __l(this->_M_get_mutex());
	    base() = __x.base();
	    _M_version = __x._M_sequence->_M_version;
	  } _GLIBCXX20_CONSTEXPR_NON_LITERAL_SCOPE_END
	else
	  {
	    _M_detach();
	    base() = __x.base();
	    _M_attach(__x._M_sequence);
	  }

	__x._M_detach();
	__x.base() = _Iterator();
	return *this;
      }
#endif

      /**
       *  @brief Iterator dereference.
       *  @pre iterator is dereferenceable
       */
      _GLIBCXX_NODISCARD
      _GLIBCXX20_CONSTEXPR
      reference
      operator*() const _GLIBCXX_NOEXCEPT
      {
	if (!std::__is_constant_evaluated())
	  {
	    _GLIBCXX_DEBUG_VERIFY(this->_M_dereferenceable(),
				  _M_message(__msg_bad_deref)
				  ._M_iterator(*this, "this"));
	  }
	return *base();
      }

      /**
       *  @brief Iterator dereference.
       *  @pre iterator is dereferenceable
       */
      _GLIBCXX_NODISCARD
      _GLIBCXX20_CONSTEXPR
      pointer
      operator->() const _GLIBCXX_NOEXCEPT
      {
	if (!std::__is_constant_evaluated())
	  {
	    _GLIBCXX_DEBUG_VERIFY(this->_M_dereferenceable(),
				  _M_message(__msg_bad_deref)
				  ._M_iterator(*this, "this"));
	  }
	return base().operator->();
      }

      // ------ Input iterator requirements ------
      /**
       *  @brief Iterator preincrement
       *  @pre iterator is incrementable
       */
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator&
      operator++() _GLIBCXX_NOEXCEPT
      {
	if (std::__is_constant_evaluated())
	  {
	    ++base();
	    return *this;
	  }

	_GLIBCXX_DEBUG_VERIFY(this->_M_incrementable(),
			      _M_message(__msg_bad_inc)
			      ._M_iterator(*this, "this"));
	_GLIBCXX20_CONSTEXPR_NON_LITERAL_SCOPE_BEGIN {
	  __gnu_cxx::__scoped_lock __l(this->_M_get_mutex());
	  ++base();
	} _GLIBCXX20_CONSTEXPR_NON_LITERAL_SCOPE_END
	return *this;
      }

      /**
       *  @brief Iterator postincrement
       *  @pre iterator is incrementable
       */
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator
      operator++(int) _GLIBCXX_NOEXCEPT
      {
	if (!std::__is_constant_evaluated())
	  {
	    _GLIBCXX_DEBUG_VERIFY(this->_M_incrementable(),
				  _M_message(__msg_bad_inc)
				  ._M_iterator(*this, "this"));
	  }
	_Safe_iterator __ret(*this, _Unchecked());
	++*this;
	return __ret;
      }

      // ------ Utilities ------

      /// Determine if this is a constant iterator.
      static _GLIBCXX_CONSTEXPR bool
      _S_constant()
      { return _IsConstant::__value; }

      /**
       * @brief Return the underlying iterator
       */
      _GLIBCXX20_CONSTEXPR
      _Iterator&
      base() _GLIBCXX_NOEXCEPT { return *this; }

      _GLIBCXX20_CONSTEXPR
      const _Iterator&
      base() const _GLIBCXX_NOEXCEPT { return *this; }

      /**
       * @brief Conversion to underlying non-debug iterator to allow
       * better interaction with non-debug containers.
       */
      _GLIBCXX20_CONSTEXPR
      operator _Iterator() const _GLIBCXX_NOEXCEPT { return *this; }

      /** Attach iterator to the given sequence. */
      void
      _M_attach(const _Safe_sequence_base* __seq)
      { _Safe_base::_M_attach(__seq, _S_constant()); }

      /** Likewise, but not thread-safe. */
      void
      _M_attach_single(const _Safe_sequence_base* __seq)
      { _Safe_base::_M_attach_single(__seq, _S_constant()); }

      /// Is the iterator dereferenceable?
      bool
      _M_dereferenceable() const
      { return !this->_M_singular() && !_M_is_end() && !_M_is_before_begin(); }

      /// Is the iterator before a dereferenceable one?
      bool
      _M_before_dereferenceable() const
      {
	if (this->_M_incrementable())
	{
	  _Iterator __base = base();
	  return ++__base != _M_get_sequence()->_M_base().end();
	}
	return false;
      }

      /// Is the iterator incrementable?
      bool
      _M_incrementable() const
      { return !this->_M_singular() && !_M_is_end(); }

      /// Is the iterator value-initialized?
      bool
      _M_value_initialized() const
      { return _M_version == 0 && base() == _Iter_base(); }

      // Can we advance the iterator @p __n steps (@p __n may be negative)
      bool
      _M_can_advance(difference_type __n, bool __strict = false) const;

      // Can we advance the iterator using @p __dist in @p __way direction.
      template<typename _Diff>
	bool
	_M_can_advance(const std::pair<_Diff, _Distance_precision>& __dist,
		       int __way) const;

      // Is the iterator range [*this, __rhs) valid?
      bool
      _M_valid_range(const _Safe_iterator& __rhs,
		     std::pair<difference_type, _Distance_precision>& __dist,
		     bool __check_dereferenceable = true) const;

      // The sequence this iterator references.
      typename __gnu_cxx::__conditional_type<
	_IsConstant::__value, const _Sequence*, _Sequence*>::__type
      _M_get_sequence() const
      {
	// Looks like not const-correct, but if _IsConstant the constness
	// is restored when returning the sequence pointer and if not
	// _IsConstant we are allowed to remove constness.
	return static_cast<_Sequence*>
	  (const_cast<_Safe_sequence_base*>(_M_sequence));
      }

      // Get distance to __rhs.
      typename _Distance_traits<_Iterator>::__type
      _M_get_distance_to(const _Safe_iterator& __rhs) const;

      // Get distance from sequence begin up to *this.
      typename _Distance_traits<_Iterator>::__type
      _M_get_distance_from_begin() const;

      // Get distance from *this to sequence end.
      typename _Distance_traits<_Iterator>::__type
      _M_get_distance_to_end() const;

      /// Is this iterator equal to the sequence's begin() iterator?
      _GLIBCXX20_CONSTEXPR
      bool
      _M_is_begin() const
      { return base() == _M_get_sequence()->_M_base().begin(); }

      /// Is this iterator equal to the sequence's end() iterator?
      bool
      _M_is_end() const
      { return base() == _M_get_sequence()->_M_base().end(); }

      /// Is this iterator equal to the sequence's before_begin() iterator if
      /// any?
      bool
      _M_is_before_begin() const
      { return _BeforeBeginHelper<_Sequence>::_S_Is(*this); }

      /// Is this iterator equal to the sequence's before_begin() iterator if
      /// any or begin() otherwise?
      bool
      _M_is_beginnest() const
      { return _BeforeBeginHelper<_Sequence>::_S_Is_Beginnest(*this); }

      // ------ Operators ------

      typedef _Safe_iterator<_Iterator, _Sequence, iterator_category> _Self;

      _GLIBCXX_NODISCARD
      _GLIBCXX20_CONSTEXPR
      friend bool
      operator==(const _Self& __lhs, const _Self& __rhs) _GLIBCXX_NOEXCEPT
      {
	_GLIBCXX_DEBUG_VERIFY_EQ_OPERANDS(__lhs, __rhs);
	return __lhs.base() == __rhs.base();
      }

      template<typename _IteR>
	_GLIBCXX_NODISCARD
	_GLIBCXX20_CONSTEXPR
	friend bool
	operator==(const _Self& __lhs,
	  const _Safe_iterator<_IteR, _Sequence, iterator_category>& __rhs)
	_GLIBCXX_NOEXCEPT
	{
	  _GLIBCXX_DEBUG_VERIFY_EQ_OPERANDS(__lhs, __rhs);
	  return __lhs.base() == __rhs.base();
	}

#if ! __cpp_lib_three_way_comparison
      _GLIBCXX_NODISCARD
      friend bool
      operator!=(const _Self& __lhs, const _Self& __rhs) _GLIBCXX_NOEXCEPT
      {
	_GLIBCXX_DEBUG_VERIFY_EQ_OPERANDS(__lhs, __rhs);
	return __lhs.base() != __rhs.base();
      }

      template<typename _IteR>
	_GLIBCXX_NODISCARD
	friend bool
	operator!=(const _Self& __lhs,
	  const _Safe_iterator<_IteR, _Sequence, iterator_category>& __rhs)
	_GLIBCXX_NOEXCEPT
	{
	  _GLIBCXX_DEBUG_VERIFY_EQ_OPERANDS(__lhs, __rhs);
	  return __lhs.base() != __rhs.base();
	}
#endif // three-way comparison
    };

  template<typename _Iterator, typename _Sequence>
    class _Safe_iterator<_Iterator, _Sequence, std::bidirectional_iterator_tag>
    : public _Safe_iterator<_Iterator, _Sequence, std::forward_iterator_tag>
    {
      typedef _Safe_iterator<_Iterator, _Sequence,
			     std::forward_iterator_tag> _Safe_base;

    protected:
      typedef typename _Safe_base::_OtherIterator _OtherIterator;

      typedef typename _Safe_base::_Unchecked _Unchecked;

      _GLIBCXX20_CONSTEXPR
      _Safe_iterator(const _Safe_iterator& __x,
		     _Unchecked __unchecked) _GLIBCXX_NOEXCEPT
	: _Safe_base(__x, __unchecked)
      { }

    public:
      /// @post the iterator is singular and unattached
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator() _GLIBCXX_NOEXCEPT { }

      /**
       * @brief Safe iterator construction from an unsafe iterator and
       * its sequence.
       *
       * @pre @p seq is not NULL
       * @post this is not singular
       */
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator(_Iterator __i, const _Safe_sequence_base* __seq)
      _GLIBCXX_NOEXCEPT
      : _Safe_base(__i, __seq)
      { }

      /**
       * @brief Copy construction.
       */
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator(const _Safe_iterator& __x) _GLIBCXX_NOEXCEPT
      : _Safe_base(__x)
      { }

#if __cplusplus >= 201103L
      /** @brief Move construction. */
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator(_Safe_iterator&&) = default;
#endif

      /**
       *  @brief Converting constructor from a mutable iterator to a
       *  constant iterator.
      */
      template<typename _MutableIterator>
	_GLIBCXX20_CONSTEXPR
	_Safe_iterator(
	  const _Safe_iterator<_MutableIterator, _Sequence,
	    typename __gnu_cxx::__enable_if<_Safe_base::_IsConstant::__value &&
	      std::__are_same<_MutableIterator, _OtherIterator>::__value,
			       std::bidirectional_iterator_tag>::__type>& __x)
	_GLIBCXX_NOEXCEPT
	: _Safe_base(__x)
        { }

#if __cplusplus >= 201103L
      /** @brief Copy assignment. */
      _Safe_iterator&
      operator=(const _Safe_iterator&) = default;

      /** @brief Move assignment. */
      _Safe_iterator&
      operator=(_Safe_iterator&&) = default;
#else
      /** @brief Copy assignment. */
      _Safe_iterator&
      operator=(const _Safe_iterator& __x)
      {
	_Safe_base::operator=(__x);
	return *this;
      }
#endif

      // ------ Input iterator requirements ------
      /**
       *  @brief Iterator preincrement
       *  @pre iterator is incrementable
       */
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator&
      operator++() _GLIBCXX_NOEXCEPT
      {
	_Safe_base::operator++();
	return *this;
      }

      /**
       *  @brief Iterator postincrement
       *  @pre iterator is incrementable
       */
      _Safe_iterator
      operator++(int) _GLIBCXX_NOEXCEPT
      {
	_GLIBCXX_DEBUG_VERIFY(this->_M_incrementable(),
			      _M_message(__msg_bad_inc)
			      ._M_iterator(*this, "this"));
	_Safe_iterator __ret(*this, _Unchecked());
	++*this;
	return __ret;
      }

      // ------ Bidirectional iterator requirements ------
      /**
       *  @brief Iterator predecrement
       *  @pre iterator is decrementable
       */
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator&
      operator--() _GLIBCXX_NOEXCEPT
      {
	if (std::__is_constant_evaluated())
	  {
	    --this->base();
	    return *this;
	  }

	_GLIBCXX_DEBUG_VERIFY(this->_M_decrementable(),
			      _M_message(__msg_bad_dec)
			      ._M_iterator(*this, "this"));
	_GLIBCXX20_CONSTEXPR_NON_LITERAL_SCOPE_BEGIN {
	  __gnu_cxx::__scoped_lock __l(this->_M_get_mutex());
	  --this->base();
	} _GLIBCXX20_CONSTEXPR_NON_LITERAL_SCOPE_END
	return *this;
      }

      /**
       *  @brief Iterator postdecrement
       *  @pre iterator is decrementable
       */
      _Safe_iterator
      operator--(int) _GLIBCXX_NOEXCEPT
      {
	_GLIBCXX_DEBUG_VERIFY(this->_M_decrementable(),
			      _M_message(__msg_bad_dec)
			      ._M_iterator(*this, "this"));
	_Safe_iterator __ret(*this, _Unchecked());
	--*this;
	return __ret;
      }

      // ------ Utilities ------

      // Is the iterator decrementable?
      bool
      _M_decrementable() const
      { return !this->_M_singular() && !this->_M_is_begin(); }
    };

  template<typename _Iterator, typename _Sequence>
    class _Safe_iterator<_Iterator, _Sequence, std::random_access_iterator_tag>
    : public _Safe_iterator<_Iterator, _Sequence,
			    std::bidirectional_iterator_tag>
    {
      typedef _Safe_iterator<_Iterator, _Sequence,
			     std::bidirectional_iterator_tag> _Safe_base;
      typedef typename _Safe_base::_OtherIterator _OtherIterator;

      typedef typename _Safe_base::_Self _Self;
      typedef _Safe_iterator<_OtherIterator, _Sequence,
			     std::random_access_iterator_tag> _OtherSelf;

      typedef typename _Safe_base::_Unchecked _Unchecked;

      _GLIBCXX20_CONSTEXPR
      _Safe_iterator(const _Safe_iterator& __x,
		     _Unchecked __unchecked) _GLIBCXX_NOEXCEPT
	: _Safe_base(__x, __unchecked)
      { }

    public:
      typedef typename _Safe_base::difference_type	difference_type;
      typedef typename _Safe_base::reference		reference;

      /// @post the iterator is singular and unattached
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator() _GLIBCXX_NOEXCEPT { }

      /**
       * @brief Safe iterator construction from an unsafe iterator and
       * its sequence.
       *
       * @pre @p seq is not NULL
       * @post this is not singular
       */
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator(_Iterator __i, const _Safe_sequence_base* __seq)
      _GLIBCXX_NOEXCEPT
      : _Safe_base(__i, __seq)
      { }

      /**
       * @brief Copy construction.
       */
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator(const _Safe_iterator& __x) _GLIBCXX_NOEXCEPT
      : _Safe_base(__x)
      { }

#if __cplusplus >= 201103L
      /** @brief Move construction. */
      _Safe_iterator(_Safe_iterator&&) = default;
#endif

      /**
       *  @brief Converting constructor from a mutable iterator to a
       *  constant iterator.
      */
      template<typename _MutableIterator>
	_GLIBCXX20_CONSTEXPR
	_Safe_iterator(
	  const _Safe_iterator<_MutableIterator, _Sequence,
	    typename __gnu_cxx::__enable_if<_Safe_base::_IsConstant::__value &&
	      std::__are_same<_MutableIterator, _OtherIterator>::__value,
			       std::random_access_iterator_tag>::__type>& __x)
	_GLIBCXX_NOEXCEPT
	: _Safe_base(__x)
        { }

#if __cplusplus >= 201103L
      /** @brief Copy assignment. */
      _Safe_iterator&
      operator=(const _Safe_iterator&) = default;

      /** @brief Move assignment. */
      _Safe_iterator&
      operator=(_Safe_iterator&&) = default;
#else
      /** @brief Copy assignment. */
      _Safe_iterator&
      operator=(const _Safe_iterator& __x)
      {
	_Safe_base::operator=(__x);
	return *this;
      }
#endif

      // Is the iterator range [*this, __rhs) valid?
      bool
      _M_valid_range(const _Safe_iterator& __rhs,
		     std::pair<difference_type,
			       _Distance_precision>& __dist) const;

      // ------ Input iterator requirements ------
      /**
       *  @brief Iterator preincrement
       *  @pre iterator is incrementable
       */
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator&
      operator++() _GLIBCXX_NOEXCEPT
      {
	_Safe_base::operator++();
	return *this;
      }

      /**
       *  @brief Iterator postincrement
       *  @pre iterator is incrementable
       */
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator
      operator++(int) _GLIBCXX_NOEXCEPT
      {
	if (!std::__is_constant_evaluated())
	  {
	    _GLIBCXX_DEBUG_VERIFY(this->_M_incrementable(),
				  _M_message(__msg_bad_inc)
				  ._M_iterator(*this, "this"));
	  }
	_Safe_iterator __ret(*this, _Unchecked());
	++*this;
	return __ret;
      }

      // ------ Bidirectional iterator requirements ------
      /**
       *  @brief Iterator predecrement
       *  @pre iterator is decrementable
       */
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator&
      operator--() _GLIBCXX_NOEXCEPT
      {
	_Safe_base::operator--();
	return *this;
      }

      /**
       *  @brief Iterator postdecrement
       *  @pre iterator is decrementable
       */
      _GLIBCXX20_CONSTEXPR
      _Safe_iterator
      operator--(int) _GLIBCXX_NOEXCEPT
      {
	if (!std::__is_constant_evaluated())
	  {
	    _GLIBCXX_DEBUG_VERIFY(this->_M_decrementable(),
				  _M_message(__msg_bad_dec)
				  ._M_iterator(*this, "this"));
	  }
	_Safe_iterator __ret(*this, _Unchecked());
	--*this;
	return __ret;
      }

      // ------ Random access iterator requirements ------
      _GLIBCXX_NODISCARD
      _GLIBCXX20_CONSTEXPR
      reference
      operator[](difference_type __n) const _GLIBCXX_NOEXCEPT
      {
	if (!std::__is_constant_evaluated())
	  {
	    _GLIBCXX_DEBUG_VERIFY(this->_M_can_advance(__n)
				  && this->_M_can_advance(__n + 1),
				  _M_message(__msg_iter_subscript_oob)
				  ._M_iterator(*this)._M_integer(__n));
	  }
	return this->base()[__n];
      }

      _GLIBCXX20_CONSTEXPR
      _Safe_iterator&
      operator+=(difference_type __n) _GLIBCXX_NOEXCEPT
      {
	if (std::__is_constant_evaluated())
	  {
	    this->base() += __n;
	    return *this;
	  }

	_GLIBCXX_DEBUG_VERIFY(this->_M_can_advance(__n),
			      _M_message(__msg_advance_oob)
			      ._M_iterator(*this)._M_integer(__n));
	_GLIBCXX20_CONSTEXPR_NON_LITERAL_SCOPE_BEGIN {
	  __gnu_cxx::__scoped_lock __l(this->_M_get_mutex());
	  this->base() += __n;
	} _GLIBCXX20_CONSTEXPR_NON_LITERAL_SCOPE_END
	return *this;
      }

      _GLIBCXX20_CONSTEXPR
      _Safe_iterator&
      operator-=(difference_type __n) _GLIBCXX_NOEXCEPT
      {
	if (std::__is_constant_evaluated())
	  {
	    this->base() -= __n;
	    return *this;
	  }

	_GLIBCXX_DEBUG_VERIFY(this->_M_can_advance(-__n),
			      _M_message(__msg_retreat_oob)
			      ._M_iterator(*this)._M_integer(__n));
	_GLIBCXX20_CONSTEXPR_NON_LITERAL_SCOPE_BEGIN {
	  __gnu_cxx::__scoped_lock __l(this->_M_get_mutex());
	  this->base() -= __n;
	} _GLIBCXX20_CONSTEXPR_NON_LITERAL_SCOPE_END
	return *this;
      }

#if __cpp_lib_three_way_comparison
      [[nodiscard]]
      _GLIBCXX20_CONSTEXPR
      friend auto
      operator<=>(const _Self& __lhs, const _Self& __rhs) noexcept
      {
	_GLIBCXX_DEBUG_VERIFY_REL_OPERANDS(__lhs, __rhs);
	return __lhs.base() <=> __rhs.base();
      }

      [[nodiscard]]
      _GLIBCXX20_CONSTEXPR
      friend auto
      operator<=>(const _Self& __lhs, const _OtherSelf& __rhs) noexcept
      {
	_GLIBCXX_DEBUG_VERIFY_REL_OPERANDS(__lhs, __rhs);
	return __lhs.base() <=> __rhs.base();
      }
#else
      _GLIBCXX_NODISCARD
      friend bool
      operator<(const _Self& __lhs, const _Self& __rhs) _GLIBCXX_NOEXCEPT
      {
	_GLIBCXX_DEBUG_VERIFY_REL_OPERANDS(__lhs, __rhs);
	return __lhs.base() < __rhs.base();
      }

      _GLIBCXX_NODISCARD
      friend bool
      operator<(const _Self& __lhs, const _OtherSelf& __rhs) _GLIBCXX_NOEXCEPT
      {
	_GLIBCXX_DEBUG_VERIFY_REL_OPERANDS(__lhs, __rhs);
	return __lhs.base() < __rhs.base();
      }

      _GLIBCXX_NODISCARD
      friend bool
      operator<=(const _Self& __lhs, const _Self& __rhs) _GLIBCXX_NOEXCEPT
      {
	_GLIBCXX_DEBUG_VERIFY_REL_OPERANDS(__lhs, __rhs);
	return __lhs.base() <= __rhs.base();
      }

      _GLIBCXX_NODISCARD
      friend bool
      operator<=(const _Self& __lhs, const _OtherSelf& __rhs) _GLIBCXX_NOEXCEPT
      {
	_GLIBCXX_DEBUG_VERIFY_REL_OPERANDS(__lhs, __rhs);
	return __lhs.base() <= __rhs.base();
      }

      _GLIBCXX_NODISCARD
      friend bool
      operator>(const _Self& __lhs, const _Self& __rhs) _GLIBCXX_NOEXCEPT
      {
	_GLIBCXX_DEBUG_VERIFY_REL_OPERANDS(__lhs, __rhs);
	return __lhs.base() > __rhs.base();
      }

      _GLIBCXX_NODISCARD
      friend bool
      operator>(const _Self& __lhs, const _OtherSelf& __rhs) _GLIBCXX_NOEXCEPT
      {
	_GLIBCXX_DEBUG_VERIFY_REL_OPERANDS(__lhs, __rhs);
	return __lhs.base() > __rhs.base();
      }

      _GLIBCXX_NODISCARD
      friend bool
      operator>=(const _Self& __lhs, const _Self& __rhs) _GLIBCXX_NOEXCEPT
      {
	_GLIBCXX_DEBUG_VERIFY_REL_OPERANDS(__lhs, __rhs);
	return __lhs.base() >= __rhs.base();
      }

      _GLIBCXX_NODISCARD
      friend bool
      operator>=(const _Self& __lhs, const _OtherSelf& __rhs) _GLIBCXX_NOEXCEPT
      {
	_GLIBCXX_DEBUG_VERIFY_REL_OPERANDS(__lhs, __rhs);
	return __lhs.base() >= __rhs.base();
      }
#endif // three-way comparison

      // _GLIBCXX_RESOLVE_LIB_DEFECTS
      // According to the resolution of DR179 not only the various comparison
      // operators but also operator- must accept mixed iterator/const_iterator
      // parameters.
      _GLIBCXX_NODISCARD
      _GLIBCXX20_CONSTEXPR
      friend difference_type
      operator-(const _Self& __lhs, const _OtherSelf& __rhs) _GLIBCXX_NOEXCEPT
      {
	_GLIBCXX_DEBUG_VERIFY_DIST_OPERANDS(__lhs, __rhs);
	return __lhs.base() - __rhs.base();
      }

      _GLIBCXX_NODISCARD
      _GLIBCXX20_CONSTEXPR
      friend difference_type
      operator-(const _Self& __lhs, const _Self& __rhs) _GLIBCXX_NOEXCEPT
      {
	_GLIBCXX_DEBUG_VERIFY_DIST_OPERANDS(__lhs, __rhs);
	return __lhs.base() - __rhs.base();
      }

      _GLIBCXX_NODISCARD
      _GLIBCXX20_CONSTEXPR
      friend _Self
      operator+(const _Self& __x, difference_type __n) _GLIBCXX_NOEXCEPT
      {
	if (!std::__is_constant_evaluated())
	  {
	    _GLIBCXX_DEBUG_VERIFY(__x._M_can_advance(__n),
				  _M_message(__msg_advance_oob)
				  ._M_iterator(__x)._M_integer(__n));
	  }
	return _Safe_iterator(__x.base() + __n, __x._M_sequence);
      }

      _GLIBCXX_NODISCARD
      _GLIBCXX20_CONSTEXPR
      friend _Self
      operator+(difference_type __n, const _Self& __x) _GLIBCXX_NOEXCEPT
      {
	if (!std::__is_constant_evaluated())
	  {
	    _GLIBCXX_DEBUG_VERIFY(__x._M_can_advance(__n),
				  _M_message(__msg_advance_oob)
				  ._M_iterator(__x)._M_integer(__n));
	  }
	return _Safe_iterator(__n + __x.base(), __x._M_sequence);
      }

      _GLIBCXX_NODISCARD
      _GLIBCXX20_CONSTEXPR
      friend _Self
      operator-(const _Self& __x, difference_type __n) _GLIBCXX_NOEXCEPT
      {
	if (!std::__is_constant_evaluated())
	  {
	    _GLIBCXX_DEBUG_VERIFY(__x._M_can_advance(-__n),
				  _M_message(__msg_retreat_oob)
				  ._M_iterator(__x)._M_integer(__n));
	  }
	return _Safe_iterator(__x.base() - __n, __x._M_sequence);
      }
    };

  /** Safe iterators know how to check if they form a valid range. */
  template<typename _Iterator, typename _Sequence, typename _Category>
    inline bool
    __valid_range(const _Safe_iterator<_Iterator, _Sequence,
				       _Category>& __first,
		  const _Safe_iterator<_Iterator, _Sequence,
				       _Category>& __last,
		  typename _Distance_traits<_Iterator>::__type& __dist)
    { return __first._M_valid_range(__last, __dist); }

  template<typename _Iterator, typename _Sequence, typename _Category>
    inline bool
    __valid_range(const _Safe_iterator<_Iterator, _Sequence,
				       _Category>& __first,
		  const _Safe_iterator<_Iterator, _Sequence,
				       _Category>& __last)
    {
      typename _Distance_traits<_Iterator>::__type __dist;
      return __first._M_valid_range(__last, __dist);
    }

  template<typename _Iterator, typename _Sequence, typename _Category,
	   typename _Size>
    inline bool
    __can_advance(const _Safe_iterator<_Iterator, _Sequence, _Category>& __it,
		  _Size __n)
    { return __it._M_can_advance(__n); }

  template<typename _Iterator, typename _Sequence, typename _Category,
	   typename _Diff>
    inline bool
    __can_advance(const _Safe_iterator<_Iterator, _Sequence, _Category>& __it,
		  const std::pair<_Diff, _Distance_precision>& __dist,
		  int __way)
    { return __it._M_can_advance(__dist, __way); }

  template<typename _Iterator, typename _Sequence>
    _Iterator
    __base(const _Safe_iterator<_Iterator, _Sequence,
				std::random_access_iterator_tag>& __it)
    { return __it.base(); }

#if __cplusplus < 201103L
  template<typename _Iterator, typename _Sequence>
    struct _Unsafe_type<_Safe_iterator<_Iterator, _Sequence> >
    { typedef _Iterator _Type; };
#endif

  template<typename _Iterator, typename _Sequence>
    inline _Iterator
    __unsafe(const _Safe_iterator<_Iterator, _Sequence>& __it)
    { return __it.base(); }

} // namespace __gnu_debug

#undef _GLIBCXX20_CONSTEXPR_NON_LITERAL_SCOPE_END
#undef _GLIBCXX20_CONSTEXPR_NON_LITERAL_SCOPE_BEGIN
#undef _GLIBCXX_DEBUG_VERIFY_DIST_OPERANDS
#undef _GLIBCXX_DEBUG_VERIFY_REL_OPERANDS
#undef _GLIBCXX_DEBUG_VERIFY_EQ_OPERANDS
#undef _GLIBCXX_DEBUG_VERIFY_OPERANDS

#include <debug/safe_iterator.tcc>

#endif
