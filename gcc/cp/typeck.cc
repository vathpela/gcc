/* Build expressions with type checking for C++ compiler.
   Copyright (C) 1987-2025 Free Software Foundation, Inc.
   Hacked by Michael Tiemann (tiemann@cygnus.com)

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */


/* This file is part of the C++ front end.
   It contains routines to build C++ expressions given their operands,
   including computing the types of the result, C and C++ specific error
   checks, and some optimization.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "target.h"
#include "cp-tree.h"
#include "stor-layout.h"
#include "varasm.h"
#include "intl.h"
#include "convert.h"
#include "c-family/c-objc.h"
#include "c-family/c-ubsan.h"
#include "c-family/c-type-mismatch.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "gimplify.h"

static tree cp_build_addr_expr_strict (tree, tsubst_flags_t);
static tree cp_build_function_call (tree, tree, tsubst_flags_t);
static tree pfn_from_ptrmemfunc (tree);
static tree delta_from_ptrmemfunc (tree);
static tree convert_for_assignment (tree, tree, impl_conv_rhs, tree, int,
				    tsubst_flags_t, int);
static tree cp_pointer_int_sum (location_t, enum tree_code, tree, tree,
				tsubst_flags_t);
static tree rationalize_conditional_expr (enum tree_code, tree,
					  tsubst_flags_t);
static bool comp_ptr_ttypes_real (tree, tree, int);
static bool comp_except_types (tree, tree, bool);
static bool comp_array_types (const_tree, const_tree, compare_bounds_t, bool);
static tree pointer_diff (location_t, tree, tree, tree, tsubst_flags_t, tree *);
static tree get_delta_difference (tree, tree, bool, bool, tsubst_flags_t);
static void casts_away_constness_r (tree *, tree *, tsubst_flags_t);
static bool casts_away_constness (tree, tree, tsubst_flags_t);
static bool maybe_warn_about_returning_address_of_local (tree, location_t = UNKNOWN_LOCATION);
static void error_args_num (location_t, tree, bool);
static int convert_arguments (tree, vec<tree, va_gc> **, tree, int,
                              tsubst_flags_t);
static bool is_std_move_p (tree);
static bool is_std_forward_p (tree);

/* Do `exp = require_complete_type (exp);' to make sure exp
   does not have an incomplete type.  (That includes void types.)
   Returns error_mark_node if the VALUE does not have
   complete type when this function returns.  */

tree
require_complete_type (tree value,
		       tsubst_flags_t complain /* = tf_warning_or_error */)
{
  tree type;

  if (processing_template_decl || value == error_mark_node)
    return value;

  if (TREE_CODE (value) == OVERLOAD)
    type = unknown_type_node;
  else
    type = TREE_TYPE (value);

  if (type == error_mark_node)
    return error_mark_node;

  /* First, detect a valid value with a complete type.  */
  if (COMPLETE_TYPE_P (type))
    return value;

  if (complete_type_or_maybe_complain (type, value, complain))
    return value;
  else
    return error_mark_node;
}

/* Try to complete TYPE, if it is incomplete.  For example, if TYPE is
   a template instantiation, do the instantiation.  Returns TYPE,
   whether or not it could be completed, unless something goes
   horribly wrong, in which case the error_mark_node is returned.  */

tree
complete_type (tree type)
{
  if (type == NULL_TREE)
    /* Rather than crash, we return something sure to cause an error
       at some point.  */
    return error_mark_node;

  if (type == error_mark_node || COMPLETE_TYPE_P (type))
    ;
  else if (TREE_CODE (type) == ARRAY_TYPE)
    {
      tree t = complete_type (TREE_TYPE (type));
      unsigned int needs_constructing, has_nontrivial_dtor;
      if (COMPLETE_TYPE_P (t) && !dependent_type_p (type))
	layout_type (type);
      needs_constructing
	= TYPE_NEEDS_CONSTRUCTING (TYPE_MAIN_VARIANT (t));
      has_nontrivial_dtor
	= TYPE_HAS_NONTRIVIAL_DESTRUCTOR (TYPE_MAIN_VARIANT (t));
      for (t = TYPE_MAIN_VARIANT (type); t; t = TYPE_NEXT_VARIANT (t))
	{
	  TYPE_NEEDS_CONSTRUCTING (t) = needs_constructing;
	  TYPE_HAS_NONTRIVIAL_DESTRUCTOR (t) = has_nontrivial_dtor;
	}
    }
  else if (CLASS_TYPE_P (type))
    {
      if (modules_p ())
	/* TYPE could be a class member we've not loaded the definition of.  */
	lazy_load_pendings (TYPE_NAME (TYPE_MAIN_VARIANT (type)));

      if (CLASSTYPE_TEMPLATE_INSTANTIATION (type))
	instantiate_class_template (TYPE_MAIN_VARIANT (type));
    }

  return type;
}

/* Like complete_type, but issue an error if the TYPE cannot be completed.
   VALUE is used for informative diagnostics.
   Returns NULL_TREE if the type cannot be made complete.  */

tree
complete_type_or_maybe_complain (tree type, tree value, tsubst_flags_t complain)
{
  type = complete_type (type);
  if (type == error_mark_node)
    /* We already issued an error.  */
    return NULL_TREE;
  else if (!COMPLETE_TYPE_P (type))
    {
      if (complain & tf_error)
	cxx_incomplete_type_diagnostic (value, type, diagnostics::kind::error);
      note_failed_type_completion (type, complain);
      return NULL_TREE;
    }
  else
    return type;
}

tree
complete_type_or_else (tree type, tree value)
{
  return complete_type_or_maybe_complain (type, value, tf_warning_or_error);
}


/* Return the common type of two parameter lists.
   We assume that comptypes has already been done and returned 1;
   if that isn't so, this may crash.

   As an optimization, free the space we allocate if the parameter
   lists are already common.  */

static tree
commonparms (tree p1, tree p2)
{
  tree oldargs = p1, newargs, n;
  int i, len;
  int any_change = 0;

  len = list_length (p1);
  newargs = tree_last (p1);

  if (newargs == void_list_node)
    i = 1;
  else
    {
      i = 0;
      newargs = 0;
    }

  for (; i < len; i++)
    newargs = tree_cons (NULL_TREE, NULL_TREE, newargs);

  n = newargs;

  for (i = 0; p1;
       p1 = TREE_CHAIN (p1), p2 = TREE_CHAIN (p2), n = TREE_CHAIN (n), i++)
    {
      if (TREE_PURPOSE (p1) && !TREE_PURPOSE (p2))
	{
	  TREE_PURPOSE (n) = TREE_PURPOSE (p1);
	  any_change = 1;
	}
      else if (! TREE_PURPOSE (p1))
	{
	  if (TREE_PURPOSE (p2))
	    {
	      TREE_PURPOSE (n) = TREE_PURPOSE (p2);
	      any_change = 1;
	    }
	}
      else
	{
	  if (simple_cst_equal (TREE_PURPOSE (p1), TREE_PURPOSE (p2)) != 1)
	    any_change = 1;
	  TREE_PURPOSE (n) = TREE_PURPOSE (p2);
	}
      if (TREE_VALUE (p1) != TREE_VALUE (p2))
	{
	  any_change = 1;
	  TREE_VALUE (n) = merge_types (TREE_VALUE (p1), TREE_VALUE (p2));
	}
      else
	TREE_VALUE (n) = TREE_VALUE (p1);
    }
  if (! any_change)
    return oldargs;

  return newargs;
}

/* Given a type, perhaps copied for a typedef,
   find the "original" version of it.  */
static tree
original_type (tree t)
{
  int quals = cp_type_quals (t);
  while (t != error_mark_node
	 && TYPE_NAME (t) != NULL_TREE)
    {
      tree x = TYPE_NAME (t);
      if (TREE_CODE (x) != TYPE_DECL)
	break;
      x = DECL_ORIGINAL_TYPE (x);
      if (x == NULL_TREE)
	break;
      t = x;
    }
  return cp_build_qualified_type (t, quals);
}

/* Merge the attributes of type OTHER_TYPE into the attributes of type TYPE
   and return a variant of TYPE with the merged attributes.  */

static tree
merge_type_attributes_from (tree type, tree other_type)
{
  tree attrs = targetm.merge_type_attributes (type, other_type);
  attrs = restrict_type_identity_attributes_to (attrs, TYPE_ATTRIBUTES (type));
  return cp_build_type_attribute_variant (type, attrs);
}

/* Compare floating point conversion ranks and subranks of T1 and T2
   types.  If T1 and T2 have unordered conversion ranks, return 3.
   If T1 has greater conversion rank than T2, return 2.
   If T2 has greater conversion rank than T1, return -2.
   If T1 has equal conversion rank as T2, return -1, 0 or 1 depending
   on if T1 has smaller, equal or greater conversion subrank than
   T2.  */

int
cp_compare_floating_point_conversion_ranks (tree t1, tree t2)
{
  tree mv1 = TYPE_MAIN_VARIANT (t1);
  tree mv2 = TYPE_MAIN_VARIANT (t2);
  int extended1 = 0;
  int extended2 = 0;

  if (mv1 == mv2)
    return 0;

  for (int i = 0; i < NUM_FLOATN_NX_TYPES; ++i)
    {
      if (mv1 == FLOATN_NX_TYPE_NODE (i))
	extended1 = i + 1;
      if (mv2 == FLOATN_NX_TYPE_NODE (i))
	extended2 = i + 1;
    }
  if (mv1 == bfloat16_type_node)
    extended1 = true;
  if (mv2 == bfloat16_type_node)
    extended2 = true;
  if (extended2 && !extended1)
    {
      int ret = cp_compare_floating_point_conversion_ranks (t2, t1);
      return ret == 3 ? 3 : -ret;
    }

  const struct real_format *fmt1 = REAL_MODE_FORMAT (TYPE_MODE (t1));
  const struct real_format *fmt2 = REAL_MODE_FORMAT (TYPE_MODE (t2));
  gcc_assert (fmt1->b == 2 && fmt2->b == 2);
  /* For {ibm,mips}_extended_format formats, the type has variable
     precision up to ~2150 bits when the first double is around maximum
     representable double and second double is subnormal minimum.
     So, e.g. for __ibm128 vs. std::float128_t, they have unordered
     ranks.  */
  int p1 = (MODE_COMPOSITE_P (TYPE_MODE (t1))
	    ? fmt1->emax - fmt1->emin + fmt1->p - 1 : fmt1->p);
  int p2 = (MODE_COMPOSITE_P (TYPE_MODE (t2))
	    ? fmt2->emax - fmt2->emin + fmt2->p - 1 : fmt2->p);
  /* The rank of a floating point type T is greater than the rank of
     any floating-point type whose set of values is a proper subset
     of the set of values of T.  */
  if ((p1 > p2 && fmt1->emax >= fmt2->emax)
       || (p1 == p2 && fmt1->emax > fmt2->emax))
    return 2;
  if ((p1 < p2 && fmt1->emax <= fmt2->emax)
       || (p1 == p2 && fmt1->emax < fmt2->emax))
    return -2;
  if ((p1 > p2 && fmt1->emax < fmt2->emax)
       || (p1 < p2 && fmt1->emax > fmt2->emax))
    return 3;
  if (!extended1 && !extended2)
    {
      /* The rank of long double is greater than the rank of double, which
	 is greater than the rank of float.  */
      if (t1 == long_double_type_node)
	return 2;
      else if (t2 == long_double_type_node)
	return -2;
      if (t1 == double_type_node)
	return 2;
      else if (t2 == double_type_node)
	return -2;
      if (t1 == float_type_node)
	return 2;
      else if (t2 == float_type_node)
	return -2;
      return 0;
    }
  /* Two extended floating-point types with the same set of values have equal
     ranks.  */
  if (extended1 && extended2)
    {
      if ((extended1 <= NUM_FLOATN_TYPES) == (extended2 <= NUM_FLOATN_TYPES))
	{
	  /* Prefer higher extendedN value.  */
	  if (extended1 > extended2)
	    return 1;
	  else if (extended1 < extended2)
	    return -1;
	  else
	    return 0;
	}
      else if (extended1 <= NUM_FLOATN_TYPES)
	/* Prefer _FloatN type over _FloatMx type.  */
	return 1;
      else if (extended2 <= NUM_FLOATN_TYPES)
	return -1;
      else
	return 0;
    }

  /* gcc_assert (extended1 && !extended2);  */
  tree *p;
  int cnt = 0;
  for (p = &float_type_node; p <= &long_double_type_node; ++p)
    {
      const struct real_format *fmt3 = REAL_MODE_FORMAT (TYPE_MODE (*p));
      gcc_assert (fmt3->b == 2);
      int p3 = (MODE_COMPOSITE_P (TYPE_MODE (*p))
		? fmt3->emax - fmt3->emin + fmt3->p - 1 : fmt3->p);
      if (p1 == p3 && fmt1->emax == fmt3->emax)
	++cnt;
    }
  /* An extended floating-point type with the same set of values
     as exactly one cv-unqualified standard floating-point type
     has a rank equal to the rank of that standard floating-point
     type.

     An extended floating-point type with the same set of values
     as more than one cv-unqualified standard floating-point type
     has a rank equal to the rank of double.

     Thus, if the latter is true and t2 is long double, t2
     has higher rank.  */
  if (cnt > 1 && mv2 == long_double_type_node)
    return -2;
  /* And similarly if t2 is float, t2 has lower rank.  */
  if (cnt > 1 && mv2 == float_type_node)
    return 2;
  /* Otherwise, they have equal rank, but extended types
     (other than std::bfloat16_t) have higher subrank.
     std::bfloat16_t shouldn't have equal rank to any standard
     floating point type.  */
  return 1;
}

/* Return the common type for two arithmetic types T1 and T2 under the
   usual arithmetic conversions.  The default conversions have already
   been applied, and enumerated types converted to their compatible
   integer types.  */

static tree
cp_common_type (tree t1, tree t2)
{
  enum tree_code code1 = TREE_CODE (t1);
  enum tree_code code2 = TREE_CODE (t2);
  tree attributes;
  int i;


  /* In what follows, we slightly generalize the rules given in [expr] so
     as to deal with `long long' and `complex'.  First, merge the
     attributes.  */
  attributes = (*targetm.merge_type_attributes) (t1, t2);

  if (SCOPED_ENUM_P (t1) || SCOPED_ENUM_P (t2))
    {
      if (TYPE_MAIN_VARIANT (t1) == TYPE_MAIN_VARIANT (t2))
	return build_type_attribute_variant (t1, attributes);
      else
	return NULL_TREE;
    }

  /* FIXME: Attributes.  */
  gcc_assert (ARITHMETIC_TYPE_P (t1)
	      || VECTOR_TYPE_P (t1)
	      || UNSCOPED_ENUM_P (t1));
  gcc_assert (ARITHMETIC_TYPE_P (t2)
	      || VECTOR_TYPE_P (t2)
	      || UNSCOPED_ENUM_P (t2));

  /* If one type is complex, form the common type of the non-complex
     components, then make that complex.  Use T1 or T2 if it is the
     required type.  */
  if (code1 == COMPLEX_TYPE || code2 == COMPLEX_TYPE)
    {
      tree subtype1 = code1 == COMPLEX_TYPE ? TREE_TYPE (t1) : t1;
      tree subtype2 = code2 == COMPLEX_TYPE ? TREE_TYPE (t2) : t2;
      tree subtype
	= type_after_usual_arithmetic_conversions (subtype1, subtype2);

      if (subtype == error_mark_node)
	return subtype;
      if (code1 == COMPLEX_TYPE && TREE_TYPE (t1) == subtype)
	return build_type_attribute_variant (t1, attributes);
      else if (code2 == COMPLEX_TYPE && TREE_TYPE (t2) == subtype)
	return build_type_attribute_variant (t2, attributes);
      else
	return build_type_attribute_variant (build_complex_type (subtype),
					     attributes);
    }

  if (code1 == VECTOR_TYPE)
    {
      /* When we get here we should have two vectors of the same size.
	 Just prefer the unsigned one if present.  */
      if (TYPE_UNSIGNED (t1))
	return merge_type_attributes_from (t1, t2);
      else
	return merge_type_attributes_from (t2, t1);
    }

  /* If only one is real, use it as the result.  */
  if (code1 == REAL_TYPE && code2 != REAL_TYPE)
    return build_type_attribute_variant (t1, attributes);
  if (code2 == REAL_TYPE && code1 != REAL_TYPE)
    return build_type_attribute_variant (t2, attributes);

  if (code1 == REAL_TYPE
      && (extended_float_type_p (t1) || extended_float_type_p (t2)))
    {
      tree mv1 = TYPE_MAIN_VARIANT (t1);
      tree mv2 = TYPE_MAIN_VARIANT (t2);
      if (mv1 == mv2)
	return build_type_attribute_variant (t1, attributes);

      int cmpret = cp_compare_floating_point_conversion_ranks (mv1, mv2);
      if (cmpret == 3)
	return error_mark_node;
      else if (cmpret >= 0)
	return build_type_attribute_variant (t1, attributes);
      else
	return build_type_attribute_variant (t2, attributes);
    }

  /* Both real or both integers; use the one with greater precision.  */
  if (TYPE_PRECISION (t1) > TYPE_PRECISION (t2))
    return build_type_attribute_variant (t1, attributes);
  else if (TYPE_PRECISION (t2) > TYPE_PRECISION (t1))
    return build_type_attribute_variant (t2, attributes);

  /* The types are the same; no need to do anything fancy.  */
  if (TYPE_MAIN_VARIANT (t1) == TYPE_MAIN_VARIANT (t2))
    return build_type_attribute_variant (t1, attributes);

  if (code1 != REAL_TYPE)
    {
      /* If one is unsigned long long, then convert the other to unsigned
	 long long.  */
      if (same_type_p (TYPE_MAIN_VARIANT (t1), long_long_unsigned_type_node)
	  || same_type_p (TYPE_MAIN_VARIANT (t2), long_long_unsigned_type_node))
	return build_type_attribute_variant (long_long_unsigned_type_node,
					     attributes);
      /* If one is a long long, and the other is an unsigned long, and
	 long long can represent all the values of an unsigned long, then
	 convert to a long long.  Otherwise, convert to an unsigned long
	 long.  Otherwise, if either operand is long long, convert the
	 other to long long.

	 Since we're here, we know the TYPE_PRECISION is the same;
	 therefore converting to long long cannot represent all the values
	 of an unsigned long, so we choose unsigned long long in that
	 case.  */
      if (same_type_p (TYPE_MAIN_VARIANT (t1), long_long_integer_type_node)
	  || same_type_p (TYPE_MAIN_VARIANT (t2), long_long_integer_type_node))
	{
	  tree t = ((TYPE_UNSIGNED (t1) || TYPE_UNSIGNED (t2))
		    ? long_long_unsigned_type_node
		    : long_long_integer_type_node);
	  return build_type_attribute_variant (t, attributes);
	}

      /* Go through the same procedure, but for longs.  */
      if (same_type_p (TYPE_MAIN_VARIANT (t1), long_unsigned_type_node)
	  || same_type_p (TYPE_MAIN_VARIANT (t2), long_unsigned_type_node))
	return build_type_attribute_variant (long_unsigned_type_node,
					     attributes);
      if (same_type_p (TYPE_MAIN_VARIANT (t1), long_integer_type_node)
	  || same_type_p (TYPE_MAIN_VARIANT (t2), long_integer_type_node))
	{
	  tree t = ((TYPE_UNSIGNED (t1) || TYPE_UNSIGNED (t2))
		    ? long_unsigned_type_node : long_integer_type_node);
	  return build_type_attribute_variant (t, attributes);
	}

      /* For __intN types, either the type is __int128 (and is lower
	 priority than the types checked above, but higher than other
	 128-bit types) or it's known to not be the same size as other
	 types (enforced in toplev.cc).  Prefer the unsigned type. */
      for (i = 0; i < NUM_INT_N_ENTS; i ++)
	{
	  if (int_n_enabled_p [i]
	      && (same_type_p (TYPE_MAIN_VARIANT (t1), int_n_trees[i].signed_type)
		  || same_type_p (TYPE_MAIN_VARIANT (t2), int_n_trees[i].signed_type)
		  || same_type_p (TYPE_MAIN_VARIANT (t1), int_n_trees[i].unsigned_type)
		  || same_type_p (TYPE_MAIN_VARIANT (t2), int_n_trees[i].unsigned_type)))
	    {
	      tree t = ((TYPE_UNSIGNED (t1) || TYPE_UNSIGNED (t2))
			? int_n_trees[i].unsigned_type
			: int_n_trees[i].signed_type);
	      return build_type_attribute_variant (t, attributes);
	    }
	}

      /* Otherwise prefer the unsigned one.  */
      if (TYPE_UNSIGNED (t1))
	return build_type_attribute_variant (t1, attributes);
      else
	return build_type_attribute_variant (t2, attributes);
    }
  else
    {
      if (same_type_p (TYPE_MAIN_VARIANT (t1), long_double_type_node)
	  || same_type_p (TYPE_MAIN_VARIANT (t2), long_double_type_node))
	return build_type_attribute_variant (long_double_type_node,
					     attributes);
      if (same_type_p (TYPE_MAIN_VARIANT (t1), double_type_node)
	  || same_type_p (TYPE_MAIN_VARIANT (t2), double_type_node))
	return build_type_attribute_variant (double_type_node,
					     attributes);
      if (same_type_p (TYPE_MAIN_VARIANT (t1), float_type_node)
	  || same_type_p (TYPE_MAIN_VARIANT (t2), float_type_node))
	return build_type_attribute_variant (float_type_node,
					     attributes);

      /* Two floating-point types whose TYPE_MAIN_VARIANTs are none of
	 the standard C++ floating-point types.  Logic earlier in this
	 function has already eliminated the possibility that
	 TYPE_PRECISION (t2) != TYPE_PRECISION (t1), so there's no
	 compelling reason to choose one or the other.  */
      return build_type_attribute_variant (t1, attributes);
    }
}

/* T1 and T2 are arithmetic or enumeration types.  Return the type
   that will result from the "usual arithmetic conversions" on T1 and
   T2 as described in [expr].  */

tree
type_after_usual_arithmetic_conversions (tree t1, tree t2)
{
  gcc_assert (ARITHMETIC_TYPE_P (t1)
	      || VECTOR_TYPE_P (t1)
	      || UNSCOPED_ENUM_P (t1));
  gcc_assert (ARITHMETIC_TYPE_P (t2)
	      || VECTOR_TYPE_P (t2)
	      || UNSCOPED_ENUM_P (t2));

  /* Perform the integral promotions.  We do not promote real types here.  */
  if (INTEGRAL_OR_ENUMERATION_TYPE_P (t1)
      && INTEGRAL_OR_ENUMERATION_TYPE_P (t2))
    {
      t1 = type_promotes_to (t1);
      t2 = type_promotes_to (t2);
    }

  return cp_common_type (t1, t2);
}

static void
composite_pointer_error (const op_location_t &location,
			 enum diagnostics::kind kind, tree t1, tree t2,
			 composite_pointer_operation operation)
{
  switch (operation)
    {
    case CPO_COMPARISON:
      emit_diagnostic (kind, location, 0,
		       "comparison between "
		       "distinct pointer types %qT and %qT lacks a cast",
		       t1, t2);
      break;
    case CPO_CONVERSION:
      emit_diagnostic (kind, location, 0,
		       "conversion between "
		       "distinct pointer types %qT and %qT lacks a cast",
		       t1, t2);
      break;
    case CPO_CONDITIONAL_EXPR:
      emit_diagnostic (kind, location, 0,
		       "conditional expression between "
		       "distinct pointer types %qT and %qT lacks a cast",
		       t1, t2);
      break;
    default:
      gcc_unreachable ();
    }
}

/* Subroutine of composite_pointer_type to implement the recursive
   case.  See that function for documentation of the parameters.  And ADD_CONST
   is used to track adding "const" where needed.  */

static tree
composite_pointer_type_r (const op_location_t &location,
			  tree t1, tree t2, bool *add_const,
			  composite_pointer_operation operation,
			  tsubst_flags_t complain)
{
  tree pointee1;
  tree pointee2;
  tree result_type;
  tree attributes;

  /* Determine the types pointed to by T1 and T2.  */
  if (TYPE_PTR_P (t1))
    {
      pointee1 = TREE_TYPE (t1);
      pointee2 = TREE_TYPE (t2);
    }
  else
    {
      pointee1 = TYPE_PTRMEM_POINTED_TO_TYPE (t1);
      pointee2 = TYPE_PTRMEM_POINTED_TO_TYPE (t2);
    }

  /* [expr.type]

     If T1 and T2 are similar types, the result is the cv-combined type of
     T1 and T2.  */
  if (same_type_ignoring_top_level_qualifiers_p (pointee1, pointee2))
    result_type = pointee1;
  else if ((TYPE_PTR_P (pointee1) && TYPE_PTR_P (pointee2))
	   || (TYPE_PTRMEM_P (pointee1) && TYPE_PTRMEM_P (pointee2)))
    {
      result_type = composite_pointer_type_r (location, pointee1, pointee2,
					      add_const, operation, complain);
      if (result_type == error_mark_node)
	return error_mark_node;
    }
  else
    {
      if (complain & tf_error)
	composite_pointer_error (location, diagnostics::kind::permerror,
				 t1, t2, operation);
      else
	return error_mark_node;
      result_type = void_type_node;
    }
  const int q1 = cp_type_quals (pointee1);
  const int q2 = cp_type_quals (pointee2);
  const int quals = q1 | q2;
  result_type = cp_build_qualified_type (result_type,
					 (quals | (*add_const
						   ? TYPE_QUAL_CONST
						   : TYPE_UNQUALIFIED)));
  /* The cv-combined type can add "const" as per [conv.qual]/3.3 (except for
     the TLQ).  The reason is that both T1 and T2 can then be converted to the
     cv-combined type of T1 and T2.  */
  if (quals != q1 || quals != q2)
    *add_const = true;
  /* If the original types were pointers to members, so is the
     result.  */
  if (TYPE_PTRMEM_P (t1))
    {
      if (!same_type_p (TYPE_PTRMEM_CLASS_TYPE (t1),
			TYPE_PTRMEM_CLASS_TYPE (t2)))
	{
	  if (complain & tf_error)
	    composite_pointer_error (location, diagnostics::kind::permerror,
				     t1, t2, operation);
	  else
	    return error_mark_node;
	}
      result_type = build_ptrmem_type (TYPE_PTRMEM_CLASS_TYPE (t1),
				       result_type);
    }
  else
    result_type = build_pointer_type (result_type);

  /* Merge the attributes.  */
  attributes = (*targetm.merge_type_attributes) (t1, t2);
  return build_type_attribute_variant (result_type, attributes);
}

/* Return the composite pointer type (see [expr.type]) for T1 and T2.
   ARG1 and ARG2 are the values with those types.  The OPERATION is to
   describe the operation between the pointer types,
   in case an error occurs.

   This routine also implements the computation of a common type for
   pointers-to-members as per [expr.eq].  */

tree
composite_pointer_type (const op_location_t &location,
			tree t1, tree t2, tree arg1, tree arg2,
			composite_pointer_operation operation,
			tsubst_flags_t complain)
{
  tree class1;
  tree class2;

  /* [expr.type]

     If one operand is a null pointer constant, the composite pointer
     type is the type of the other operand.  */
  if (null_ptr_cst_p (arg1))
    return t2;
  if (null_ptr_cst_p (arg2))
    return t1;

  /* We have:

       [expr.type]

       If one of the operands has type "pointer to cv1 void", then
       the other has type "pointer to cv2 T", and the composite pointer
       type is "pointer to cv12 void", where cv12 is the union of cv1
       and cv2.

    If either type is a pointer to void, make sure it is T1.  */
  if (TYPE_PTR_P (t2) && VOID_TYPE_P (TREE_TYPE (t2)))
    std::swap (t1, t2);

  /* Now, if T1 is a pointer to void, merge the qualifiers.  */
  if (TYPE_PTR_P (t1) && VOID_TYPE_P (TREE_TYPE (t1)))
    {
      tree attributes;
      tree result_type;

      if (TYPE_PTRFN_P (t2))
	{
	  if (complain & tf_error)
	    {
	      switch (operation)
		{
		case CPO_COMPARISON:
		  pedwarn (location, OPT_Wpedantic,
			   "ISO C++ forbids comparison between pointer "
			   "of type %<void *%> and pointer-to-function");
		  break;
		case CPO_CONVERSION:
		  pedwarn (location, OPT_Wpedantic,
			   "ISO C++ forbids conversion between pointer "
			   "of type %<void *%> and pointer-to-function");
		  break;
		case CPO_CONDITIONAL_EXPR:
		  pedwarn (location, OPT_Wpedantic,
			   "ISO C++ forbids conditional expression between "
			   "pointer of type %<void *%> and "
			   "pointer-to-function");
		  break;
		default:
		  gcc_unreachable ();
		}
	    }
	  else
	    return error_mark_node;
        }
      result_type
	= cp_build_qualified_type (void_type_node,
				   (cp_type_quals (TREE_TYPE (t1))
				    | cp_type_quals (TREE_TYPE (t2))));
      result_type = build_pointer_type (result_type);
      /* Merge the attributes.  */
      attributes = (*targetm.merge_type_attributes) (t1, t2);
      return build_type_attribute_variant (result_type, attributes);
    }

  if (c_dialect_objc () && TYPE_PTR_P (t1)
      && TYPE_PTR_P (t2))
    {
      if (objc_have_common_type (t1, t2, -3, NULL_TREE))
	return objc_common_type (t1, t2);
    }

  /* if T1 or T2 is "pointer to noexcept function" and the other type is
     "pointer to function", where the function types are otherwise the same,
     "pointer to function" */
  if (fnptr_conv_p (t1, t2))
    return t1;
  if (fnptr_conv_p (t2, t1))
    return t2;

  /* [expr.eq] permits the application of a pointer conversion to
     bring the pointers to a common type.  */
  if (TYPE_PTR_P (t1) && TYPE_PTR_P (t2)
      && CLASS_TYPE_P (TREE_TYPE (t1))
      && CLASS_TYPE_P (TREE_TYPE (t2))
      && !same_type_ignoring_top_level_qualifiers_p (TREE_TYPE (t1),
						     TREE_TYPE (t2)))
    {
      class1 = TREE_TYPE (t1);
      class2 = TREE_TYPE (t2);

      if (DERIVED_FROM_P (class1, class2))
	t2 = (build_pointer_type
	      (cp_build_qualified_type (class1, cp_type_quals (class2))));
      else if (DERIVED_FROM_P (class2, class1))
	t1 = (build_pointer_type
	      (cp_build_qualified_type (class2, cp_type_quals (class1))));
      else
        {
          if (complain & tf_error)
	    composite_pointer_error (location, diagnostics::kind::error,
				     t1, t2, operation);
          return error_mark_node;
        }
    }
  /* [expr.eq] permits the application of a pointer-to-member
     conversion to change the class type of one of the types.  */
  else if (TYPE_PTRMEM_P (t1)
           && !same_type_p (TYPE_PTRMEM_CLASS_TYPE (t1),
			    TYPE_PTRMEM_CLASS_TYPE (t2)))
    {
      class1 = TYPE_PTRMEM_CLASS_TYPE (t1);
      class2 = TYPE_PTRMEM_CLASS_TYPE (t2);

      if (DERIVED_FROM_P (class1, class2))
	t1 = build_ptrmem_type (class2, TYPE_PTRMEM_POINTED_TO_TYPE (t1));
      else if (DERIVED_FROM_P (class2, class1))
	t2 = build_ptrmem_type (class1, TYPE_PTRMEM_POINTED_TO_TYPE (t2));
      else
        {
          if (complain & tf_error)
            switch (operation)
              {
              case CPO_COMPARISON:
                error_at (location, "comparison between distinct "
			  "pointer-to-member types %qT and %qT lacks a cast",
			  t1, t2);
                break;
              case CPO_CONVERSION:
                error_at (location, "conversion between distinct "
			  "pointer-to-member types %qT and %qT lacks a cast",
			  t1, t2);
                break;
              case CPO_CONDITIONAL_EXPR:
                error_at (location, "conditional expression between distinct "
			  "pointer-to-member types %qT and %qT lacks a cast",
			  t1, t2);
                break;
              default:
                gcc_unreachable ();
              }
          return error_mark_node;
        }
    }

  bool add_const = false;
  return composite_pointer_type_r (location, t1, t2, &add_const, operation,
				   complain);
}

/* Return the merged type of two types.
   We assume that comptypes has already been done and returned 1;
   if that isn't so, this may crash.

   This just combines attributes and default arguments; any other
   differences would cause the two types to compare unalike.  */

tree
merge_types (tree t1, tree t2)
{
  enum tree_code code1;
  enum tree_code code2;
  tree attributes;

  /* Save time if the two types are the same.  */
  if (t1 == t2)
    return t1;
  if (original_type (t1) == original_type (t2))
    return t1;

  /* If one type is nonsense, use the other.  */
  if (t1 == error_mark_node)
    return t2;
  if (t2 == error_mark_node)
    return t1;

  /* Handle merging an auto redeclaration with a previous deduced
     return type.  */
  if (is_auto (t1))
    return t2;

  /* Merge the attributes.  */
  attributes = (*targetm.merge_type_attributes) (t1, t2);

  if (TYPE_PTRMEMFUNC_P (t1))
    t1 = TYPE_PTRMEMFUNC_FN_TYPE (t1);
  if (TYPE_PTRMEMFUNC_P (t2))
    t2 = TYPE_PTRMEMFUNC_FN_TYPE (t2);

  code1 = TREE_CODE (t1);
  code2 = TREE_CODE (t2);
  if (code1 != code2)
    {
      gcc_assert (code1 == TYPENAME_TYPE || code2 == TYPENAME_TYPE);
      if (code1 == TYPENAME_TYPE)
	{
          t1 = resolve_typename_type (t1, /*only_current_p=*/true);
	  code1 = TREE_CODE (t1);
	}
      else
	{
          t2 = resolve_typename_type (t2, /*only_current_p=*/true);
	  code2 = TREE_CODE (t2);
	}
    }

  switch (code1)
    {
    case POINTER_TYPE:
    case REFERENCE_TYPE:
      /* For two pointers, do this recursively on the target type.  */
      {
	tree target = merge_types (TREE_TYPE (t1), TREE_TYPE (t2));
	int quals = cp_type_quals (t1);

	if (code1 == POINTER_TYPE)
	  {
	    t1 = build_pointer_type (target);
	    if (TREE_CODE (target) == METHOD_TYPE)
	      t1 = build_ptrmemfunc_type (t1);
	  }
	else
	  t1 = cp_build_reference_type (target, TYPE_REF_IS_RVALUE (t1));
	t1 = build_type_attribute_variant (t1, attributes);
	t1 = cp_build_qualified_type (t1, quals);

	return t1;
      }

    case OFFSET_TYPE:
      {
	int quals;
	tree pointee;
	quals = cp_type_quals (t1);
	pointee = merge_types (TYPE_PTRMEM_POINTED_TO_TYPE (t1),
			       TYPE_PTRMEM_POINTED_TO_TYPE (t2));
	t1 = build_ptrmem_type (TYPE_PTRMEM_CLASS_TYPE (t1),
				pointee);
	t1 = cp_build_qualified_type (t1, quals);
	break;
      }

    case ARRAY_TYPE:
      {
	tree elt = merge_types (TREE_TYPE (t1), TREE_TYPE (t2));
	/* Save space: see if the result is identical to one of the args.  */
	if (elt == TREE_TYPE (t1) && TYPE_DOMAIN (t1))
	  return build_type_attribute_variant (t1, attributes);
	if (elt == TREE_TYPE (t2) && TYPE_DOMAIN (t2))
	  return build_type_attribute_variant (t2, attributes);
	/* Merge the element types, and have a size if either arg has one.  */
	t1 = build_cplus_array_type
	  (elt, TYPE_DOMAIN (TYPE_DOMAIN (t1) ? t1 : t2));
	break;
      }

    case FUNCTION_TYPE:
      /* Function types: prefer the one that specified arg types.
	 If both do, merge the arg types.  Also merge the return types.  */
      {
	tree valtype = merge_types (TREE_TYPE (t1), TREE_TYPE (t2));
	tree p1 = TYPE_ARG_TYPES (t1);
	tree p2 = TYPE_ARG_TYPES (t2);
	tree parms;

	/* Save space: see if the result is identical to one of the args.  */
	if (valtype == TREE_TYPE (t1) && ! p2)
	  return cp_build_type_attribute_variant (t1, attributes);
	if (valtype == TREE_TYPE (t2) && ! p1)
	  return cp_build_type_attribute_variant (t2, attributes);

	/* Simple way if one arg fails to specify argument types.  */
	if (p1 == NULL_TREE || TREE_VALUE (p1) == void_type_node)
	  parms = p2;
	else if (p2 == NULL_TREE || TREE_VALUE (p2) == void_type_node)
	  parms = p1;
	else
	  parms = commonparms (p1, p2);

	cp_cv_quals quals = type_memfn_quals (t1);
	cp_ref_qualifier rqual = type_memfn_rqual (t1);
	gcc_assert (quals == type_memfn_quals (t2));
	gcc_assert (rqual == type_memfn_rqual (t2));

	tree rval = build_function_type (valtype, parms);
	rval = apply_memfn_quals (rval, quals);
	tree raises = merge_exception_specifiers (TYPE_RAISES_EXCEPTIONS (t1),
						  TYPE_RAISES_EXCEPTIONS (t2));
	bool late_return_type_p = TYPE_HAS_LATE_RETURN_TYPE (t1);
	t1 = build_cp_fntype_variant (rval, rqual, raises, late_return_type_p);
	break;
      }

    case METHOD_TYPE:
      {
	/* Get this value the long way, since TYPE_METHOD_BASETYPE
	   is just the main variant of this.  */
	tree basetype = class_of_this_parm (t2);
	tree raises = merge_exception_specifiers (TYPE_RAISES_EXCEPTIONS (t1),
						  TYPE_RAISES_EXCEPTIONS (t2));
	cp_ref_qualifier rqual = type_memfn_rqual (t1);
	tree t3;
	bool late_return_type_1_p = TYPE_HAS_LATE_RETURN_TYPE (t1);

	/* If this was a member function type, get back to the
	   original type of type member function (i.e., without
	   the class instance variable up front.  */
	t1 = build_function_type (TREE_TYPE (t1),
				  TREE_CHAIN (TYPE_ARG_TYPES (t1)));
	t2 = build_function_type (TREE_TYPE (t2),
				  TREE_CHAIN (TYPE_ARG_TYPES (t2)));
	t3 = merge_types (t1, t2);
	t3 = build_method_type_directly (basetype, TREE_TYPE (t3),
					 TYPE_ARG_TYPES (t3));
	t1 = build_cp_fntype_variant (t3, rqual, raises, late_return_type_1_p);
	break;
      }

    case TYPENAME_TYPE:
      /* There is no need to merge attributes into a TYPENAME_TYPE.
	 When the type is instantiated it will have whatever
	 attributes result from the instantiation.  */
      return t1;

    default:;
      if (attribute_list_equal (TYPE_ATTRIBUTES (t1), attributes))
	return t1;
      else if (attribute_list_equal (TYPE_ATTRIBUTES (t2), attributes))
	return t2;
      break;
    }

  return cp_build_type_attribute_variant (t1, attributes);
}

/* Return the ARRAY_TYPE type without its domain.  */

tree
strip_array_domain (tree type)
{
  tree t2;
  gcc_assert (TREE_CODE (type) == ARRAY_TYPE);
  if (TYPE_DOMAIN (type) == NULL_TREE)
    return type;
  t2 = build_cplus_array_type (TREE_TYPE (type), NULL_TREE);
  return cp_build_type_attribute_variant (t2, TYPE_ATTRIBUTES (type));
}

/* Wrapper around cp_common_type that is used by c-common.cc and other
   front end optimizations that remove promotions.

   Return the common type for two arithmetic types T1 and T2 under the
   usual arithmetic conversions.  The default conversions have already
   been applied, and enumerated types converted to their compatible
   integer types.  */

tree
common_type (tree t1, tree t2)
{
  /* If one type is nonsense, use the other  */
  if (t1 == error_mark_node)
    return t2;
  if (t2 == error_mark_node)
    return t1;

  return cp_common_type (t1, t2);
}

/* Return the common type of two pointer types T1 and T2.  This is the
   type for the result of most arithmetic operations if the operands
   have the given two types.

   We assume that comp_target_types has already been done and returned
   nonzero; if that isn't so, this may crash.  */

tree
common_pointer_type (tree t1, tree t2)
{
  gcc_assert ((TYPE_PTR_P (t1) && TYPE_PTR_P (t2))
              || (TYPE_PTRDATAMEM_P (t1) && TYPE_PTRDATAMEM_P (t2))
              || (TYPE_PTRMEMFUNC_P (t1) && TYPE_PTRMEMFUNC_P (t2)));

  return composite_pointer_type (input_location, t1, t2,
				 error_mark_node, error_mark_node,
                                 CPO_CONVERSION, tf_warning_or_error);
}

/* Compare two exception specifier types for exactness or subsetness, if
   allowed. Returns false for mismatch, true for match (same, or
   derived and !exact).

   [except.spec] "If a class X ... objects of class X or any class publicly
   and unambiguously derived from X. Similarly, if a pointer type Y * ...
   exceptions of type Y * or that are pointers to any type publicly and
   unambiguously derived from Y. Otherwise a function only allows exceptions
   that have the same type ..."
   This does not mention cv qualifiers and is different to what throw
   [except.throw] and catch [except.catch] will do. They will ignore the
   top level cv qualifiers, and allow qualifiers in the pointer to class
   example.

   We implement the letter of the standard.  */

static bool
comp_except_types (tree a, tree b, bool exact)
{
  if (same_type_p (a, b))
    return true;
  else if (!exact)
    {
      if (cp_type_quals (a) || cp_type_quals (b))
	return false;

      if (TYPE_PTR_P (a) && TYPE_PTR_P (b))
	{
	  a = TREE_TYPE (a);
	  b = TREE_TYPE (b);
	  if (cp_type_quals (a) || cp_type_quals (b))
	    return false;
	}

      if (TREE_CODE (a) != RECORD_TYPE
	  || TREE_CODE (b) != RECORD_TYPE)
	return false;

      if (publicly_uniquely_derived_p (a, b))
	return true;
    }
  return false;
}

/* Return true if TYPE1 and TYPE2 are equivalent exception specifiers.
   If EXACT is ce_derived, T2 can be stricter than T1 (according to 15.4/5).
   If EXACT is ce_type, the C++17 type compatibility rules apply.
   If EXACT is ce_normal, the compatibility rules in 15.4/3 apply.
   If EXACT is ce_exact, the specs must be exactly the same. Exception lists
   are unordered, but we've already filtered out duplicates. Most lists will
   be in order, we should try to make use of that.  */

bool
comp_except_specs (const_tree t1, const_tree t2, int exact)
{
  const_tree probe;
  const_tree base;
  int  length = 0;

  if (t1 == t2)
    return true;

  /* First handle noexcept.  */
  if (exact < ce_exact)
    {
      if (exact == ce_type
	  && (canonical_eh_spec (CONST_CAST_TREE (t1))
	      == canonical_eh_spec (CONST_CAST_TREE (t2))))
	return true;

      /* noexcept(false) is compatible with no exception-specification,
	 and less strict than any spec.  */
      if (t1 == noexcept_false_spec)
	return t2 == NULL_TREE || exact == ce_derived;
      /* Even a derived noexcept(false) is compatible with no
	 exception-specification.  */
      if (t2 == noexcept_false_spec)
	return t1 == NULL_TREE;

      /* Otherwise, if we aren't looking for an exact match, noexcept is
	 equivalent to throw().  */
      if (t1 == noexcept_true_spec)
	t1 = empty_except_spec;
      if (t2 == noexcept_true_spec)
	t2 = empty_except_spec;
    }

  /* If any noexcept is left, it is only comparable to itself;
     either we're looking for an exact match or we're redeclaring a
     template with dependent noexcept.  */
  if ((t1 && TREE_PURPOSE (t1))
      || (t2 && TREE_PURPOSE (t2)))
    return (t1 && t2
	    && (exact == ce_exact
		? TREE_PURPOSE (t1) == TREE_PURPOSE (t2)
		: cp_tree_equal (TREE_PURPOSE (t1), TREE_PURPOSE (t2))));

  if (t1 == NULL_TREE)			   /* T1 is ...  */
    return t2 == NULL_TREE || exact == ce_derived;
  if (!TREE_VALUE (t1))			   /* t1 is EMPTY */
    return t2 != NULL_TREE && !TREE_VALUE (t2);
  if (t2 == NULL_TREE)			   /* T2 is ...  */
    return false;
  if (TREE_VALUE (t1) && !TREE_VALUE (t2)) /* T2 is EMPTY, T1 is not */
    return exact == ce_derived;

  /* Neither set is ... or EMPTY, make sure each part of T2 is in T1.
     Count how many we find, to determine exactness. For exact matching and
     ordered T1, T2, this is an O(n) operation, otherwise its worst case is
     O(nm).  */
  for (base = t1; t2 != NULL_TREE; t2 = TREE_CHAIN (t2))
    {
      for (probe = base; probe != NULL_TREE; probe = TREE_CHAIN (probe))
	{
	  tree a = TREE_VALUE (probe);
	  tree b = TREE_VALUE (t2);

	  if (comp_except_types (a, b, exact))
	    {
	      if (probe == base && exact > ce_derived)
		base = TREE_CHAIN (probe);
	      length++;
	      break;
	    }
	}
      if (probe == NULL_TREE)
	return false;
    }
  return exact == ce_derived || base == NULL_TREE || length == list_length (t1);
}

/* Compare the array types T1 and T2.  CB says how we should behave when
   comparing array bounds: bounds_none doesn't allow dimensionless arrays,
   bounds_either says than any array can be [], bounds_first means that
   onlt T1 can be an array with unknown bounds.  STRICT is true if
   qualifiers must match when comparing the types of the array elements.  */

static bool
comp_array_types (const_tree t1, const_tree t2, compare_bounds_t cb,
		  bool strict)
{
  tree d1;
  tree d2;
  tree max1, max2;

  if (t1 == t2)
    return true;

  /* The type of the array elements must be the same.  */
  if (strict
      ? !same_type_p (TREE_TYPE (t1), TREE_TYPE (t2))
      : !similar_type_p (TREE_TYPE (t1), TREE_TYPE (t2)))
    return false;

  d1 = TYPE_DOMAIN (t1);
  d2 = TYPE_DOMAIN (t2);

  if (d1 == d2)
    return true;

  /* If one of the arrays is dimensionless, and the other has a
     dimension, they are of different types.  However, it is valid to
     write:

       extern int a[];
       int a[3];

     by [basic.link]:

       declarations for an array object can specify
       array types that differ by the presence or absence of a major
       array bound (_dcl.array_).  */
  if (!d1 && d2)
    return cb >= bounds_either;
  else if (d1 && !d2)
    return cb == bounds_either;

  /* Check that the dimensions are the same.  */

  if (!cp_tree_equal (TYPE_MIN_VALUE (d1), TYPE_MIN_VALUE (d2)))
    return false;
  max1 = TYPE_MAX_VALUE (d1);
  max2 = TYPE_MAX_VALUE (d2);

  if (!cp_tree_equal (max1, max2))
    return false;

  return true;
}

/* Compare the relative position of T1 and T2 into their respective
   template parameter list.
   T1 and T2 must be template parameter types.
   Return TRUE if T1 and T2 have the same position, FALSE otherwise.  */

static bool
comp_template_parms_position (tree t1, tree t2)
{
  tree index1, index2;
  gcc_assert (t1 && t2
	      && TREE_CODE (t1) == TREE_CODE (t2)
	      && (TREE_CODE (t1) == BOUND_TEMPLATE_TEMPLATE_PARM
		  || TREE_CODE (t1) == TEMPLATE_TEMPLATE_PARM
		  || TREE_CODE (t1) == TEMPLATE_TYPE_PARM));

  index1 = TEMPLATE_TYPE_PARM_INDEX (TYPE_MAIN_VARIANT (t1));
  index2 = TEMPLATE_TYPE_PARM_INDEX (TYPE_MAIN_VARIANT (t2));

  /* Then compare their relative position.  */
  if (TEMPLATE_PARM_IDX (index1) != TEMPLATE_PARM_IDX (index2)
      || TEMPLATE_PARM_LEVEL (index1) != TEMPLATE_PARM_LEVEL (index2)
      || (TEMPLATE_PARM_PARAMETER_PACK (index1)
	  != TEMPLATE_PARM_PARAMETER_PACK (index2)))
    return false;

  /* In C++14 we can end up comparing 'auto' to a normal template
     parameter.  Don't confuse them.  */
  if (cxx_dialect >= cxx14 && (is_auto (t1) || is_auto (t2)))
    return TYPE_IDENTIFIER (t1) == TYPE_IDENTIFIER (t2);

  return true;
}

/* Heuristic check if two parameter types can be considered ABI-equivalent.  */

static bool
cxx_safe_arg_type_equiv_p (tree t1, tree t2)
{
  t1 = TYPE_MAIN_VARIANT (t1);
  t2 = TYPE_MAIN_VARIANT (t2);

  if (TYPE_PTR_P (t1)
      && TYPE_PTR_P (t2))
    return true;

  /* Only the precision of the parameter matters.  This check should
     make sure that the callee does not see undefined values in argument
     registers.  */
  if (INTEGRAL_TYPE_P (t1)
      && INTEGRAL_TYPE_P (t2)
      && TYPE_PRECISION (t1) == TYPE_PRECISION (t2))
    return true;

  return same_type_p (t1, t2);
}

/* Check if a type cast between two function types can be considered safe.  */

static bool
cxx_safe_function_type_cast_p (tree t1, tree t2)
{
  if (TREE_TYPE (t1) == void_type_node &&
      TYPE_ARG_TYPES (t1) == void_list_node)
    return true;

  if (TREE_TYPE (t2) == void_type_node &&
      TYPE_ARG_TYPES (t2) == void_list_node)
    return true;

  if (!cxx_safe_arg_type_equiv_p (TREE_TYPE (t1), TREE_TYPE (t2)))
    return false;

  for (t1 = TYPE_ARG_TYPES (t1), t2 = TYPE_ARG_TYPES (t2);
       t1 && t2;
       t1 = TREE_CHAIN (t1), t2 = TREE_CHAIN (t2))
    if (!cxx_safe_arg_type_equiv_p (TREE_VALUE (t1), TREE_VALUE (t2)))
      return false;

  return true;
}

/* Subroutine in comptypes.  */

static bool
structural_comptypes (tree t1, tree t2, int strict)
{
  /* Both should be types that are not obviously the same.  */
  gcc_checking_assert (t1 != t2 && TYPE_P (t1) && TYPE_P (t2));

  /* Suppress typename resolution under spec_hasher::equal in place of calling
     push_to_top_level there.  */
  if (!comparing_specializations)
    {
      /* TYPENAME_TYPEs should be resolved if the qualifying scope is the
	 current instantiation.  */
      if (TREE_CODE (t1) == TYPENAME_TYPE)
	t1 = resolve_typename_type (t1, /*only_current_p=*/true);

      if (TREE_CODE (t2) == TYPENAME_TYPE)
	t2 = resolve_typename_type (t2, /*only_current_p=*/true);
    }

  if (TYPE_PTRMEMFUNC_P (t1))
    t1 = TYPE_PTRMEMFUNC_FN_TYPE (t1);
  if (TYPE_PTRMEMFUNC_P (t2))
    t2 = TYPE_PTRMEMFUNC_FN_TYPE (t2);

  /* Different classes of types can't be compatible.  */
  if (TREE_CODE (t1) != TREE_CODE (t2))
    return false;

  /* Qualifiers must match.  For array types, we will check when we
     recur on the array element types.  */
  if (TREE_CODE (t1) != ARRAY_TYPE
      && cp_type_quals (t1) != cp_type_quals (t2))
    return false;
  if (TREE_CODE (t1) == FUNCTION_TYPE
      && type_memfn_quals (t1) != type_memfn_quals (t2))
    return false;
  /* Need to check this before TYPE_MAIN_VARIANT.
     FIXME function qualifiers should really change the main variant.  */
  if (FUNC_OR_METHOD_TYPE_P (t1))
    {
      if (type_memfn_rqual (t1) != type_memfn_rqual (t2))
	return false;
      if (flag_noexcept_type
	  && !comp_except_specs (TYPE_RAISES_EXCEPTIONS (t1),
				 TYPE_RAISES_EXCEPTIONS (t2),
				 ce_type))
	return false;
    }

  /* Allow for two different type nodes which have essentially the same
     definition.  Note that we already checked for equality of the type
     qualifiers (just above).  */
  if (TREE_CODE (t1) != ARRAY_TYPE
      && TYPE_MAIN_VARIANT (t1) == TYPE_MAIN_VARIANT (t2))
    goto check_alias;

  /* Compare the types.  Return false on known not-same. Break on not
     known.   Never return true from this switch -- you'll break
     specialization comparison.    */
  switch (TREE_CODE (t1))
    {
    case VOID_TYPE:
    case BOOLEAN_TYPE:
      /* All void and bool types are the same.  */
      break;

    case OPAQUE_TYPE:
    case INTEGER_TYPE:
    case FIXED_POINT_TYPE:
    case REAL_TYPE:
      /* With these nodes, we can't determine type equivalence by
	 looking at what is stored in the nodes themselves, because
	 two nodes might have different TYPE_MAIN_VARIANTs but still
	 represent the same type.  For example, wchar_t and int could
	 have the same properties (TYPE_PRECISION, TYPE_MIN_VALUE,
	 TYPE_MAX_VALUE, etc.), but have different TYPE_MAIN_VARIANTs
	 and are distinct types. On the other hand, int and the
	 following typedef

           typedef int INT __attribute((may_alias));

	 have identical properties, different TYPE_MAIN_VARIANTs, but
	 represent the same type.  The canonical type system keeps
	 track of equivalence in this case, so we fall back on it.  */
      if (TYPE_CANONICAL (t1) != TYPE_CANONICAL (t2))
	return false;

      /* We don't need or want the attribute comparison.  */
      goto check_alias;

    case TEMPLATE_TEMPLATE_PARM:
    case BOUND_TEMPLATE_TEMPLATE_PARM:
      if (!comp_template_parms_position (t1, t2))
	return false;
      if (!comp_template_parms
	  (DECL_TEMPLATE_PARMS (TEMPLATE_TEMPLATE_PARM_TEMPLATE_DECL (t1)),
	   DECL_TEMPLATE_PARMS (TEMPLATE_TEMPLATE_PARM_TEMPLATE_DECL (t2))))
	return false;
      if (TREE_CODE (t1) == TEMPLATE_TEMPLATE_PARM)
	break;
      /* Don't check inheritance.  */
      strict = COMPARE_STRICT;
      /* Fall through.  */

    case RECORD_TYPE:
    case UNION_TYPE:
      if (TYPE_TEMPLATE_INFO (t1) && TYPE_TEMPLATE_INFO (t2)
	  && (TYPE_TI_TEMPLATE (t1) == TYPE_TI_TEMPLATE (t2)
	      || TREE_CODE (t1) == BOUND_TEMPLATE_TEMPLATE_PARM)
	  && comp_template_args (TYPE_TI_ARGS (t1), TYPE_TI_ARGS (t2)))
	break;

      if ((strict & COMPARE_BASE) && DERIVED_FROM_P (t1, t2))
	break;
      else if ((strict & COMPARE_DERIVED) && DERIVED_FROM_P (t2, t1))
	break;

      return false;

    case OFFSET_TYPE:
      if (!comptypes (TYPE_OFFSET_BASETYPE (t1), TYPE_OFFSET_BASETYPE (t2),
		      strict & ~COMPARE_REDECLARATION))
	return false;
      if (!same_type_p (TREE_TYPE (t1), TREE_TYPE (t2)))
	return false;
      break;

    case REFERENCE_TYPE:
      if (TYPE_REF_IS_RVALUE (t1) != TYPE_REF_IS_RVALUE (t2))
	return false;
      /* fall through to checks for pointer types */
      gcc_fallthrough ();

    case POINTER_TYPE:
      if (TYPE_MODE (t1) != TYPE_MODE (t2)
	  || !same_type_p (TREE_TYPE (t1), TREE_TYPE (t2)))
	return false;
      break;

    case METHOD_TYPE:
    case FUNCTION_TYPE:
      /* Exception specs and memfn_rquals were checked above.  */
      if (!same_type_p (TREE_TYPE (t1), TREE_TYPE (t2)))
	return false;
      if (!compparms (TYPE_ARG_TYPES (t1), TYPE_ARG_TYPES (t2)))
	return false;
      break;

    case ARRAY_TYPE:
      /* Target types must match incl. qualifiers.  */
      if (!comp_array_types (t1, t2, ((strict & COMPARE_REDECLARATION)
				      ? bounds_either : bounds_none),
			     /*strict=*/true))
	return false;
      break;

    case TEMPLATE_TYPE_PARM:
      /* If T1 and T2 don't have the same relative position in their
	 template parameters set, they can't be equal.  */
      if (!comp_template_parms_position (t1, t2))
	return false;
      /* If T1 and T2 don't represent the same class template deduction,
         they aren't equal.  */
      if (!cp_tree_equal (CLASS_PLACEHOLDER_TEMPLATE (t1),
			  CLASS_PLACEHOLDER_TEMPLATE (t2)))
	return false;
      /* Constrained 'auto's are distinct from parms that don't have the same
	 constraints.  */
      if (!equivalent_placeholder_constraints (t1, t2))
	return false;
      break;

    case TYPENAME_TYPE:
      if (!cp_tree_equal (TYPENAME_TYPE_FULLNAME (t1),
			  TYPENAME_TYPE_FULLNAME (t2)))
	return false;
      /* Qualifiers don't matter on scopes.  */
      if (!same_type_ignoring_top_level_qualifiers_p (TYPE_CONTEXT (t1),
						      TYPE_CONTEXT (t2)))
	return false;
      break;

    case UNBOUND_CLASS_TEMPLATE:
      if (!cp_tree_equal (TYPE_IDENTIFIER (t1), TYPE_IDENTIFIER (t2)))
	return false;
      if (!same_type_p (TYPE_CONTEXT (t1), TYPE_CONTEXT (t2)))
	return false;
      break;

    case COMPLEX_TYPE:
      if (!same_type_p (TREE_TYPE (t1), TREE_TYPE (t2)))
	return false;
      break;

    case VECTOR_TYPE:
      if (gnu_vector_type_p (t1) != gnu_vector_type_p (t2)
	  || maybe_ne (TYPE_VECTOR_SUBPARTS (t1), TYPE_VECTOR_SUBPARTS (t2))
	  || !same_type_p (TREE_TYPE (t1), TREE_TYPE (t2)))
	return false;
      break;

    case TYPE_PACK_EXPANSION:
      return (same_type_p (PACK_EXPANSION_PATTERN (t1),
			   PACK_EXPANSION_PATTERN (t2))
	      && comp_template_args (PACK_EXPANSION_EXTRA_ARGS (t1),
				     PACK_EXPANSION_EXTRA_ARGS (t2)));

    case PACK_INDEX_TYPE:
      return (cp_tree_equal (PACK_INDEX_PACK (t1),
			     PACK_INDEX_PACK (t2))
	      && cp_tree_equal (PACK_INDEX_INDEX (t1),
				PACK_INDEX_INDEX (t2)));

    case DECLTYPE_TYPE:
      if (DECLTYPE_TYPE_ID_EXPR_OR_MEMBER_ACCESS_P (t1)
          != DECLTYPE_TYPE_ID_EXPR_OR_MEMBER_ACCESS_P (t2))
	return false;
      if (DECLTYPE_FOR_LAMBDA_CAPTURE (t1) != DECLTYPE_FOR_LAMBDA_CAPTURE (t2))
	return false;
      if (DECLTYPE_FOR_LAMBDA_PROXY (t1) != DECLTYPE_FOR_LAMBDA_PROXY (t2))
	return false;
      if (!cp_tree_equal (DECLTYPE_TYPE_EXPR (t1), DECLTYPE_TYPE_EXPR (t2)))
        return false;
      break;

    case TRAIT_TYPE:
      if (TRAIT_TYPE_KIND (t1) != TRAIT_TYPE_KIND (t2))
	return false;
      if (!cp_tree_equal (TRAIT_TYPE_TYPE1 (t1), TRAIT_TYPE_TYPE1 (t2))
	  || !cp_tree_equal (TRAIT_TYPE_TYPE2 (t1), TRAIT_TYPE_TYPE2 (t2)))
	return false;
      break;

    case TYPEOF_TYPE:
      if (!cp_tree_equal (TYPEOF_TYPE_EXPR (t1), TYPEOF_TYPE_EXPR (t2)))
	return false;
      break;

    default:
      return false;
    }

  /* If we get here, we know that from a target independent POV the
     types are the same.  Make sure the target attributes are also
     the same.  */
  if (!comp_type_attributes (t1, t2))
    return false;

 check_alias:
  if (comparing_dependent_aliases
      && (typedef_variant_p (t1) || typedef_variant_p (t2)))
    {
      tree dep1 = dependent_opaque_alias_p (t1) ? t1 : NULL_TREE;
      tree dep2 = dependent_opaque_alias_p (t2) ? t2 : NULL_TREE;
      if ((dep1 || dep2)
	  && (!(dep1 && dep2)
	      || !comp_type_attributes (DECL_ATTRIBUTES (TYPE_NAME (dep1)),
					DECL_ATTRIBUTES (TYPE_NAME (dep2)))))
	return false;

      /* Don't treat an alias template specialization with dependent
	 arguments as equivalent to its underlying type when used as a
	 template argument; we need them to be distinct so that we
	 substitute into the specialization arguments at instantiation
	 time.  And aliases can't be equivalent without being ==, so
	 we don't need to look any deeper.  */
      ++processing_template_decl;
      dep1 = dependent_alias_template_spec_p (t1, nt_transparent);
      dep2 = dependent_alias_template_spec_p (t2, nt_transparent);
      --processing_template_decl;
      if ((dep1 || dep2) && dep1 != dep2)
	return false;
    }

  return true;
}

/* C++ implementation of compatible_types_for_indirection_note_p.  */

bool
compatible_types_for_indirection_note_p (tree type1, tree type2)
{
  return same_type_p (type1, type2);
}

/* Return true if T1 and T2 are related as allowed by STRICT.  STRICT
   is a bitwise-or of the COMPARE_* flags.  */

bool
comptypes (tree t1, tree t2, int strict)
{
  gcc_checking_assert (t1 && t2);

  /* TYPE_ARGUMENT_PACKS are not really types.  */
  gcc_checking_assert (TREE_CODE (t1) != TYPE_ARGUMENT_PACK
		       && TREE_CODE (t2) != TYPE_ARGUMENT_PACK);

  if (t1 == t2)
    return true;

  /* Suppress errors caused by previously reported errors.  */
  if (t1 == error_mark_node || t2 == error_mark_node)
    return false;

  if (strict == COMPARE_STRICT)
    {
      if (TYPE_STRUCTURAL_EQUALITY_P (t1) || TYPE_STRUCTURAL_EQUALITY_P (t2))
	/* At least one of the types requires structural equality, so
	   perform a deep check. */
	return structural_comptypes (t1, t2, strict);

      if (flag_checking && param_use_canonical_types)
	{
	  bool result = structural_comptypes (t1, t2, strict);

	  if (result && TYPE_CANONICAL (t1) != TYPE_CANONICAL (t2))
	    /* The two types are structurally equivalent, but their
	       canonical types were different. This is a failure of the
	       canonical type propagation code.*/
	    internal_error
	      ("canonical types differ for identical types %qT and %qT",
	       t1, t2);
	  else if (!result && TYPE_CANONICAL (t1) == TYPE_CANONICAL (t2))
	    /* Two types are structurally different, but the canonical
	       types are the same. This means we were over-eager in
	       assigning canonical types. */
	    internal_error
	      ("same canonical type node for different types %qT and %qT",
	       t1, t2);

	  return result;
	}
      if (!flag_checking && param_use_canonical_types)
	return TYPE_CANONICAL (t1) == TYPE_CANONICAL (t2);
      else
	return structural_comptypes (t1, t2, strict);
    }
  else if (strict == COMPARE_STRUCTURAL)
    return structural_comptypes (t1, t2, COMPARE_STRICT);
  else
    return structural_comptypes (t1, t2, strict);
}

/* Returns nonzero iff TYPE1 and TYPE2 are the same type, ignoring
   top-level qualifiers.  */

bool
same_type_ignoring_top_level_qualifiers_p (tree type1, tree type2)
{
  if (type1 == error_mark_node || type2 == error_mark_node)
    return false;
  if (type1 == type2)
    return true;

  type1 = cp_build_qualified_type (type1, TYPE_UNQUALIFIED);
  type2 = cp_build_qualified_type (type2, TYPE_UNQUALIFIED);
  return same_type_p (type1, type2);
}

/* Returns nonzero iff TYPE1 and TYPE2 are similar, as per [conv.qual].  */

bool
similar_type_p (tree type1, tree type2)
{
  if (type1 == error_mark_node || type2 == error_mark_node)
    return false;

  /* Informally, two types are similar if, ignoring top-level cv-qualification:
     * they are the same type; or
     * they are both pointers, and the pointed-to types are similar; or
     * they are both pointers to member of the same class, and the types of
       the pointed-to members are similar; or
     * they are both arrays of the same size or both arrays of unknown bound,
       and the array element types are similar.  */

  if (same_type_ignoring_top_level_qualifiers_p (type1, type2))
    return true;

  if ((TYPE_PTR_P (type1) && TYPE_PTR_P (type2))
      || (TYPE_PTRDATAMEM_P (type1) && TYPE_PTRDATAMEM_P (type2))
      || (TREE_CODE (type1) == ARRAY_TYPE && TREE_CODE (type2) == ARRAY_TYPE))
    return comp_ptr_ttypes_const (type1, type2, bounds_either);

  return false;
}

/* Helper function for layout_compatible_type_p and
   is_corresponding_member_aggr.  Advance to next members (NULL if
   no further ones) and return true if those members are still part of
   the common initial sequence.  */

bool
next_common_initial_sequence (tree &memb1, tree &memb2)
{
  while (memb1)
    {
      if (TREE_CODE (memb1) != FIELD_DECL
	  || (DECL_FIELD_IS_BASE (memb1) && is_empty_field (memb1)))
	{
	  memb1 = DECL_CHAIN (memb1);
	  continue;
	}
      if (DECL_FIELD_IS_BASE (memb1))
	{
	  memb1 = TYPE_FIELDS (TREE_TYPE (memb1));
	  continue;
	}
      break;
    }
  while (memb2)
    {
      if (TREE_CODE (memb2) != FIELD_DECL
	  || (DECL_FIELD_IS_BASE (memb2) && is_empty_field (memb2)))
	{
	  memb2 = DECL_CHAIN (memb2);
	  continue;
	}
      if (DECL_FIELD_IS_BASE (memb2))
	{
	  memb2 = TYPE_FIELDS (TREE_TYPE (memb2));
	  continue;
	}
      break;
    }
  if (memb1 == NULL_TREE && memb2 == NULL_TREE)
    return true;
  if (memb1 == NULL_TREE || memb2 == NULL_TREE)
    return false;
  if (DECL_BIT_FIELD_TYPE (memb1))
    {
      if (!DECL_BIT_FIELD_TYPE (memb2))
	return false;
      if (!layout_compatible_type_p (DECL_BIT_FIELD_TYPE (memb1),
				     DECL_BIT_FIELD_TYPE (memb2)))
	return false;
      if (TYPE_PRECISION (TREE_TYPE (memb1))
	  != TYPE_PRECISION (TREE_TYPE (memb2)))
	return false;
    }
  else if (DECL_BIT_FIELD_TYPE (memb2))
    return false;
  else if (!layout_compatible_type_p (TREE_TYPE (memb1), TREE_TYPE (memb2)))
    return false;
  if ((!lookup_attribute ("no_unique_address", DECL_ATTRIBUTES (memb1)))
      != !lookup_attribute ("no_unique_address", DECL_ATTRIBUTES (memb2)))
    return false;
  if (DECL_ALIGN (memb1) != DECL_ALIGN (memb2))
    return false;
  if (!tree_int_cst_equal (bit_position (memb1), bit_position (memb2)))
    return false;
  return true;
}

/* Return true if TYPE1 and TYPE2 are layout-compatible types.  */

bool
layout_compatible_type_p (tree type1, tree type2)
{
  if (type1 == error_mark_node || type2 == error_mark_node)
    return false;
  if (type1 == type2)
    return true;
  if (TREE_CODE (type1) != TREE_CODE (type2))
    return false;

  type1 = cp_build_qualified_type (type1, TYPE_UNQUALIFIED);
  type2 = cp_build_qualified_type (type2, TYPE_UNQUALIFIED);

  if (TREE_CODE (type1) == ENUMERAL_TYPE)
    return (tree_int_cst_equal (TYPE_SIZE (type1), TYPE_SIZE (type2))
	    && same_type_p (finish_underlying_type (type1),
			    finish_underlying_type (type2)));

  if (CLASS_TYPE_P (type1)
      && std_layout_type_p (type1)
      && std_layout_type_p (type2)
      && tree_int_cst_equal (TYPE_SIZE (type1), TYPE_SIZE (type2)))
    {
      tree field1 = TYPE_FIELDS (type1);
      tree field2 = TYPE_FIELDS (type2);
      if (TREE_CODE (type1) == RECORD_TYPE)
	{
	  while (1)
	    {
	      if (!next_common_initial_sequence (field1, field2))
		return false;
	      if (field1 == NULL_TREE)
		return true;
	      field1 = DECL_CHAIN (field1);
	      field2 = DECL_CHAIN (field2);
	    }
	}
      /* Otherwise both types must be union types.
	 The standard says:
	 "Two standard-layout unions are layout-compatible if they have
	 the same number of non-static data members and corresponding
	 non-static data members (in any order) have layout-compatible
	 types."
	 but the code anticipates that bitfield vs. non-bitfield,
	 different bitfield widths or presence/absence of
	 [[no_unique_address]] should be checked as well.  */
      auto_vec<tree, 16> vec;
      unsigned int count = 0;
      for (; field1; field1 = DECL_CHAIN (field1))
	if (TREE_CODE (field1) == FIELD_DECL)
	  count++;
      for (; field2; field2 = DECL_CHAIN (field2))
	if (TREE_CODE (field2) == FIELD_DECL)
	  vec.safe_push (field2);
      /* Discussions on core lean towards treating multiple union fields
	 of the same type as the same field, so this might need changing
	 in the future.  */
      if (count != vec.length ())
	return false;
      for (field1 = TYPE_FIELDS (type1); field1; field1 = DECL_CHAIN (field1))
	{
	  if (TREE_CODE (field1) != FIELD_DECL)
	    continue;
	  unsigned int j;
	  tree t1 = DECL_BIT_FIELD_TYPE (field1);
	  if (t1 == NULL_TREE)
	    t1 = TREE_TYPE (field1);
	  FOR_EACH_VEC_ELT (vec, j, field2)
	    {
	      tree t2 = DECL_BIT_FIELD_TYPE (field2);
	      if (t2 == NULL_TREE)
		t2 = TREE_TYPE (field2);
	      if (DECL_BIT_FIELD_TYPE (field1))
		{
		  if (!DECL_BIT_FIELD_TYPE (field2))
		    continue;
		  if (TYPE_PRECISION (TREE_TYPE (field1))
		      != TYPE_PRECISION (TREE_TYPE (field2)))
		    continue;
		}
	      else if (DECL_BIT_FIELD_TYPE (field2))
		continue;
	      if (!layout_compatible_type_p (t1, t2))
		continue;
	      if ((!lookup_attribute ("no_unique_address",
				      DECL_ATTRIBUTES (field1)))
		  != !lookup_attribute ("no_unique_address",
					DECL_ATTRIBUTES (field2)))
		continue;
	      break;
	    }
	  if (j == vec.length ())
	    return false;
	  vec.unordered_remove (j);
	}
      return true;
    }

  return same_type_p (type1, type2);
}

/* Returns 1 if TYPE1 is at least as qualified as TYPE2.  */

bool
at_least_as_qualified_p (const_tree type1, const_tree type2)
{
  int q1 = cp_type_quals (type1);
  int q2 = cp_type_quals (type2);

  /* All qualifiers for TYPE2 must also appear in TYPE1.  */
  return (q1 & q2) == q2;
}

/* Returns 1 if TYPE1 is more cv-qualified than TYPE2, -1 if TYPE2 is
   more cv-qualified that TYPE1, and 0 otherwise.  */

int
comp_cv_qualification (int q1, int q2)
{
  if (q1 == q2)
    return 0;

  if ((q1 & q2) == q2)
    return 1;
  else if ((q1 & q2) == q1)
    return -1;

  return 0;
}

int
comp_cv_qualification (const_tree type1, const_tree type2)
{
  int q1 = cp_type_quals (type1);
  int q2 = cp_type_quals (type2);
  return comp_cv_qualification (q1, q2);
}

/* Returns 1 if the cv-qualification signature of TYPE1 is a proper
   subset of the cv-qualification signature of TYPE2, and the types
   are similar.  Returns -1 if the other way 'round, and 0 otherwise.  */

int
comp_cv_qual_signature (tree type1, tree type2)
{
  if (comp_ptr_ttypes_real (type2, type1, -1))
    return 1;
  else if (comp_ptr_ttypes_real (type1, type2, -1))
    return -1;
  else
    return 0;
}

/* Subroutines of `comptypes'.  */

/* Return true if two parameter type lists PARMS1 and PARMS2 are
   equivalent in the sense that functions with those parameter types
   can have equivalent types.  The two lists must be equivalent,
   element by element.  */

bool
compparms (const_tree parms1, const_tree parms2)
{
  const_tree t1, t2;

  /* An unspecified parmlist matches any specified parmlist
     whose argument types don't need default promotions.  */

  for (t1 = parms1, t2 = parms2;
       t1 || t2;
       t1 = TREE_CHAIN (t1), t2 = TREE_CHAIN (t2))
    {
      /* If one parmlist is shorter than the other,
	 they fail to match.  */
      if (!t1 || !t2)
	return false;
      if (!same_type_p (TREE_VALUE (t1), TREE_VALUE (t2)))
	return false;
    }
  return true;
}


/* Process a sizeof or alignof expression where the operand is a type.
   STD_ALIGNOF indicates whether an alignof has C++11 (minimum alignment)
   or GNU (preferred alignment) semantics; it is ignored if OP is
   SIZEOF_EXPR.  */

tree
cxx_sizeof_or_alignof_type (location_t loc, tree type, enum tree_code op,
			    bool std_alignof, bool complain)
{
  gcc_assert (op == SIZEOF_EXPR || op == ALIGNOF_EXPR);
  if (type == error_mark_node)
    return error_mark_node;

  type = non_reference (type);
  if (TREE_CODE (type) == METHOD_TYPE)
    {
      if (complain)
	{
	  pedwarn (loc, OPT_Wpointer_arith,
		   "invalid application of %qs to a member function",
		   OVL_OP_INFO (false, op)->name);
	  return size_one_node;
	}
      else
	return error_mark_node;
    }
  else if (VOID_TYPE_P (type) && std_alignof)
    {
      if (complain)
	error_at (loc, "invalid application of %qs to a void type",
		  OVL_OP_INFO (false, op)->name);
      return error_mark_node;
    }

  bool dependent_p = dependent_type_p (type);
  if (!dependent_p)
    {
      complete_type (type);
      if (!COMPLETE_TYPE_P (type))
	/* Call this here because the incompleteness diagnostic comes from
	   c_sizeof_or_alignof_type instead of
	   complete_type_or_maybe_complain.  */
	note_failed_type_completion (type, complain);
    }
  if (dependent_p
      /* VLA types will have a non-constant size.  In the body of an
	 uninstantiated template, we don't need to try to compute the
	 value, because the sizeof expression is not an integral
	 constant expression in that case.  And, if we do try to
	 compute the value, we'll likely end up with SAVE_EXPRs, which
	 the template substitution machinery does not expect to see.  */
      || (processing_template_decl
	  && COMPLETE_TYPE_P (type)
	  && TREE_CODE (TYPE_SIZE (type)) != INTEGER_CST))
    {
      tree value = build_min (op, size_type_node, type);
      TREE_READONLY (value) = 1;
      if (op == ALIGNOF_EXPR && std_alignof)
	ALIGNOF_EXPR_STD_P (value) = true;
      SET_EXPR_LOCATION (value, loc);
      return value;
    }

  return c_sizeof_or_alignof_type (loc, complete_type (type),
				   op == SIZEOF_EXPR, std_alignof,
				   complain & (tf_warning_or_error));
}

/* Return the size of the type, without producing any warnings for
   types whose size cannot be taken.  This routine should be used only
   in some other routine that has already produced a diagnostic about
   using the size of such a type.  */
tree
cxx_sizeof_nowarn (tree type)
{
  if (TREE_CODE (type) == FUNCTION_TYPE
      || VOID_TYPE_P (type)
      || TREE_CODE (type) == ERROR_MARK)
    return size_one_node;
  else if (!COMPLETE_TYPE_P (type))
    return size_zero_node;
  else
    return cxx_sizeof_or_alignof_type (input_location, type,
				       SIZEOF_EXPR, false, false);
}

/* Process a sizeof expression where the operand is an expression.  */

static tree
cxx_sizeof_expr (location_t loc, tree e, tsubst_flags_t complain)
{
  if (e == error_mark_node)
    return error_mark_node;

  if (instantiation_dependent_uneval_expression_p (e))
    {
      e = build_min (SIZEOF_EXPR, size_type_node, e);
      TREE_SIDE_EFFECTS (e) = 0;
      TREE_READONLY (e) = 1;
      SET_EXPR_LOCATION (e, loc);

      return e;
    }

  location_t e_loc = cp_expr_loc_or_loc (e, loc);
  STRIP_ANY_LOCATION_WRAPPER (e);

  if (TREE_CODE (e) == PARM_DECL
      && DECL_ARRAY_PARAMETER_P (e)
      && (complain & tf_warning))
    {
      auto_diagnostic_group d;
      if (warning_at (e_loc, OPT_Wsizeof_array_argument,
		      "%<sizeof%> on array function parameter %qE "
		      "will return size of %qT", e, TREE_TYPE (e)))
	inform (DECL_SOURCE_LOCATION (e), "declared here");
    }

  e = mark_type_use (e);

  if (bitfield_p (e))
    {
      if (complain & tf_error)
	error_at (e_loc,
		  "invalid application of %<sizeof%> to a bit-field");
      else
        return error_mark_node;
      e = char_type_node;
    }
  else if (is_overloaded_fn (e))
    {
      if (complain & tf_error)
	permerror (e_loc, "ISO C++ forbids applying %<sizeof%> to "
		   "an expression of function type");
      else
        return error_mark_node;
      e = char_type_node;
    }
  else if (type_unknown_p (e))
    {
      if (complain & tf_error)
        cxx_incomplete_type_error (e_loc, e, TREE_TYPE (e));
      else
        return error_mark_node;
      e = char_type_node;
    }
  else
    e = TREE_TYPE (e);

  return cxx_sizeof_or_alignof_type (loc, e, SIZEOF_EXPR, false,
				     complain & tf_error);
}

/* Implement the __alignof keyword: Return the minimum required
   alignment of E, measured in bytes.  For VAR_DECL's and
   FIELD_DECL's return DECL_ALIGN (which can be set from an
   "aligned" __attribute__ specification).  STD_ALIGNOF acts
   like in cxx_sizeof_or_alignof_type.  */

static tree
cxx_alignof_expr (location_t loc, tree e, bool std_alignof,
		  tsubst_flags_t complain)
{
  tree t;

  if (e == error_mark_node)
    return error_mark_node;

  if (processing_template_decl)
    {
      e = build_min (ALIGNOF_EXPR, size_type_node, e);
      TREE_SIDE_EFFECTS (e) = 0;
      TREE_READONLY (e) = 1;
      SET_EXPR_LOCATION (e, loc);
      ALIGNOF_EXPR_STD_P (e) = std_alignof;

      return e;
    }

  location_t e_loc = cp_expr_loc_or_loc (e, loc);
  STRIP_ANY_LOCATION_WRAPPER (e);

  e = mark_type_use (e);

  if (!verify_type_context (loc, TCTX_ALIGNOF, TREE_TYPE (e),
			    !(complain & tf_error)))
    {
      if (!(complain & tf_error))
	return error_mark_node;
      t = size_one_node;
    }
  else if (VAR_P (e))
    t = size_int (DECL_ALIGN_UNIT (e));
  else if (bitfield_p (e))
    {
      if (complain & tf_error)
	error_at (e_loc,
		  "invalid application of %<__alignof%> to a bit-field");
      else
        return error_mark_node;
      t = size_one_node;
    }
  else if (TREE_CODE (e) == COMPONENT_REF
	   && TREE_CODE (TREE_OPERAND (e, 1)) == FIELD_DECL)
    t = size_int (DECL_ALIGN_UNIT (TREE_OPERAND (e, 1)));
  else if (is_overloaded_fn (e))
    {
      if (complain & tf_error)
	permerror (e_loc, "ISO C++ forbids applying %<__alignof%> to "
		   "an expression of function type");
      else
        return error_mark_node;
      if (TREE_CODE (e) == FUNCTION_DECL)
	t = size_int (DECL_ALIGN_UNIT (e));
      else
	t = size_one_node;
    }
  else if (type_unknown_p (e))
    {
      if (complain & tf_error)
        cxx_incomplete_type_error (e_loc, e, TREE_TYPE (e));
      else
        return error_mark_node;
      t = size_one_node;
    }
  else
    return cxx_sizeof_or_alignof_type (loc, TREE_TYPE (e),
				       ALIGNOF_EXPR, std_alignof,
                                       complain & tf_error);

  return fold_convert_loc (loc, size_type_node, t);
}

/* Process a sizeof or alignof expression E with code OP where the operand
   is an expression. STD_ALIGNOF acts like in cxx_sizeof_or_alignof_type.  */

tree
cxx_sizeof_or_alignof_expr (location_t loc, tree e, enum tree_code op,
			    bool std_alignof, bool complain)
{
  gcc_assert (op == SIZEOF_EXPR || op == ALIGNOF_EXPR);
  if (op == SIZEOF_EXPR)
    return cxx_sizeof_expr (loc, e, complain? tf_warning_or_error : tf_none);
  else
    return cxx_alignof_expr (loc, e, std_alignof,
			     complain? tf_warning_or_error : tf_none);
}

/*  Build a representation of an expression 'alignas(E).'  Return the
    folded integer value of E if it is an integral constant expression
    that resolves to a valid alignment.  If E depends on a template
    parameter, return a syntactic representation tree of kind
    ALIGNOF_EXPR.  Otherwise, return an error_mark_node if the
    expression is ill formed, or NULL_TREE if E is NULL_TREE.  */

tree
cxx_alignas_expr (tree e)
{
  if (e == NULL_TREE || e == error_mark_node
      || (!TYPE_P (e) && !require_potential_rvalue_constant_expression (e)))
    return e;

  if (TYPE_P (e))
    /* [dcl.align]/3:

	   When the alignment-specifier is of the form
	   alignas(type-id), it shall have the same effect as
	   alignas(alignof(type-id)).  */

    return cxx_sizeof_or_alignof_type (input_location,
				       e, ALIGNOF_EXPR,
				       /*std_alignof=*/true,
				       /*complain=*/true);

  /* If we reach this point, it means the alignas expression if of
     the form "alignas(assignment-expression)", so we should follow
     what is stated by [dcl.align]/2.  */

  if (value_dependent_expression_p (e))
    /* Leave value-dependent expression alone for now. */
    return e;

  e = instantiate_non_dependent_expr (e);
  e = mark_rvalue_use (e);

  /* [dcl.align]/2 says:

         the assignment-expression shall be an integral constant
	 expression.  */

  if (!INTEGRAL_OR_UNSCOPED_ENUMERATION_TYPE_P (TREE_TYPE (e)))
    {
      error ("%<alignas%> argument has non-integral type %qT", TREE_TYPE (e));
      return error_mark_node;
    }

  return cxx_constant_value (e);
}


/* EXPR is being used in a context that is not a function call.
   Enforce:

     [expr.ref]

     The expression can be used only as the left-hand operand of a
     member function call.

     [expr.mptr.operator]

     If the result of .* or ->* is a function, then that result can be
     used only as the operand for the function call operator ().

   by issuing an error message if appropriate.  Returns true iff EXPR
   violates these rules.  */

bool
invalid_nonstatic_memfn_p (location_t loc, tree expr, tsubst_flags_t complain)
{
  if (expr == NULL_TREE)
    return false;
  /* Don't enforce this in MS mode.  */
  if (flag_ms_extensions)
    return false;
  if (is_overloaded_fn (expr) && !really_overloaded_fn (expr))
    expr = get_first_fn (expr);
  if (TREE_TYPE (expr)
      && DECL_IOBJ_MEMBER_FUNCTION_P (expr))
    {
      if (complain & tf_error)
	{
	  if (DECL_P (expr))
	    {
	      auto_diagnostic_group d;
	      error_at (loc, "invalid use of non-static member function %qD",
			expr);
	      inform (DECL_SOURCE_LOCATION (expr), "declared here");
	    }
	  else
	    error_at (loc, "invalid use of non-static member function of "
		      "type %qT", TREE_TYPE (expr));
	}
      return true;
    }
  return false;
}

/* If EXP is a reference to a bit-field, and the type of EXP does not
   match the declared type of the bit-field, return the declared type
   of the bit-field.  Otherwise, return NULL_TREE.  */

tree
is_bitfield_expr_with_lowered_type (const_tree exp)
{
  switch (TREE_CODE (exp))
    {
    case COND_EXPR:
      if (!is_bitfield_expr_with_lowered_type (TREE_OPERAND (exp, 1)
					       ? TREE_OPERAND (exp, 1)
					       : TREE_OPERAND (exp, 0)))
	return NULL_TREE;
      return is_bitfield_expr_with_lowered_type (TREE_OPERAND (exp, 2));

    case COMPOUND_EXPR:
      return is_bitfield_expr_with_lowered_type (TREE_OPERAND (exp, 1));

    case MODIFY_EXPR:
    case SAVE_EXPR:
    case UNARY_PLUS_EXPR:
    case PREDECREMENT_EXPR:
    case PREINCREMENT_EXPR:
    case POSTDECREMENT_EXPR:
    case POSTINCREMENT_EXPR:
    case NEGATE_EXPR:
    case NON_LVALUE_EXPR:
    case BIT_NOT_EXPR:
    case CLEANUP_POINT_EXPR:
      return is_bitfield_expr_with_lowered_type (TREE_OPERAND (exp, 0));

    case COMPONENT_REF:
      {
	tree field;

	field = TREE_OPERAND (exp, 1);
	if (TREE_CODE (field) != FIELD_DECL || !DECL_BIT_FIELD_TYPE (field))
	  return NULL_TREE;
	if (same_type_ignoring_top_level_qualifiers_p
	    (TREE_TYPE (exp), DECL_BIT_FIELD_TYPE (field)))
	  return NULL_TREE;
	return DECL_BIT_FIELD_TYPE (field);
      }

    case VAR_DECL:
      if (DECL_HAS_VALUE_EXPR_P (exp))
	return is_bitfield_expr_with_lowered_type (DECL_VALUE_EXPR
						   (CONST_CAST_TREE (exp)));
      return NULL_TREE;

    case VIEW_CONVERT_EXPR:
      if (location_wrapper_p (exp))
	return is_bitfield_expr_with_lowered_type (TREE_OPERAND (exp, 0));
      else
	return NULL_TREE;

    default:
      return NULL_TREE;
    }
}

/* Like is_bitfield_with_lowered_type, except that if EXP is not a
   bitfield with a lowered type, the type of EXP is returned, rather
   than NULL_TREE.  */

tree
unlowered_expr_type (const_tree exp)
{
  tree type;
  tree etype = TREE_TYPE (exp);

  type = is_bitfield_expr_with_lowered_type (exp);
  if (type)
    type = cp_build_qualified_type (type, cp_type_quals (etype));
  else
    type = etype;

  return type;
}

/* Perform the conversions in [expr] that apply when an lvalue appears
   in an rvalue context: the lvalue-to-rvalue, array-to-pointer, and
   function-to-pointer conversions.  In addition, bitfield references are
   converted to their declared types. Note that this function does not perform
   the lvalue-to-rvalue conversion for class types. If you need that conversion
   for class types, then you probably need to use force_rvalue.

   Although the returned value is being used as an rvalue, this
   function does not wrap the returned expression in a
   NON_LVALUE_EXPR; the caller is expected to be mindful of the fact
   that the return value is no longer an lvalue.  */

tree
decay_conversion (tree exp,
		  tsubst_flags_t complain,
		  bool reject_builtin /* = true */)
{
  tree type;
  enum tree_code code;
  location_t loc = cp_expr_loc_or_input_loc (exp);

  type = TREE_TYPE (exp);
  if (type == error_mark_node)
    return error_mark_node;

  exp = resolve_nondeduced_context_or_error (exp, complain);

  code = TREE_CODE (type);

  if (error_operand_p (exp))
    return error_mark_node;

  if (NULLPTR_TYPE_P (type) && !TREE_SIDE_EFFECTS (exp))
    {
      mark_rvalue_use (exp, loc, reject_builtin);
      return nullptr_node;
    }

  /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
     Leave such NOP_EXPRs, since RHS is being used in non-lvalue context.  */
  if (code == VOID_TYPE)
    {
      if (complain & tf_error)
	error_at (loc, "void value not ignored as it ought to be");
      return error_mark_node;
    }
  if (invalid_nonstatic_memfn_p (loc, exp, complain))
    return error_mark_node;
  if (code == FUNCTION_TYPE || is_overloaded_fn (exp))
    {
      exp = mark_lvalue_use (exp);
      if (reject_builtin && reject_gcc_builtin (exp, loc))
	return error_mark_node;
      return cp_build_addr_expr (exp, complain);
    }
  if (code == ARRAY_TYPE)
    {
      tree adr;
      tree ptrtype;

      exp = mark_lvalue_use (exp);

      if (INDIRECT_REF_P (exp))
	return build_nop (build_pointer_type (TREE_TYPE (type)),
			  TREE_OPERAND (exp, 0));

      if (TREE_CODE (exp) == COMPOUND_EXPR)
	{
	  tree op1 = decay_conversion (TREE_OPERAND (exp, 1), complain);
	  if (op1 == error_mark_node)
            return error_mark_node;
	  return build2 (COMPOUND_EXPR, TREE_TYPE (op1),
			 TREE_OPERAND (exp, 0), op1);
	}

      if (!obvalue_p (exp)
	  && ! (TREE_CODE (exp) == CONSTRUCTOR && TREE_STATIC (exp)))
	{
	  if (complain & tf_error)
	    error_at (loc, "invalid use of non-lvalue array");
	  return error_mark_node;
	}

      ptrtype = build_pointer_type (TREE_TYPE (type));

      if (VAR_P (exp))
	{
	  if (!cxx_mark_addressable (exp))
	    return error_mark_node;
	  adr = build_nop (ptrtype, build_address (exp));
	  return adr;
	}
      /* This way is better for a COMPONENT_REF since it can
	 simplify the offset for a component.  */
      adr = cp_build_addr_expr (exp, complain);
      return cp_convert (ptrtype, adr, complain);
    }

  /* Otherwise, it's the lvalue-to-rvalue conversion.  */
  exp = mark_rvalue_use (exp, loc, reject_builtin);

  /* If a bitfield is used in a context where integral promotion
     applies, then the caller is expected to have used
     default_conversion.  That function promotes bitfields correctly
     before calling this function.  At this point, if we have a
     bitfield referenced, we may assume that is not subject to
     promotion, and that, therefore, the type of the resulting rvalue
     is the declared type of the bitfield.  */
  exp = convert_bitfield_to_declared_type (exp);

  /* We do not call rvalue() here because we do not want to wrap EXP
     in a NON_LVALUE_EXPR.  */

  /* [basic.lval]

     Non-class rvalues always have cv-unqualified types.  */
  type = TREE_TYPE (exp);
  if (!CLASS_TYPE_P (type) && cv_qualified_p (type))
    exp = build_nop (cv_unqualified (type), exp);

  if (!complete_type_or_maybe_complain (type, exp, complain))
    return error_mark_node;

  return exp;
}

/* Perform preparatory conversions, as part of the "usual arithmetic
   conversions".  In particular, as per [expr]:

     Whenever an lvalue expression appears as an operand of an
     operator that expects the rvalue for that operand, the
     lvalue-to-rvalue, array-to-pointer, or function-to-pointer
     standard conversions are applied to convert the expression to an
     rvalue.

   In addition, we perform integral promotions here, as those are
   applied to both operands to a binary operator before determining
   what additional conversions should apply.  */

static tree
cp_default_conversion (tree exp, tsubst_flags_t complain)
{
  /* Check for target-specific promotions.  */
  tree promoted_type = targetm.promoted_type (TREE_TYPE (exp));
  if (promoted_type)
    exp = cp_convert (promoted_type, exp, complain);
  /* Perform the integral promotions first so that bitfield
     expressions (which may promote to "int", even if the bitfield is
     declared "unsigned") are promoted correctly.  */
  else if (INTEGRAL_OR_UNSCOPED_ENUMERATION_TYPE_P (TREE_TYPE (exp)))
    exp = cp_perform_integral_promotions (exp, complain);
  /* Perform the other conversions.  */
  exp = decay_conversion (exp, complain);

  return exp;
}

/* C version.  */

tree
default_conversion (tree exp)
{
  return cp_default_conversion (exp, tf_warning_or_error);
}

/* EXPR is an expression with an integral or enumeration type.
   Perform the integral promotions in [conv.prom], and return the
   converted value.  */

tree
cp_perform_integral_promotions (tree expr, tsubst_flags_t complain)
{
  tree type;
  tree promoted_type;

  expr = mark_rvalue_use (expr);
  if (error_operand_p (expr))
    return error_mark_node;

  type = TREE_TYPE (expr);

  /* [conv.prom]

     A prvalue for an integral bit-field (11.3.9) can be converted to a prvalue
     of type int if int can represent all the values of the bit-field;
     otherwise, it can be converted to unsigned int if unsigned int can
     represent all the values of the bit-field. If the bit-field is larger yet,
     no integral promotion applies to it. If the bit-field has an enumerated
     type, it is treated as any other value of that type for promotion
     purposes.  */
  tree bitfield_type = is_bitfield_expr_with_lowered_type (expr);
  if (bitfield_type
      && (TREE_CODE (bitfield_type) == ENUMERAL_TYPE
	  || TYPE_PRECISION (type) > TYPE_PRECISION (integer_type_node)))
    type = bitfield_type;

  gcc_assert (INTEGRAL_OR_ENUMERATION_TYPE_P (type));
  /* Scoped enums don't promote.  */
  if (SCOPED_ENUM_P (type))
    return expr;
  promoted_type = type_promotes_to (type);
  if (type != promoted_type)
    expr = cp_convert (promoted_type, expr, complain);
  else if (bitfield_type && bitfield_type != type)
    /* Prevent decay_conversion from converting to bitfield_type.  */
    expr = build_nop (type, expr);
  return expr;
}

/* C version.  */

tree
perform_integral_promotions (tree expr)
{
  return cp_perform_integral_promotions (expr, tf_warning_or_error);
}

/* Returns nonzero iff exp is a STRING_CST or the result of applying
   decay_conversion to one.  */

int
string_conv_p (const_tree totype, const_tree exp, int warn)
{
  tree t;

  if (!TYPE_PTR_P (totype))
    return 0;

  t = TREE_TYPE (totype);
  if (!same_type_p (t, char_type_node)
      && !same_type_p (t, char8_type_node)
      && !same_type_p (t, char16_type_node)
      && !same_type_p (t, char32_type_node)
      && !same_type_p (t, wchar_type_node))
    return 0;

  location_t loc = EXPR_LOC_OR_LOC (exp, input_location);

  STRIP_ANY_LOCATION_WRAPPER (exp);

  if (TREE_CODE (exp) == STRING_CST)
    {
      /* Make sure that we don't try to convert between char and wide chars.  */
      if (!same_type_p (TYPE_MAIN_VARIANT (TREE_TYPE (TREE_TYPE (exp))), t))
	return 0;
    }
  else
    {
      /* Is this a string constant which has decayed to 'const char *'?  */
      t = build_pointer_type (cp_build_qualified_type (t, TYPE_QUAL_CONST));
      if (!same_type_p (TREE_TYPE (exp), t))
	return 0;
      STRIP_NOPS (exp);
      if (TREE_CODE (exp) != ADDR_EXPR
	  || TREE_CODE (TREE_OPERAND (exp, 0)) != STRING_CST)
	return 0;
    }
  if (warn)
    {
      if (cxx_dialect >= cxx11)
	pedwarn (loc, OPT_Wwrite_strings,
		 "ISO C++ forbids converting a string constant to %qT",
		 totype);
      else
	warning_at (loc, OPT_Wwrite_strings,
		    "deprecated conversion from string constant to %qT",
		    totype);
    }

  return 1;
}

/* Given a COND_EXPR, MIN_EXPR, or MAX_EXPR in T, return it in a form that we
   can, for example, use as an lvalue.  This code used to be in
   unary_complex_lvalue, but we needed it to deal with `a = (d == c) ? b : c'
   expressions, where we're dealing with aggregates.  But now it's again only
   called from unary_complex_lvalue.  The case (in particular) that led to
   this was with CODE == ADDR_EXPR, since it's not an lvalue when we'd
   get it there.  */

static tree
rationalize_conditional_expr (enum tree_code code, tree t,
                              tsubst_flags_t complain)
{
  location_t loc = cp_expr_loc_or_input_loc (t);

  /* For MIN_EXPR or MAX_EXPR, fold-const.cc has arranged things so that
     the first operand is always the one to be used if both operands
     are equal, so we know what conditional expression this used to be.  */
  if (TREE_CODE (t) == MIN_EXPR || TREE_CODE (t) == MAX_EXPR)
    {
      tree op0 = TREE_OPERAND (t, 0);
      tree op1 = TREE_OPERAND (t, 1);

      /* The following code is incorrect if either operand side-effects.  */
      gcc_assert (!TREE_SIDE_EFFECTS (op0)
		  && !TREE_SIDE_EFFECTS (op1));
      return
	build_conditional_expr (loc,
				build_x_binary_op (loc,
						   (TREE_CODE (t) == MIN_EXPR
						    ? LE_EXPR : GE_EXPR),
						   op0, TREE_CODE (op0),
						   op1, TREE_CODE (op1),
						   NULL_TREE,
						   /*overload=*/NULL,
						   complain),
                                cp_build_unary_op (code, op0, false, complain),
                                cp_build_unary_op (code, op1, false, complain),
                                complain);
    }

  tree op1 = TREE_OPERAND (t, 1);
  if (TREE_CODE (op1) != THROW_EXPR)
    op1 = cp_build_unary_op (code, op1, false, complain);
  tree op2 = TREE_OPERAND (t, 2);
  if (TREE_CODE (op2) != THROW_EXPR)
    op2 = cp_build_unary_op (code, op2, false, complain);

  return
    build_conditional_expr (loc, TREE_OPERAND (t, 0), op1, op2, complain);
}

/* Given the TYPE of an anonymous union field inside T, return the
   FIELD_DECL for the field.  If not found return NULL_TREE.  Because
   anonymous unions can nest, we must also search all anonymous unions
   that are directly reachable.  */

tree
lookup_anon_field (tree, tree type)
{
  tree field;

  type = TYPE_MAIN_VARIANT (type);
  field = ANON_AGGR_TYPE_FIELD (type);
  gcc_assert (field);
  return field;
}

/* Build an expression representing OBJECT.MEMBER.  OBJECT is an
   expression; MEMBER is a DECL or baselink.  If ACCESS_PATH is
   non-NULL, it indicates the path to the base used to name MEMBER.
   If PRESERVE_REFERENCE is true, the expression returned will have
   REFERENCE_TYPE if the MEMBER does.  Otherwise, the expression
   returned will have the type referred to by the reference.

   This function does not perform access control; that is either done
   earlier by the parser when the name of MEMBER is resolved to MEMBER
   itself, or later when overload resolution selects one of the
   functions indicated by MEMBER.  */

tree
build_class_member_access_expr (cp_expr object, tree member,
				tree access_path, bool preserve_reference,
				tsubst_flags_t complain)
{
  tree object_type;
  tree member_scope;
  tree result = NULL_TREE;
  tree using_decl = NULL_TREE;

  if (error_operand_p (object) || error_operand_p (member))
    return error_mark_node;

  gcc_assert (DECL_P (member) || BASELINK_P (member));

  /* [expr.ref]

     The type of the first expression shall be "class object" (of a
     complete type).  */
  object_type = TREE_TYPE (object);
  if (!currently_open_class (object_type)
      && !complete_type_or_maybe_complain (object_type, object, complain))
    return error_mark_node;
  if (!CLASS_TYPE_P (object_type))
    {
      if (complain & tf_error)
	{
	  if (INDIRECT_TYPE_P (object_type)
	      && CLASS_TYPE_P (TREE_TYPE (object_type)))
	    error ("request for member %qD in %qE, which is of pointer "
		   "type %qT (maybe you meant to use %<->%> ?)",
		   member, object.get_value (), object_type);
	  else
	    error ("request for member %qD in %qE, which is of non-class "
		   "type %qT", member, object.get_value (), object_type);
	}
      return error_mark_node;
    }

  /* The standard does not seem to actually say that MEMBER must be a
     member of OBJECT_TYPE.  However, that is clearly what is
     intended.  */
  if (DECL_P (member))
    {
      member_scope = DECL_CLASS_CONTEXT (member);
      if (!mark_used (member, complain) && !(complain & tf_error))
	return error_mark_node;

      if (TREE_UNAVAILABLE (member))
	error_unavailable_use (member, NULL_TREE);
      else if (TREE_DEPRECATED (member))
	warn_deprecated_use (member, NULL_TREE);
    }
  else
    member_scope = BINFO_TYPE (BASELINK_ACCESS_BINFO (member));
  /* If MEMBER is from an anonymous aggregate, MEMBER_SCOPE will
     presently be the anonymous union.  Go outwards until we find a
     type related to OBJECT_TYPE.  */
  while ((ANON_AGGR_TYPE_P (member_scope) || UNSCOPED_ENUM_P (member_scope))
	 && !same_type_ignoring_top_level_qualifiers_p (member_scope,
							object_type))
    member_scope = TYPE_CONTEXT (member_scope);
  if (!member_scope || !DERIVED_FROM_P (member_scope, object_type))
    {
      if (complain & tf_error)
	{
	  if (TREE_CODE (member) == FIELD_DECL)
	    error ("invalid use of non-static data member %qE", member);
	  else
	    error ("%qD is not a member of %qT", member, object_type);
	}
      return error_mark_node;
    }

  /* Transform `(a, b).x' into `(*(a, &b)).x', `(a ? b : c).x' into
     `(*(a ?  &b : &c)).x', and so on.  A COND_EXPR is only an lvalue
     in the front end; only _DECLs and _REFs are lvalues in the back end.  */
  if (tree temp = unary_complex_lvalue (ADDR_EXPR, object))
    {
      temp = cp_build_fold_indirect_ref (temp);
      if (!lvalue_p (object) && lvalue_p (temp))
	/* Preserve rvalueness.  */
	temp = move (temp);
      object = temp;
    }

  /* In [expr.ref], there is an explicit list of the valid choices for
     MEMBER.  We check for each of those cases here.  */
  if (VAR_P (member))
    {
      /* A static data member.  */
      result = member;
      mark_exp_read (object);

      if (tree wrap = maybe_get_tls_wrapper_call (result))
	/* Replace an evaluated use of the thread_local variable with
	   a call to its wrapper.  */
	result = wrap;

      /* If OBJECT has side-effects, they are supposed to occur.  */
      if (TREE_SIDE_EFFECTS (object))
	result = build2 (COMPOUND_EXPR, TREE_TYPE (result), object, result);
    }
  else if (TREE_CODE (member) == FIELD_DECL)
    {
      /* A non-static data member.  */
      bool null_object_p;
      int type_quals;
      tree member_type;

      if (INDIRECT_REF_P (object))
	null_object_p =
	  integer_zerop (tree_strip_nop_conversions (TREE_OPERAND (object, 0)));
      else
	null_object_p = false;

      /* Convert OBJECT to the type of MEMBER.  */
      if (!same_type_p (TYPE_MAIN_VARIANT (object_type),
			TYPE_MAIN_VARIANT (member_scope)))
	{
	  tree binfo;
	  base_kind kind;

	  /* We didn't complain above about a currently open class, but now we
	     must: we don't know how to refer to a base member before layout is
	     complete.  But still don't complain in a template.  */
	  if (!cp_unevaluated_operand
	      && !dependent_type_p (object_type)
	      && !complete_type_or_maybe_complain (object_type, object,
						   complain))
	    return error_mark_node;

	  binfo = lookup_base (access_path ? access_path : object_type,
			       member_scope, ba_unique, &kind, complain);
	  if (binfo == error_mark_node)
	    return error_mark_node;

	  /* It is invalid to try to get to a virtual base of a
	     NULL object.  The most common cause is invalid use of
	     offsetof macro.  */
	  if (null_object_p && kind == bk_via_virtual)
	    {
	      if (complain & tf_error)
		{
		  error ("invalid access to non-static data member %qD in "
			 "virtual base of NULL object", member);
		}
	      return error_mark_node;
	    }

	  /* Convert to the base.  */
	  object = build_base_path (PLUS_EXPR, object, binfo,
				    /*nonnull=*/1, complain);
	  /* If we found the base successfully then we should be able
	     to convert to it successfully.  */
	  gcc_assert (object != error_mark_node || seen_error ());
	}

      /* If MEMBER is from an anonymous aggregate, we have converted
	 OBJECT so that it refers to the class containing the
	 anonymous union.  Generate a reference to the anonymous union
	 itself, and recur to find MEMBER.  */
      if (ANON_AGGR_TYPE_P (DECL_CONTEXT (member))
	  /* When this code is called from build_field_call, the
	     object already has the type of the anonymous union.
	     That is because the COMPONENT_REF was already
	     constructed, and was then disassembled before calling
	     build_field_call.  After the function-call code is
	     cleaned up, this waste can be eliminated.  */
	  && (!same_type_ignoring_top_level_qualifiers_p
	      (TREE_TYPE (object), DECL_CONTEXT (member))))
	{
	  tree anonymous_union;

	  anonymous_union = lookup_anon_field (TREE_TYPE (object),
					       DECL_CONTEXT (member));
	  object = build_class_member_access_expr (object,
						   anonymous_union,
						   /*access_path=*/NULL_TREE,
						   preserve_reference,
						   complain);
	}

      /* Compute the type of the field, as described in [expr.ref].  */
      type_quals = TYPE_UNQUALIFIED;
      member_type = TREE_TYPE (member);
      if (!TYPE_REF_P (member_type))
	{
	  type_quals = (cp_type_quals (member_type)
			| cp_type_quals (object_type));

	  /* A field is const (volatile) if the enclosing object, or the
	     field itself, is const (volatile).  But, a mutable field is
	     not const, even within a const object.  */
	  if (DECL_MUTABLE_P (member))
	    type_quals &= ~TYPE_QUAL_CONST;
	  member_type = cp_build_qualified_type (member_type, type_quals);
	}

      result = build3_loc (input_location, COMPONENT_REF, member_type,
			   object, member, NULL_TREE);

      /* Mark the expression const or volatile, as appropriate.  Even
	 though we've dealt with the type above, we still have to mark the
	 expression itself.  */
      if (type_quals & TYPE_QUAL_CONST)
	TREE_READONLY (result) = 1;
      if (type_quals & TYPE_QUAL_VOLATILE)
	TREE_THIS_VOLATILE (result) = 1;
    }
  else if (BASELINK_P (member))
    {
      /* The member is a (possibly overloaded) member function.  */
      tree functions;
      tree type;

      /* If the MEMBER is exactly one static member function, then we
	 know the type of the expression.  Otherwise, we must wait
	 until overload resolution has been performed.  */
      functions = BASELINK_FUNCTIONS (member);
      if (TREE_CODE (functions) == OVERLOAD && OVL_SINGLE_P (functions))
	functions = OVL_FIRST (functions);
      if (TREE_CODE (functions) == FUNCTION_DECL
	  && DECL_STATIC_FUNCTION_P (functions))
	type = TREE_TYPE (functions);
      else
	type = unknown_type_node;
      /* Note that we do not convert OBJECT to the BASELINK_BINFO
	 base.  That will happen when the function is called.  */
      result = build3_loc (input_location, COMPONENT_REF, type, object, member,
			   NULL_TREE);
    }
  else if (TREE_CODE (member) == CONST_DECL)
    {
      /* The member is an enumerator.  */
      result = member;
      /* If OBJECT has side-effects, they are supposed to occur.  */
      if (TREE_SIDE_EFFECTS (object))
	result = build2 (COMPOUND_EXPR, TREE_TYPE (result),
			 object, result);
    }
  else if ((using_decl = strip_using_decl (member)) != member)
    result = build_class_member_access_expr (object,
					     using_decl,
					     access_path, preserve_reference,
					     complain);
  else
    {
      if (complain & tf_error)
	error ("invalid use of %qD", member);
      return error_mark_node;
    }

  if (!preserve_reference)
    /* [expr.ref]

       If E2 is declared to have type "reference to T", then ... the
       type of E1.E2 is T.  */
    result = convert_from_reference (result);

  return result;
}

/* Return the destructor denoted by OBJECT.SCOPE::DTOR_NAME, or, if
   SCOPE is NULL, by OBJECT.DTOR_NAME, where DTOR_NAME is ~type.  */

tree
lookup_destructor (tree object, tree scope, tree dtor_name,
		   tsubst_flags_t complain)
{
  tree object_type = TREE_TYPE (object);
  tree dtor_type = TREE_OPERAND (dtor_name, 0);
  tree expr;

  /* We've already complained about this destructor.  */
  if (dtor_type == error_mark_node)
    return error_mark_node;

  if (scope && !check_dtor_name (scope, dtor_type))
    {
      if (complain & tf_error)
	error ("qualified type %qT does not match destructor name ~%qT",
	       scope, dtor_type);
      return error_mark_node;
    }
  if (is_auto (dtor_type))
    dtor_type = object_type;
  else if (identifier_p (dtor_type))
    {
      /* In a template, names we can't find a match for are still accepted
	 destructor names, and we check them here.  */
      if (check_dtor_name (object_type, dtor_type))
	dtor_type = object_type;
      else
	{
	  if (complain & tf_error)
	    error ("object type %qT does not match destructor name ~%qT",
		   object_type, dtor_type);
	  return error_mark_node;
	}

    }
  else if (!DERIVED_FROM_P (dtor_type, TYPE_MAIN_VARIANT (object_type)))
    {
      if (complain & tf_error)
	error ("the type being destroyed is %qT, but the destructor "
	       "refers to %qT", TYPE_MAIN_VARIANT (object_type), dtor_type);
      return error_mark_node;
    }
  expr = lookup_member (dtor_type, complete_dtor_identifier,
			/*protect=*/1, /*want_type=*/false,
			tf_warning_or_error);
  if (!expr)
    {
      if (complain & tf_error)
	cxx_incomplete_type_error (dtor_name, dtor_type);
      return error_mark_node;
    }
  expr = (adjust_result_of_qualified_name_lookup
	  (expr, dtor_type, object_type));
  if (scope == NULL_TREE)
    /* We need to call adjust_result_of_qualified_name_lookup in case the
       destructor names a base class, but we unset BASELINK_QUALIFIED_P so
       that we still get virtual function binding.  */
    BASELINK_QUALIFIED_P (expr) = false;
  return expr;
}

/* An expression of the form "A::template B" has been resolved to
   DECL.  Issue a diagnostic if B is not a template or template
   specialization.  */

void
check_template_keyword (tree decl)
{
  /* The standard says:

      [temp.names]

      If a name prefixed by the keyword template is not a member
      template, the program is ill-formed.

     DR 228 removed the restriction that the template be a member
     template.

     DR 96, if accepted would add the further restriction that explicit
     template arguments must be provided if the template keyword is
     used, but, as of 2005-10-16, that DR is still in "drafting".  If
     this DR is accepted, then the semantic checks here can be
     simplified, as the entity named must in fact be a template
     specialization, rather than, as at present, a set of overloaded
     functions containing at least one template function.  */
  if (TREE_CODE (decl) != TEMPLATE_DECL
      && TREE_CODE (decl) != TEMPLATE_ID_EXPR)
    {
      if (VAR_P (decl))
	{
	  if (DECL_USE_TEMPLATE (decl)
	      && PRIMARY_TEMPLATE_P (DECL_TI_TEMPLATE (decl)))
	    ;
	  else
	    permerror (input_location, "%qD is not a template", decl);
	}
      else if (!is_overloaded_fn (decl))
	permerror (input_location, "%qD is not a template", decl);
      else
	{
	  bool found = false;

	  for (lkp_iterator iter (MAYBE_BASELINK_FUNCTIONS (decl));
	       !found && iter; ++iter)
	    {
	      tree fn = *iter;
	      if (TREE_CODE (fn) == TEMPLATE_DECL
		  || TREE_CODE (fn) == TEMPLATE_ID_EXPR
		  || (TREE_CODE (fn) == FUNCTION_DECL
		      && DECL_USE_TEMPLATE (fn)
		      && PRIMARY_TEMPLATE_P (DECL_TI_TEMPLATE (fn))))
		found = true;
	    }
	  if (!found)
	    permerror (input_location, "%qD is not a template", decl);
	}
    }
}

/* Record that an access failure occurred on BASETYPE_PATH attempting
   to access DECL, where DIAG_DECL should be used for diagnostics.  */

void
access_failure_info::record_access_failure (tree basetype_path,
					    tree decl, tree diag_decl)
{
  m_was_inaccessible = true;
  m_basetype_path = basetype_path;
  m_decl = decl;
  m_diag_decl = diag_decl;
}

/* If an access failure was recorded, then attempt to locate an
   accessor function for the pertinent field.
   Otherwise, return NULL_TREE.  */

tree
access_failure_info::get_any_accessor (bool const_p) const
{
  if (!was_inaccessible_p ())
    return NULL_TREE;

  tree accessor
    = locate_field_accessor (m_basetype_path, m_diag_decl, const_p);
  if (!accessor)
    return NULL_TREE;

  /* The accessor must itself be accessible for it to be a reasonable
     suggestion.  */
  if (!accessible_p (m_basetype_path, accessor, true))
    return NULL_TREE;

  return accessor;
}

/* Add a fix-it hint to RICHLOC suggesting the use of ACCESSOR_DECL, by
   replacing the primary location in RICHLOC with "accessor()".  */

void
access_failure_info::add_fixit_hint (rich_location *richloc,
				     tree accessor_decl)
{
  pretty_printer pp;
  pp_string (&pp, IDENTIFIER_POINTER (DECL_NAME (accessor_decl)));
  pp_string (&pp, "()");
  richloc->add_fixit_replace (pp_formatted_text (&pp));
}

/* If an access failure was recorded, then attempt to locate an
   accessor function for the pertinent field, and if one is
   available, add a note and fix-it hint suggesting using it.  */

void
access_failure_info::maybe_suggest_accessor (bool const_p) const
{
  tree accessor = get_any_accessor (const_p);
  if (accessor == NULL_TREE)
    return;
  rich_location richloc (line_table, input_location);
  add_fixit_hint (&richloc, accessor);
  inform (&richloc, "field %q#D can be accessed via %q#D",
	  m_diag_decl, accessor);
}

/* Subroutine of finish_class_member_access_expr.
   Issue an error about NAME not being a member of ACCESS_PATH (or
   OBJECT_TYPE), potentially providing a fix-it hint for misspelled
   names.  */

static void
complain_about_unrecognized_member (tree access_path, tree name,
				    tree object_type)
{
  /* Attempt to provide a hint about misspelled names.  */
  tree guessed_id = lookup_member_fuzzy (access_path, name,
					 /*want_type=*/false);
  if (guessed_id == NULL_TREE)
    {
      /* No hint.  */
      error ("%q#T has no member named %qE",
	     TREE_CODE (access_path) == TREE_BINFO
	     ? TREE_TYPE (access_path) : object_type, name);
      return;
    }

  location_t bogus_component_loc = input_location;
  gcc_rich_location rich_loc (bogus_component_loc);

  /* Check that the guessed name is accessible along access_path.  */
  access_failure_info afi;
  lookup_member (access_path, guessed_id, /*protect=*/1,
		 /*want_type=*/false, /*complain=*/false,
		 &afi);
  if (afi.was_inaccessible_p ())
    {
      tree accessor = afi.get_any_accessor (TYPE_READONLY (object_type));
      if (accessor)
	{
	  /* The guessed name isn't directly accessible, but can be accessed
	     via an accessor member function.  */
	  afi.add_fixit_hint (&rich_loc, accessor);
	  error_at (&rich_loc,
		    "%q#T has no member named %qE;"
		    " did you mean %q#D? (accessible via %q#D)",
		    TREE_CODE (access_path) == TREE_BINFO
		    ? TREE_TYPE (access_path) : object_type,
		    name, afi.get_diag_decl (), accessor);
	}
      else
	{
	  /* The guessed name isn't directly accessible, and no accessor
	     member function could be found.  */
	  auto_diagnostic_group d;
	  error_at (&rich_loc,
		    "%q#T has no member named %qE;"
		    " did you mean %q#D? (not accessible from this context)",
		    TREE_CODE (access_path) == TREE_BINFO
		    ? TREE_TYPE (access_path) : object_type,
		    name, afi.get_diag_decl ());
	  complain_about_access (afi.get_decl (), afi.get_diag_decl (),
				 afi.get_diag_decl (), false, ak_none);
	}
    }
  else
    {
      /* The guessed name is directly accessible; suggest it.  */
      rich_loc.add_fixit_misspelled_id (bogus_component_loc,
					guessed_id);
      error_at (&rich_loc,
		"%q#T has no member named %qE;"
		" did you mean %qE?",
		TREE_CODE (access_path) == TREE_BINFO
		? TREE_TYPE (access_path) : object_type,
		name, guessed_id);
    }
}

/* This function is called by the parser to process a class member
   access expression of the form OBJECT.NAME.  NAME is a node used by
   the parser to represent a name; it is not yet a DECL.  It may,
   however, be a BASELINK where the BASELINK_FUNCTIONS is a
   TEMPLATE_ID_EXPR.  Templates must be looked up by the parser, and
   there is no reason to do the lookup twice, so the parser keeps the
   BASELINK.  TEMPLATE_P is true iff NAME was explicitly declared to
   be a template via the use of the "A::template B" syntax.  */

tree
finish_class_member_access_expr (cp_expr object, tree name, bool template_p,
				 tsubst_flags_t complain)
{
  tree expr;
  tree object_type;
  tree member;
  tree access_path = NULL_TREE;
  tree orig_object = object;
  tree orig_name = name;

  if (object == error_mark_node || name == error_mark_node)
    return error_mark_node;

  /* If OBJECT is an ObjC class instance, we must obey ObjC access rules.  */
  if (!objc_is_public (object, name))
    return error_mark_node;

  object_type = TREE_TYPE (object);

  if (processing_template_decl)
    {
      if (/* If OBJECT is dependent, so is OBJECT.NAME.  */
	  type_dependent_object_expression_p (object)
	  /* If NAME is "f<args>", where either 'f' or 'args' is
	     dependent, then the expression is dependent.  */
	  || (TREE_CODE (name) == TEMPLATE_ID_EXPR
	      && dependent_template_id_p (TREE_OPERAND (name, 0),
					  TREE_OPERAND (name, 1)))
	  /* If NAME is "T::X" where "T" is dependent, then the
	     expression is dependent.  */
	  || (TREE_CODE (name) == SCOPE_REF
	      && TYPE_P (TREE_OPERAND (name, 0))
	      && dependent_scope_p (TREE_OPERAND (name, 0)))
	  /* If NAME is operator T where "T" is dependent, we can't
	     lookup until we instantiate the T.  */
	  || (TREE_CODE (name) == IDENTIFIER_NODE
	      && IDENTIFIER_CONV_OP_P (name)
	      && dependent_type_p (TREE_TYPE (name))))
	{
	dependent:
	  return build_min_nt_loc (UNKNOWN_LOCATION, COMPONENT_REF,
				   orig_object, orig_name, NULL_TREE);
	}
    }
  else if (c_dialect_objc ()
	   && identifier_p (name)
	   && (expr = objc_maybe_build_component_ref (object, name)))
    return expr;

  /* [expr.ref]

     The type of the first expression shall be "class object" (of a
     complete type).  */
  if (!currently_open_class (object_type)
      && !complete_type_or_maybe_complain (object_type, object, complain))
    return error_mark_node;
  if (!CLASS_TYPE_P (object_type))
    {
      if (complain & tf_error)
	{
	  if (INDIRECT_TYPE_P (object_type)
	      && CLASS_TYPE_P (TREE_TYPE (object_type)))
	    error ("request for member %qD in %qE, which is of pointer "
		   "type %qT (maybe you meant to use %<->%> ?)",
		   name, object.get_value (), object_type);
	  else
	    error ("request for member %qD in %qE, which is of non-class "
		   "type %qT", name, object.get_value (), object_type);
	}
      return error_mark_node;
    }

  if (BASELINK_P (name))
    /* A member function that has already been looked up.  */
    member = name;
  else
    {
      bool is_template_id = false;
      tree template_args = NULL_TREE;
      tree scope = NULL_TREE;

      access_path = object_type;

      if (TREE_CODE (name) == SCOPE_REF)
	{
	  /* A qualified name.  The qualifying class or namespace `S'
	     has already been looked up; it is either a TYPE or a
	     NAMESPACE_DECL.  */
	  scope = TREE_OPERAND (name, 0);
	  name = TREE_OPERAND (name, 1);

	  /* If SCOPE is a namespace, then the qualified name does not
	     name a member of OBJECT_TYPE.  */
	  if (TREE_CODE (scope) == NAMESPACE_DECL)
	    {
	      if (complain & tf_error)
		error ("%<%D::%D%> is not a member of %qT",
		       scope, name, object_type);
	      return error_mark_node;
	    }
	}

      if (TREE_CODE (name) == TEMPLATE_ID_EXPR)
	{
	  is_template_id = true;
	  template_args = TREE_OPERAND (name, 1);
	  name = TREE_OPERAND (name, 0);

	  if (!identifier_p (name))
	    name = OVL_NAME (name);
	}

      if (scope)
	{
	  if (TREE_CODE (scope) == ENUMERAL_TYPE)
	    {
	      gcc_assert (!is_template_id);
	      /* Looking up a member enumerator (c++/56793).  */
	      if (!TYPE_CLASS_SCOPE_P (scope)
		  || !DERIVED_FROM_P (TYPE_CONTEXT (scope), object_type))
		{
		  if (complain & tf_error)
		    error ("%<%D::%D%> is not a member of %qT",
			   scope, name, object_type);
		  return error_mark_node;
		}
	      tree val = lookup_enumerator (scope, name);
	      if (!val)
		{
		  if (complain & tf_error)
		    error ("%qD is not a member of %qD",
			   name, scope);
		  return error_mark_node;
		}

	      if (TREE_SIDE_EFFECTS (object))
		val = build2 (COMPOUND_EXPR, TREE_TYPE (val), object, val);
	      return val;
	    }

	  gcc_assert (CLASS_TYPE_P (scope));
	  gcc_assert (identifier_p (name) || TREE_CODE (name) == BIT_NOT_EXPR);

	  if (constructor_name_p (name, scope))
	    {
	      if (complain & tf_error)
		error ("cannot call constructor %<%T::%D%> directly",
		       scope, name);
	      return error_mark_node;
	    }

	  /* NAME may refer to a static data member, in which case there is
	     one copy of the data member that is shared by all the objects of
	     the class.  So NAME can be unambiguously referred to even if
	     there are multiple indirect base classes containing NAME.  */
	  const base_access ba = [scope, name] ()
	    {
	      if (identifier_p (name))
		{
		  tree m = lookup_member (scope, name, /*protect=*/0,
					  /*want_type=*/false, tf_none);
		  if (!m || shared_member_p (m))
		    return ba_any;
		}
	      return ba_check;
	    } ();

	  /* Find the base of OBJECT_TYPE corresponding to SCOPE.  */
	  access_path = lookup_base (object_type, scope, ba, NULL, complain);
	  if (access_path == error_mark_node)
	    return error_mark_node;
	  if (!access_path)
	    {
	      if (any_dependent_bases_p (object_type))
		goto dependent;
	      if (complain & tf_error)
		error ("%qT is not a base of %qT", scope, object_type);
	      return error_mark_node;
	    }
	}

      if (TREE_CODE (name) == BIT_NOT_EXPR)
	{
	  if (dependent_type_p (object_type))
	    /* The destructor isn't declared yet.  */
	    goto dependent;
	  member = lookup_destructor (object, scope, name, complain);
	}
      else
	{
	  /* Look up the member.  */
	  {
	    auto_diagnostic_group d;
	    access_failure_info afi;
	    if (processing_template_decl)
	      /* Even though this class member access expression is at this
		 point not dependent, the member itself may be dependent, and
		 we must not potentially push a access check for a dependent
		 member onto TI_DEFERRED_ACCESS_CHECKS.  So don't check access
		 ahead of time here; we're going to redo this member lookup at
		 instantiation time anyway.  */
	      push_deferring_access_checks (dk_no_check);
	    member = lookup_member (access_path, name, /*protect=*/1,
				    /*want_type=*/false, complain,
				    &afi);
	    if (processing_template_decl)
	      pop_deferring_access_checks ();
	    afi.maybe_suggest_accessor (TYPE_READONLY (object_type));
	  }
	  if (member == NULL_TREE)
	    {
	      if (dependentish_scope_p (object_type))
		/* Try again at instantiation time.  */
		goto dependent;
	      if (complain & tf_error)
		complain_about_unrecognized_member (access_path, name,
						    object_type);
	      return error_mark_node;
	    }
	  if (member == error_mark_node)
	    return error_mark_node;
	  if (DECL_P (member)
	      && any_dependent_type_attributes_p (DECL_ATTRIBUTES (member)))
	    /* Dependent type attributes on the decl mean that the TREE_TYPE is
	       wrong, so don't use it.  */
	    goto dependent;
	  if (TREE_CODE (member) == USING_DECL && DECL_DEPENDENT_P (member))
	    goto dependent;
	}

      if (is_template_id)
	{
	  tree templ = member;

	  if (BASELINK_P (templ))
	    member = lookup_template_function (templ, template_args);
	  else if (variable_template_p (templ))
	    member = (lookup_and_finish_template_variable
		      (templ, template_args, complain));
	  else
	    {
	      if (complain & tf_error)
		error ("%qD is not a member template function", name);
	      return error_mark_node;
	    }
	}
    }

  if (TREE_UNAVAILABLE (member))
    error_unavailable_use (member, NULL_TREE);
  else if (TREE_DEPRECATED (member))
    warn_deprecated_use (member, NULL_TREE);

  if (template_p)
    check_template_keyword (member);

  expr = build_class_member_access_expr (object, member, access_path,
					 /*preserve_reference=*/false,
					 complain);
  if (processing_template_decl && expr != error_mark_node)
    {
      if (BASELINK_P (member))
	{
	  if (TREE_CODE (orig_name) == SCOPE_REF)
	    BASELINK_QUALIFIED_P (member) = 1;
	  orig_name = member;
	}
      return build_min_non_dep (COMPONENT_REF, expr,
				orig_object, orig_name,
				NULL_TREE);
    }

  return expr;
}

/* Build a COMPONENT_REF of OBJECT and MEMBER with the appropriate
   type.  */

tree
build_simple_component_ref (tree object, tree member)
{
  tree type = cp_build_qualified_type (TREE_TYPE (member),
				       cp_type_quals (TREE_TYPE (object)));
  return build3_loc (input_location,
		     COMPONENT_REF, type,
		     object, member, NULL_TREE);
}

/* Return an expression for the MEMBER_NAME field in the internal
   representation of PTRMEM, a pointer-to-member function.  (Each
   pointer-to-member function type gets its own RECORD_TYPE so it is
   more convenient to access the fields by name than by FIELD_DECL.)
   This routine converts the NAME to a FIELD_DECL and then creates the
   node for the complete expression.  */

tree
build_ptrmemfunc_access_expr (tree ptrmem, tree member_name)
{
  tree ptrmem_type;
  tree member;

  if (TREE_CODE (ptrmem) == CONSTRUCTOR)
    {
      for (auto &e: CONSTRUCTOR_ELTS (ptrmem))
	if (e.index && DECL_P (e.index) && DECL_NAME (e.index) == member_name)
	  return e.value;
      gcc_unreachable ();
    }

  /* This code is a stripped down version of
     build_class_member_access_expr.  It does not work to use that
     routine directly because it expects the object to be of class
     type.  */
  ptrmem_type = TREE_TYPE (ptrmem);
  gcc_assert (TYPE_PTRMEMFUNC_P (ptrmem_type));
  for (member = TYPE_FIELDS (ptrmem_type); member;
       member = DECL_CHAIN (member))
    if (DECL_NAME (member) == member_name)
      break;
  return build_simple_component_ref (ptrmem, member);
}

/* Return a TREE_LIST of namespace-scope overloads for the given operator,
   and for any other relevant operator.  */

static tree
op_unqualified_lookup (tree_code code, bool is_assign)
{
  tree lookups = NULL_TREE;

  if (cxx_dialect >= cxx20 && !is_assign)
    {
      if (code == NE_EXPR)
	{
	  /* != can get rewritten in terms of ==.  */
	  tree fnname = ovl_op_identifier (false, EQ_EXPR);
	  if (tree fns = lookup_name (fnname, LOOK_where::BLOCK_NAMESPACE))
	    lookups = tree_cons (fnname, fns, lookups);
	}
      else if (code == GT_EXPR || code == LE_EXPR
	       || code == LT_EXPR || code == GE_EXPR)
	{
	  /* These can get rewritten in terms of <=>.  */
	  tree fnname = ovl_op_identifier (false, SPACESHIP_EXPR);
	  if (tree fns = lookup_name (fnname, LOOK_where::BLOCK_NAMESPACE))
	    lookups = tree_cons (fnname, fns, lookups);
	}
    }

  tree fnname = ovl_op_identifier (is_assign, code);
  if (tree fns = lookup_name (fnname, LOOK_where::BLOCK_NAMESPACE))
    lookups = tree_cons (fnname, fns, lookups);

  if (lookups)
    return lookups;
  else
    return build_tree_list (NULL_TREE, NULL_TREE);
}

/* Create a DEPENDENT_OPERATOR_TYPE for a dependent operator expression of
   the given operator.  LOOKUPS, if non-NULL, is the result of phase 1
   name lookup for the given operator.  */

tree
build_dependent_operator_type (tree lookups, tree_code code, bool is_assign)
{
  if (lookups)
    /* We're partially instantiating a dependent operator expression, and
       LOOKUPS is the result of phase 1 name lookup that we performed
       earlier at template definition time, so just reuse the corresponding
       DEPENDENT_OPERATOR_TYPE.  */
    return TREE_TYPE (lookups);

  /* Otherwise we're processing a dependent operator expression at template
     definition time, so perform phase 1 name lookup now.  */
  lookups = op_unqualified_lookup (code, is_assign);

  tree type = cxx_make_type (DEPENDENT_OPERATOR_TYPE);
  DEPENDENT_OPERATOR_TYPE_SAVED_LOOKUPS (type) = lookups;
  TREE_TYPE (lookups) = type;
  return type;
}

/* Given an expression PTR for a pointer, return an expression
   for the value pointed to.
   ERRORSTRING is the name of the operator to appear in error messages.

   This function may need to overload OPERATOR_FNNAME.
   Must also handle REFERENCE_TYPEs for C++.  */

tree
build_x_indirect_ref (location_t loc, tree expr, ref_operator errorstring,
		      tree lookups, tsubst_flags_t complain)
{
  tree orig_expr = expr;
  tree rval;
  tree overload = NULL_TREE;

  if (processing_template_decl)
    {
      /* Retain the type if we know the operand is a pointer.  */
      if (TREE_TYPE (expr) && INDIRECT_TYPE_P (TREE_TYPE (expr)))
	{
	  if (expr == current_class_ptr
	      || (TREE_CODE (expr) == NOP_EXPR
		  && TREE_OPERAND (expr, 0) == current_class_ptr
		  && (same_type_ignoring_top_level_qualifiers_p
			(TREE_TYPE (expr), TREE_TYPE (current_class_ptr)))))
	    return current_class_ref;
	  return build_min (INDIRECT_REF, TREE_TYPE (TREE_TYPE (expr)), expr);
	}
      if (type_dependent_expression_p (expr))
	{
	  expr = build_min_nt_loc (loc, INDIRECT_REF, expr);
	  TREE_TYPE (expr)
	    = build_dependent_operator_type (lookups, INDIRECT_REF, false);
	  return expr;
	}
    }

  rval = build_new_op (loc, INDIRECT_REF, LOOKUP_NORMAL, expr,
		       NULL_TREE, NULL_TREE, lookups,
		       &overload, complain);
  if (!rval)
    rval = cp_build_indirect_ref (loc, expr, errorstring, complain);

  if (processing_template_decl && rval != error_mark_node)
    {
      if (overload != NULL_TREE)
	return (build_min_non_dep_op_overload
		(INDIRECT_REF, rval, overload, orig_expr));

      return build_min_non_dep (INDIRECT_REF, rval, orig_expr);
    }
  else
    return rval;
}

/* Like c-family strict_aliasing_warning, but don't warn for dependent
   types or expressions.  */

static bool
cp_strict_aliasing_warning (location_t loc, tree type, tree expr)
{
  if (processing_template_decl)
    {
      tree e = expr;
      STRIP_NOPS (e);
      if (dependent_type_p (type) || type_dependent_expression_p (e))
	return false;
    }
  return strict_aliasing_warning (loc, type, expr);
}

/* The implementation of the above, and of indirection implied by other
   constructs.  If DO_FOLD is true, fold away INDIRECT_REF of ADDR_EXPR.  */

static tree
cp_build_indirect_ref_1 (location_t loc, tree ptr, ref_operator errorstring,
			 tsubst_flags_t complain, bool do_fold)
{
  tree pointer, type;

  /* RO_NULL should only be used with the folding entry points below, not
     cp_build_indirect_ref.  */
  gcc_checking_assert (errorstring != RO_NULL || do_fold);

  if (ptr == current_class_ptr
      || (TREE_CODE (ptr) == NOP_EXPR
	  && TREE_OPERAND (ptr, 0) == current_class_ptr
	  && (same_type_ignoring_top_level_qualifiers_p
	      (TREE_TYPE (ptr), TREE_TYPE (current_class_ptr)))))
    return current_class_ref;

  pointer = (TYPE_REF_P (TREE_TYPE (ptr))
	     ? ptr : decay_conversion (ptr, complain));
  if (pointer == error_mark_node)
    return error_mark_node;

  type = TREE_TYPE (pointer);

  if (INDIRECT_TYPE_P (type))
    {
      /* [expr.unary.op]

	 If the type of the expression is "pointer to T," the type
	 of  the  result  is  "T."  */
      tree t = TREE_TYPE (type);

      if ((CONVERT_EXPR_P (ptr)
	   || TREE_CODE (ptr) == VIEW_CONVERT_EXPR)
	  && (!CLASS_TYPE_P (t) || !CLASSTYPE_EMPTY_P (t)))
	{
	  /* If a warning is issued, mark it to avoid duplicates from
	     the backend.  This only needs to be done at
	     warn_strict_aliasing > 2.  */
	  if (warn_strict_aliasing > 2
	      && cp_strict_aliasing_warning (EXPR_LOCATION (ptr),
					     type, TREE_OPERAND (ptr, 0)))
	    suppress_warning (ptr, OPT_Wstrict_aliasing);
	}

      if (VOID_TYPE_P (t))
	{
	  /* A pointer to incomplete type (other than cv void) can be
	     dereferenced [expr.unary.op]/1  */
          if (complain & tf_error)
            error_at (loc, "%qT is not a pointer-to-object type", type);
	  return error_mark_node;
	}
      else if (do_fold && TREE_CODE (pointer) == ADDR_EXPR
	       && same_type_p (t, TREE_TYPE (TREE_OPERAND (pointer, 0))))
	/* The POINTER was something like `&x'.  We simplify `*&x' to
	   `x'.  This can change the value category: '*&TARGET_EXPR'
	   is an lvalue and folding it into 'TARGET_EXPR' turns it into
	   a prvalue of class type.  */
	return TREE_OPERAND (pointer, 0);
      else
	{
	  tree ref = build1 (INDIRECT_REF, t, pointer);

	  /* We *must* set TREE_READONLY when dereferencing a pointer to const,
	     so that we get the proper error message if the result is used
	     to assign to.  Also, &* is supposed to be a no-op.  */
	  TREE_READONLY (ref) = CP_TYPE_CONST_P (t);
	  TREE_THIS_VOLATILE (ref) = CP_TYPE_VOLATILE_P (t);
	  TREE_SIDE_EFFECTS (ref)
	    = (TREE_THIS_VOLATILE (ref) || TREE_SIDE_EFFECTS (pointer));
	  return ref;
	}
    }
  else if (!(complain & tf_error))
    /* Don't emit any errors; we'll just return ERROR_MARK_NODE later.  */
    ;
  /* `pointer' won't be an error_mark_node if we were given a
     pointer to member, so it's cool to check for this here.  */
  else if (TYPE_PTRMEM_P (type))
    switch (errorstring)
      {
         case RO_ARRAY_INDEXING:
           error_at (loc,
		     "invalid use of array indexing on pointer to member");
           break;
         case RO_UNARY_STAR:
           error_at (loc, "invalid use of unary %<*%> on pointer to member");
           break;
         case RO_IMPLICIT_CONVERSION:
           error_at (loc, "invalid use of implicit conversion on pointer "
		     "to member");
           break;
         case RO_ARROW_STAR:
           error_at (loc, "left hand operand of %<->*%> must be a pointer to "
		     "class, but is a pointer to member of type %qT", type);
           break;
         default:
           gcc_unreachable ();
      }
  else if (pointer != error_mark_node)
    invalid_indirection_error (loc, type, errorstring);

  return error_mark_node;
}

/* Entry point used by c-common, which expects folding.  */

tree
build_indirect_ref (location_t loc, tree ptr, ref_operator errorstring)
{
  return cp_build_indirect_ref_1 (loc, ptr, errorstring,
				  tf_warning_or_error, true);
}

/* Entry point used by internal indirection needs that don't correspond to any
   syntactic construct.  */

tree
cp_build_fold_indirect_ref (tree pointer)
{
  return cp_build_indirect_ref_1 (input_location, pointer, RO_NULL,
				  tf_warning_or_error, true);
}

/* Entry point used by indirection needs that correspond to some syntactic
   construct.  */

tree
cp_build_indirect_ref (location_t loc, tree ptr, ref_operator errorstring,
		       tsubst_flags_t complain)
{
  return cp_build_indirect_ref_1 (loc, ptr, errorstring, complain, false);
}

/* This handles expressions of the form "a[i]", which denotes
   an array reference.

   This is logically equivalent in C to *(a+i), but we may do it differently.
   If A is a variable or a member, we generate a primitive ARRAY_REF.
   This avoids forcing the array out of registers, and can work on
   arrays that are not lvalues (for example, members of structures returned
   by functions).

   If INDEX is of some user-defined type, it must be converted to
   integer type.  Otherwise, to make a compatible PLUS_EXPR, it
   will inherit the type of the array, which will be some pointer type.

   LOC is the location to use in building the array reference.  */

tree
cp_build_array_ref (location_t loc, tree array, tree idx,
		    tsubst_flags_t complain)
{
  tree first = NULL_TREE;
  tree ret;

  if (idx == 0)
    {
      if (complain & tf_error)
	error_at (loc, "subscript missing in array reference");
      return error_mark_node;
    }

  if (TREE_TYPE (array) == error_mark_node
      || TREE_TYPE (idx) == error_mark_node)
    return error_mark_node;

  /* If ARRAY is a COMPOUND_EXPR or COND_EXPR, move our reference
     inside it.  */
  switch (TREE_CODE (array))
    {
    case COMPOUND_EXPR:
      {
	tree value = cp_build_array_ref (loc, TREE_OPERAND (array, 1), idx,
					 complain);
	ret = build2 (COMPOUND_EXPR, TREE_TYPE (value),
		      TREE_OPERAND (array, 0), value);
	SET_EXPR_LOCATION (ret, loc);
	return ret;
      }

    case COND_EXPR:
      tree op0, op1, op2;
      op0 = TREE_OPERAND (array, 0);
      op1 = TREE_OPERAND (array, 1);
      op2 = TREE_OPERAND (array, 2);
      if (TREE_SIDE_EFFECTS (idx) || !tree_invariant_p (idx))
	{
	  /* If idx could possibly have some SAVE_EXPRs, turning
	     (op0 ? op1 : op2)[idx] into
	     op0 ? op1[idx] : op2[idx] can lead into temporaries
	     initialized in one conditional path and uninitialized
	     uses of them in the other path.
	     And if idx is a really large expression, evaluating it
	     twice is also not optimal.
	     On the other side, op0 must be sequenced before evaluation
	     of op1 and op2 and for C++17 op0, op1 and op2 must be
	     sequenced before idx.
	     If idx is INTEGER_CST, we can just do the optimization
	     without any SAVE_EXPRs, if op1 and op2 are both ARRAY_TYPE
	     VAR_DECLs or COMPONENT_REFs thereof (so their address
	     is constant or relative to frame), optimize into
	     (SAVE_EXPR <op0>, SAVE_EXPR <idx>, SAVE_EXPR <op0>)
	     ? op1[SAVE_EXPR <idx>] : op2[SAVE_EXPR <idx>]
	     Otherwise avoid this optimization.  */
	  if (flag_strong_eval_order == 2)
	    {
	      if (TREE_CODE (TREE_TYPE (array)) == ARRAY_TYPE)
		{
		  if (!address_invariant_p (op1) || !address_invariant_p (op2))
		    {
		      /* Force default conversion on array if
			 we can't optimize this and array is ARRAY_TYPE
			 COND_EXPR, we can't leave COND_EXPRs with
			 ARRAY_TYPE in the IL.  */
		      array = cp_default_conversion (array, complain);
		      if (error_operand_p (array))
			return error_mark_node;
		      break;
		    }
		}
	      else if (!POINTER_TYPE_P (TREE_TYPE (array))
		       || !tree_invariant_p (op1)
		       || !tree_invariant_p (op2))
		break;
	    }
	  if (TREE_SIDE_EFFECTS (idx))
	    {
	      idx = save_expr (idx);
	      op0 = save_expr (op0);
	      tree tem = build_compound_expr (loc, op0, idx);
	      op0 = build_compound_expr (loc, tem, op0);
	    }
	}
      op1 = cp_build_array_ref (loc, op1, idx, complain);
      op2 = cp_build_array_ref (loc, op2, idx, complain);
      ret = build_conditional_expr (loc, op0, op1, op2, complain);
      protected_set_expr_location (ret, loc);
      return ret;

    default:
      break;
    }

  bool non_lvalue = convert_vector_to_array_for_subscript (loc, &array, idx);

  /* 0[array] */
  if (TREE_CODE (TREE_TYPE (idx)) == ARRAY_TYPE)
    {
      std::swap (array, idx);
      if (flag_strong_eval_order == 2 && TREE_SIDE_EFFECTS (array))
	idx = first = save_expr (idx);
    }

  if (TREE_CODE (TREE_TYPE (array)) == ARRAY_TYPE)
    {
      tree rval, type;

      warn_array_subscript_with_type_char (loc, idx);

      if (!INTEGRAL_OR_UNSCOPED_ENUMERATION_TYPE_P (TREE_TYPE (idx)))
	{
	  if (complain & tf_error)
	    error_at (loc, "array subscript is not an integer");
	  return error_mark_node;
	}

      /* Apply integral promotions *after* noticing character types.
	 (It is unclear why we do these promotions -- the standard
	 does not say that we should.  In fact, the natural thing would
	 seem to be to convert IDX to ptrdiff_t; we're performing
	 pointer arithmetic.)  */
      idx = cp_perform_integral_promotions (idx, complain);

      idx = maybe_fold_non_dependent_expr (idx, complain);

      /* An array that is indexed by a non-constant
	 cannot be stored in a register; we must be able to do
	 address arithmetic on its address.
	 Likewise an array of elements of variable size.  */
      if (TREE_CODE (idx) != INTEGER_CST
	  || (COMPLETE_TYPE_P (TREE_TYPE (TREE_TYPE (array)))
	      && (TREE_CODE (TYPE_SIZE (TREE_TYPE (TREE_TYPE (array))))
		  != INTEGER_CST)))
	{
	  if (!cxx_mark_addressable (array, true))
	    return error_mark_node;
	}

      /* An array that is indexed by a constant value which is not within
	 the array bounds cannot be stored in a register either; because we
	 would get a crash in store_bit_field/extract_bit_field when trying
	 to access a non-existent part of the register.  */
      if (TREE_CODE (idx) == INTEGER_CST
	  && TYPE_DOMAIN (TREE_TYPE (array))
	  && ! int_fits_type_p (idx, TYPE_DOMAIN (TREE_TYPE (array))))
	{
	  if (!cxx_mark_addressable (array))
	    return error_mark_node;
	}

      /* Note in C++ it is valid to subscript a `register' array, since
	 it is valid to take the address of something with that
	 storage specification.  */
      if (extra_warnings)
	{
	  tree foo = array;
	  while (TREE_CODE (foo) == COMPONENT_REF)
	    foo = TREE_OPERAND (foo, 0);
	  if (VAR_P (foo) && DECL_REGISTER (foo)
	      && (complain & tf_warning))
	    warning_at (loc, OPT_Wextra,
			"subscripting array declared %<register%>");
	}

      type = TREE_TYPE (TREE_TYPE (array));
      rval = build4 (ARRAY_REF, type, array, idx, NULL_TREE, NULL_TREE);
      /* Array ref is const/volatile if the array elements are
	 or if the array is..  */
      TREE_READONLY (rval)
	|= (CP_TYPE_CONST_P (type) | TREE_READONLY (array));
      TREE_SIDE_EFFECTS (rval)
	|= (CP_TYPE_VOLATILE_P (type) | TREE_SIDE_EFFECTS (array));
      TREE_THIS_VOLATILE (rval)
	|= (CP_TYPE_VOLATILE_P (type) | TREE_THIS_VOLATILE (array));
      ret = require_complete_type (rval, complain);
      protected_set_expr_location (ret, loc);
      if (non_lvalue)
	ret = non_lvalue_loc (loc, ret);
      if (first)
	ret = build2_loc (loc, COMPOUND_EXPR, TREE_TYPE (ret), first, ret);
      return ret;
    }

  {
    tree ar = cp_default_conversion (array, complain);
    tree ind = cp_default_conversion (idx, complain);

    if (!processing_template_decl
	&& !first && flag_strong_eval_order == 2 && TREE_SIDE_EFFECTS (ind))
      ar = first = save_expr (ar);

    /* Put the integer in IND to simplify error checking.  */
    if (TREE_CODE (TREE_TYPE (ar)) == INTEGER_TYPE)
      std::swap (ar, ind);

    if (ar == error_mark_node || ind == error_mark_node)
      return error_mark_node;

    if (!TYPE_PTR_P (TREE_TYPE (ar)))
      {
	if (complain & tf_error)
	  error_at (loc, "subscripted value is neither array nor pointer");
	return error_mark_node;
      }
    if (TREE_CODE (TREE_TYPE (ind)) != INTEGER_TYPE)
      {
	if (complain & tf_error)
	  error_at (loc, "array subscript is not an integer");
	return error_mark_node;
      }

    warn_array_subscript_with_type_char (loc, idx);

    ret = cp_build_binary_op (input_location, PLUS_EXPR, ar, ind, complain);
    if (first)
      ret = build2_loc (loc, COMPOUND_EXPR, TREE_TYPE (ret), first, ret);
    ret = cp_build_indirect_ref (loc, ret, RO_ARRAY_INDEXING, complain);
    protected_set_expr_location (ret, loc);
    if (non_lvalue)
      ret = non_lvalue_loc (loc, ret);
    return ret;
  }
}

/* Entry point for Obj-C++.  */

tree
build_array_ref (location_t loc, tree array, tree idx)
{
  return cp_build_array_ref (loc, array, idx, tf_warning_or_error);
}

/* Resolve a pointer to member function.  INSTANCE is the object
   instance to use, if the member points to a virtual member.

   This used to avoid checking for virtual functions if basetype
   has no virtual functions, according to an earlier ANSI draft.
   With the final ISO C++ rules, such an optimization is
   incorrect: A pointer to a derived member can be static_cast
   to pointer-to-base-member, as long as the dynamic object
   later has the right member.  So now we only do this optimization
   when we know the dynamic type of the object.  */

tree
get_member_function_from_ptrfunc (tree *instance_ptrptr, tree function,
				  tsubst_flags_t complain)
{
  if (TREE_CODE (function) == OFFSET_REF)
    function = TREE_OPERAND (function, 1);

  if (TYPE_PTRMEMFUNC_P (TREE_TYPE (function)))
    {
      tree idx, delta, e1, e2, e3, vtbl;
      bool nonvirtual;
      tree fntype = TYPE_PTRMEMFUNC_FN_TYPE (TREE_TYPE (function));
      tree basetype = TYPE_METHOD_BASETYPE (TREE_TYPE (fntype));

      tree instance_ptr = *instance_ptrptr;
      tree instance_save_expr = 0;
      if (instance_ptr == error_mark_node)
	{
	  if (TREE_CODE (function) == PTRMEM_CST)
	    {
	      /* Extracting the function address from a pmf is only
		 allowed with -Wno-pmf-conversions. It only works for
		 pmf constants.  */
	      e1 = build_addr_func (PTRMEM_CST_MEMBER (function), complain);
	      e1 = convert (fntype, e1);
	      return e1;
	    }
	  else
	    {
	      if (complain & tf_error)
		error ("object missing in use of %qE", function);
	      return error_mark_node;
	    }
	}

      /* True if we know that the dynamic type of the object doesn't have
	 virtual functions, so we can assume the PFN field is a pointer.  */
      nonvirtual = (COMPLETE_TYPE_P (basetype)
		    && !TYPE_POLYMORPHIC_P (basetype)
		    && resolves_to_fixed_type_p (instance_ptr, 0));

      /* If we don't really have an object (i.e. in an ill-formed
	 conversion from PMF to pointer), we can't resolve virtual
	 functions anyway.  */
      if (!nonvirtual && is_dummy_object (instance_ptr))
	nonvirtual = true;

      /* Use force_target_expr even when instance_ptr doesn't have
	 side-effects, unless it is a simple decl or constant, so
	 that we don't ubsan instrument the expression multiple times.
	 Don't use save_expr, as save_expr can avoid building a SAVE_EXPR
	 and building a SAVE_EXPR manually can be optimized away during
	 cp_fold.  See PR116449 and PR117259.  */
      if (TREE_SIDE_EFFECTS (instance_ptr)
	  || (!nonvirtual
	      && !DECL_P (instance_ptr)
	      && !TREE_CONSTANT (instance_ptr)))
	instance_ptr = instance_save_expr
	  = get_internal_target_expr (instance_ptr);

      /* See above comment.  */
      if (TREE_SIDE_EFFECTS (function)
	  || (!nonvirtual
	      && !DECL_P (function)
	      && !TREE_CONSTANT (function)))
	function = get_internal_target_expr (function);

      /* Start by extracting all the information from the PMF itself.  */
      e3 = pfn_from_ptrmemfunc (function);
      delta = delta_from_ptrmemfunc (function);
      idx = build1 (NOP_EXPR, vtable_index_type, e3);
      switch (TARGET_PTRMEMFUNC_VBIT_LOCATION)
	{
	  int flag_sanitize_save;
	case ptrmemfunc_vbit_in_pfn:
	  e1 = cp_build_binary_op (input_location,
				   BIT_AND_EXPR, idx, integer_one_node,
				   complain);
	  idx = cp_build_binary_op (input_location,
				    MINUS_EXPR, idx, integer_one_node,
				    complain);
	  if (idx == error_mark_node)
	    return error_mark_node;
	  break;

	case ptrmemfunc_vbit_in_delta:
	  e1 = cp_build_binary_op (input_location,
				   BIT_AND_EXPR, delta, integer_one_node,
				   complain);
	  /* Don't instrument the RSHIFT_EXPR we're about to create because
	     we're going to use DELTA number of times, and that wouldn't play
	     well with SAVE_EXPRs therein.  */
	  flag_sanitize_save = flag_sanitize;
	  flag_sanitize = 0;
	  delta = cp_build_binary_op (input_location,
				      RSHIFT_EXPR, delta, integer_one_node,
				      complain);
	  flag_sanitize = flag_sanitize_save;
	  if (delta == error_mark_node)
	    return error_mark_node;
	  break;

	default:
	  gcc_unreachable ();
	}

      if (e1 == error_mark_node)
	return error_mark_node;

      /* Convert down to the right base before using the instance.  A
	 special case is that in a pointer to member of class C, C may
	 be incomplete.  In that case, the function will of course be
	 a member of C, and no conversion is required.  In fact,
	 lookup_base will fail in that case, because incomplete
	 classes do not have BINFOs.  */
      if (!same_type_ignoring_top_level_qualifiers_p
	  (basetype, TREE_TYPE (TREE_TYPE (instance_ptr))))
	{
	  basetype = lookup_base (TREE_TYPE (TREE_TYPE (instance_ptr)),
				  basetype, ba_check, NULL, complain);
	  instance_ptr = build_base_path (PLUS_EXPR, instance_ptr, basetype,
					  1, complain);
	  if (instance_ptr == error_mark_node)
	    return error_mark_node;
	}
      /* ...and then the delta in the PMF.  */
      instance_ptr = fold_build_pointer_plus (instance_ptr, delta);

      /* Hand back the adjusted 'this' argument to our caller.  */
      *instance_ptrptr = instance_ptr;

      if (nonvirtual)
	/* Now just return the pointer.  */
	return e3;

      /* Next extract the vtable pointer from the object.  */
      vtbl = build1 (NOP_EXPR, build_pointer_type (vtbl_ptr_type_node),
		     instance_ptr);
      vtbl = cp_build_fold_indirect_ref (vtbl);
      if (vtbl == error_mark_node)
	return error_mark_node;

      /* Finally, extract the function pointer from the vtable.  */
      e2 = fold_build_pointer_plus_loc (input_location, vtbl, idx);
      e2 = cp_build_fold_indirect_ref (e2);
      if (e2 == error_mark_node)
	return error_mark_node;
      TREE_CONSTANT (e2) = 1;

      /* When using function descriptors, the address of the
	 vtable entry is treated as a function pointer.  */
      if (TARGET_VTABLE_USES_DESCRIPTORS)
	e2 = build1 (NOP_EXPR, TREE_TYPE (e2),
		     cp_build_addr_expr (e2, complain));

      e2 = fold_convert (TREE_TYPE (e3), e2);
      e1 = build_conditional_expr (input_location, e1, e2, e3, complain);
      if (e1 == error_mark_node)
	return error_mark_node;

      /* Make sure this doesn't get evaluated first inside one of the
	 branches of the COND_EXPR.  */
      if (instance_save_expr)
	e1 = build2 (COMPOUND_EXPR, TREE_TYPE (e1),
		     instance_save_expr, e1);

      function = e1;
    }
  return function;
}

/* Used by the C-common bits.  */
tree
build_function_call (location_t /*loc*/,
		     tree function, tree params)
{
  return cp_build_function_call (function, params, tf_warning_or_error);
}

/* Used by the C-common bits.  */
tree
build_function_call_vec (location_t /*loc*/, vec<location_t> /*arg_loc*/,
			 tree function, vec<tree, va_gc> *params,
			 vec<tree, va_gc> * /*origtypes*/, tree orig_function)
{
  vec<tree, va_gc> *orig_params = params;
  tree ret = cp_build_function_call_vec (function, &params,
					 tf_warning_or_error, orig_function);

  /* cp_build_function_call_vec can reallocate PARAMS by adding
     default arguments.  That should never happen here.  Verify
     that.  */
  gcc_assert (params == orig_params);

  return ret;
}

/* Build a function call using a tree list of arguments.  */

static tree
cp_build_function_call (tree function, tree params, tsubst_flags_t complain)
{
  tree ret;

  releasing_vec vec;
  for (; params != NULL_TREE; params = TREE_CHAIN (params))
    vec_safe_push (vec, TREE_VALUE (params));
  ret = cp_build_function_call_vec (function, &vec, complain);
  return ret;
}

/* Build a function call using varargs.  */

tree
cp_build_function_call_nary (tree function, tsubst_flags_t complain, ...)
{
  va_list args;
  tree ret, t;

  releasing_vec vec;
  va_start (args, complain);
  for (t = va_arg (args, tree); t != NULL_TREE; t = va_arg (args, tree))
    vec_safe_push (vec, t);
  va_end (args);
  ret = cp_build_function_call_vec (function, &vec, complain);
  return ret;
}

/* C++ implementation of callback for use when checking param types.  */

bool
cp_comp_parm_types (tree wanted_type, tree actual_type)
{
  if (TREE_CODE (wanted_type) == POINTER_TYPE
      && TREE_CODE (actual_type) == POINTER_TYPE)
    return same_or_base_type_p (TREE_TYPE (wanted_type),
				TREE_TYPE (actual_type));
  return false;
}

/* Build a function call using a vector of arguments.
   If FUNCTION is the result of resolving an overloaded target built-in,
   ORIG_FNDECL is the original function decl, otherwise it is null.
   PARAMS may be NULL if there are no parameters.  This changes the
   contents of PARAMS.  */

tree
cp_build_function_call_vec (tree function, vec<tree, va_gc> **params,
			    tsubst_flags_t complain, tree orig_fndecl)
{
  tree fntype, fndecl;
  int is_method;
  tree original = function;
  int nargs;
  tree *argarray;
  tree parm_types;
  vec<tree, va_gc> *allocated = NULL;
  tree ret;

  /* For Objective-C, convert any calls via a cast to OBJC_TYPE_REF
     expressions, like those used for ObjC messenger dispatches.  */
  if (params != NULL && !vec_safe_is_empty (*params))
    function = objc_rewrite_function_call (function, (**params)[0]);

  /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
     Strip such NOP_EXPRs, since FUNCTION is used in non-lvalue context.  */
  if (TREE_CODE (function) == NOP_EXPR
      && TREE_TYPE (function) == TREE_TYPE (TREE_OPERAND (function, 0)))
    function = TREE_OPERAND (function, 0);

  if (TREE_CODE (function) == FUNCTION_DECL)
    {
      if (!mark_used (function, complain))
	return error_mark_node;
      fndecl = function;

      /* Convert anything with function type to a pointer-to-function.  */
      if (DECL_MAIN_P (function))
	{
	  if (complain & tf_error)
	    pedwarn (input_location, OPT_Wpedantic,
		     "ISO C++ forbids calling %<::main%> from within program");
	  else
	    return error_mark_node;
	}
      function = build_addr_func (function, complain);
    }
  else
    {
      fndecl = NULL_TREE;

      function = build_addr_func (function, complain);
    }

  if (function == error_mark_node)
    return error_mark_node;

  fntype = TREE_TYPE (function);

  if (TYPE_PTRMEMFUNC_P (fntype))
    {
      if (complain & tf_error)
	error ("must use %<.*%> or %<->*%> to call pointer-to-member "
	       "function in %<%E (...)%>, e.g. %<(... ->* %E) (...)%>",
	       original, original);
      return error_mark_node;
    }

  is_method = (TYPE_PTR_P (fntype)
	       && TREE_CODE (TREE_TYPE (fntype)) == METHOD_TYPE);

  if (!(TYPE_PTRFN_P (fntype)
	|| is_method
	|| TREE_CODE (function) == TEMPLATE_ID_EXPR))
    {
      if (complain & tf_error)
	{
	  if (is_stub_object (original))
	    error_at (input_location,
		      "%qT cannot be used as a function",
		      TREE_TYPE (original));
	  else if (!flag_diagnostics_show_caret)
	    error_at (input_location,
		      "%qE cannot be used as a function", original);
	  else if (DECL_P (original))
	    error_at (input_location,
		      "%qD cannot be used as a function", original);
	  else
	    error_at (input_location,
		      "expression cannot be used as a function");
	}

      return error_mark_node;
    }

  /* fntype now gets the type of function pointed to.  */
  fntype = TREE_TYPE (fntype);
  parm_types = TYPE_ARG_TYPES (fntype);

  if (params == NULL)
    {
      allocated = make_tree_vector ();
      params = &allocated;
    }

    nargs = convert_arguments (parm_types, params, fndecl, LOOKUP_NORMAL,
			       complain);
  if (nargs < 0)
    return error_mark_node;

  argarray = (*params)->address ();

  /* Check for errors in format strings and inappropriately
     null parameters.  */
  bool warned_p
    = ((complain & tf_warning)
       && check_function_arguments (input_location, fndecl, fntype,
				    nargs, argarray, NULL,
				    cp_comp_parm_types));

  ret = build_cxx_call (function, nargs, argarray, complain, orig_fndecl);

  if (warned_p)
    {
      tree c = extract_call_expr (ret);
      if (TREE_CODE (c) == CALL_EXPR)
	suppress_warning (c, OPT_Wnonnull);
    }

  if (allocated != NULL)
    release_tree_vector (allocated);

  return ret;
}

/* Subroutine of convert_arguments.
   Print an error message about a wrong number of arguments.  */

static void
error_args_num (location_t loc, tree fndecl, bool too_many_p)
{
  if (fndecl)
    {
      auto_diagnostic_group d;
      if (TREE_CODE (TREE_TYPE (fndecl)) == METHOD_TYPE)
	{
	  if (DECL_NAME (fndecl) == NULL_TREE
	      || (DECL_NAME (fndecl)
		  == DECL_NAME (TYPE_NAME (DECL_CONTEXT (fndecl)))))
	    error_at (loc,
		      too_many_p
		      ? G_("too many arguments to constructor %q#D")
		      : G_("too few arguments to constructor %q#D"),
		      fndecl);
	  else
	    error_at (loc,
		      too_many_p
		      ? G_("too many arguments to member function %q#D")
		      : G_("too few arguments to member function %q#D"),
		      fndecl);
	}
      else
	error_at (loc,
		  too_many_p
		  ? G_("too many arguments to function %q#D")
		  : G_("too few arguments to function %q#D"),
		  fndecl);
      if (!DECL_IS_UNDECLARED_BUILTIN (fndecl))
	inform (DECL_SOURCE_LOCATION (fndecl), "declared here");
    }
  else
    {
      if (c_dialect_objc ()  &&  objc_message_selector ())
	error_at (loc,
		  too_many_p
		  ? G_("too many arguments to method %q#D")
		  : G_("too few arguments to method %q#D"),
		  objc_message_selector ());
      else
	error_at (loc, too_many_p ? G_("too many arguments to function")
		                  : G_("too few arguments to function"));
    }
}

/* Convert the actual parameter expressions in the list VALUES to the
   types in the list TYPELIST.  The converted expressions are stored
   back in the VALUES vector.
   If parmdecls is exhausted, or when an element has NULL as its type,
   perform the default conversions.

   NAME is an IDENTIFIER_NODE or 0.  It is used only for error messages.

   This is also where warnings about wrong number of args are generated.

   Returns the actual number of arguments processed (which might be less
   than the length of the vector), or -1 on error.

   In C++, unspecified trailing parameters can be filled in with their
   default arguments, if such were specified.  Do so here.  */

static int
convert_arguments (tree typelist, vec<tree, va_gc> **values, tree fndecl,
		   int flags, tsubst_flags_t complain)
{
  tree typetail;
  unsigned int i;

  /* Argument passing is always copy-initialization.  */
  flags |= LOOKUP_ONLYCONVERTING;

  for (i = 0, typetail = typelist;
       i < vec_safe_length (*values);
       i++)
    {
      tree type = typetail ? TREE_VALUE (typetail) : 0;
      tree val = (**values)[i];

      if (val == error_mark_node || type == error_mark_node)
	return -1;

      if (type == void_type_node)
	{
          if (complain & tf_error)
	    error_args_num (input_location, fndecl, /*too_many_p=*/true);
	  return -1;
	}

      /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
	 Strip such NOP_EXPRs, since VAL is used in non-lvalue context.  */
      if (TREE_CODE (val) == NOP_EXPR
	  && TREE_TYPE (val) == TREE_TYPE (TREE_OPERAND (val, 0))
	  && (type == 0 || !TYPE_REF_P (type)))
	val = TREE_OPERAND (val, 0);

      if (type == 0 || !TYPE_REF_P (type))
	{
	  if (TREE_CODE (TREE_TYPE (val)) == ARRAY_TYPE
	      || FUNC_OR_METHOD_TYPE_P (TREE_TYPE (val)))
	    val = decay_conversion (val, complain);
	}

      if (val == error_mark_node)
	return -1;

      if (type != 0)
	{
	  /* Formal parm type is specified by a function prototype.  */
	  tree parmval;

	  if (!COMPLETE_TYPE_P (complete_type (type)))
	    {
              if (complain & tf_error)
                {
		  location_t loc = EXPR_LOC_OR_LOC (val, input_location);
                  if (fndecl)
		    {
		      auto_diagnostic_group d;
		      error_at (loc,
				"parameter %P of %qD has incomplete type %qT",
				i, fndecl, type);
		      inform (get_fndecl_argument_location (fndecl, i),
			      "  declared here");
		    }
                  else
		    error_at (loc, "parameter %P has incomplete type %qT", i,
			      type);
                }
	      parmval = error_mark_node;
	    }
	  else
	    {
	      parmval = convert_for_initialization
		(NULL_TREE, type, val, flags,
		 ICR_ARGPASS, fndecl, i, complain);
	      parmval = convert_for_arg_passing (type, parmval, complain);
	    }

	  if (parmval == error_mark_node)
	    return -1;

	  (**values)[i] = parmval;
	}
      else
	{
	  int magic = fndecl ? magic_varargs_p (fndecl) : 0;
	  if (magic)
	    {
	      /* Don't truncate excess precision to the semantic type.  */
	      if (magic == 1 && TREE_CODE (val) == EXCESS_PRECISION_EXPR)
		val = TREE_OPERAND (val, 0);
	      /* Don't do ellipsis conversion for __built_in_constant_p
		 as this will result in spurious errors for non-trivial
		 types.  */
	      val = require_complete_type (val, complain);
	    }
	  else
	    val = convert_arg_to_ellipsis (val, complain);

	  (**values)[i] = val;
	}

      if (typetail)
	typetail = TREE_CHAIN (typetail);
    }

  if (typetail != 0 && typetail != void_list_node)
    {
      /* See if there are default arguments that can be used.  Because
	 we hold default arguments in the FUNCTION_TYPE (which is so
	 wrong), we can see default parameters here from deduced
	 contexts (and via typeof) for indirect function calls.
	 Fortunately we know whether we have a function decl to
	 provide default arguments in a language conformant
	 manner.  */
      if (fndecl && TREE_PURPOSE (typetail)
	  && TREE_CODE (TREE_PURPOSE (typetail)) != DEFERRED_PARSE)
	{
	  for (; typetail != void_list_node; ++i)
	    {
	      /* After DR777, with explicit template args we can end up with a
		 default argument followed by no default argument.  */
	      if (!TREE_PURPOSE (typetail))
		break;
	      tree parmval
		= convert_default_arg (TREE_VALUE (typetail),
				       TREE_PURPOSE (typetail),
				       fndecl, i, complain);

	      if (parmval == error_mark_node)
		return -1;

	      vec_safe_push (*values, parmval);
	      typetail = TREE_CHAIN (typetail);
	      /* ends with `...'.  */
	      if (typetail == NULL_TREE)
		break;
	    }
	}

      if (typetail && typetail != void_list_node)
	{
	  if (complain & tf_error)
	    error_args_num (input_location, fndecl, /*too_many_p=*/false);
	  return -1;
	}
    }

  return (int) i;
}

/* Build a binary-operation expression, after performing default
   conversions on the operands.  CODE is the kind of expression to
   build.  ARG1 and ARG2 are the arguments.  ARG1_CODE and ARG2_CODE
   are the tree codes which correspond to ARG1 and ARG2 when issuing
   warnings about possibly misplaced parentheses.  They may differ
   from the TREE_CODE of ARG1 and ARG2 if the parser has done constant
   folding (e.g., if the parser sees "a | 1 + 1", it may call this
   routine with ARG2 being an INTEGER_CST and ARG2_CODE == PLUS_EXPR).
   To avoid issuing any parentheses warnings, pass ARG1_CODE and/or
   ARG2_CODE as ERROR_MARK.  */

tree
build_x_binary_op (const op_location_t &loc, enum tree_code code, tree arg1,
		   enum tree_code arg1_code, tree arg2,
		   enum tree_code arg2_code, tree lookups,
		   tree *overload_p, tsubst_flags_t complain)
{
  tree orig_arg1;
  tree orig_arg2;
  tree expr;
  tree overload = NULL_TREE;

  orig_arg1 = arg1;
  orig_arg2 = arg2;

  if (processing_template_decl)
    {
      if (type_dependent_expression_p (arg1)
	  || type_dependent_expression_p (arg2))
	{
	  expr = build_min_nt_loc (loc, code, arg1, arg2);
	  TREE_TYPE (expr)
	    = build_dependent_operator_type (lookups, code, false);
	  return expr;
	}
    }

  if (code == DOTSTAR_EXPR)
    expr = build_m_component_ref (arg1, arg2, complain);
  else
    expr = build_new_op (loc, code, LOOKUP_NORMAL, arg1, arg2, NULL_TREE,
			 lookups, &overload, complain);

  if (overload_p != NULL)
    *overload_p = overload;

  /* Check for cases such as x+y<<z which users are likely to
     misinterpret.  But don't warn about obj << x + y, since that is a
     common idiom for I/O.  */
  if (warn_parentheses
      && (complain & tf_warning)
      && !processing_template_decl
      && !error_operand_p (arg1)
      && !error_operand_p (arg2)
      && (code != LSHIFT_EXPR
	  || !CLASS_TYPE_P (TREE_TYPE (arg1))))
    warn_about_parentheses (loc, code, arg1_code, orig_arg1,
			    arg2_code, orig_arg2);

  if (processing_template_decl && expr != error_mark_node)
    {
      if (overload != NULL_TREE)
	return (build_min_non_dep_op_overload
		(code, expr, overload, orig_arg1, orig_arg2));

      return build_min_non_dep (code, expr, orig_arg1, orig_arg2);
    }

  return expr;
}

/* Build and return an ARRAY_REF expression.  */

tree
build_x_array_ref (location_t loc, tree arg1, tree arg2,
		   tsubst_flags_t complain)
{
  tree orig_arg1 = arg1;
  tree orig_arg2 = arg2;
  tree expr;
  tree overload = NULL_TREE;

  if (processing_template_decl)
    {
      if (type_dependent_expression_p (arg1)
	  || type_dependent_expression_p (arg2))
	return build_min_nt_loc (loc, ARRAY_REF, arg1, arg2,
				 NULL_TREE, NULL_TREE);
    }

  expr = build_new_op (loc, ARRAY_REF, LOOKUP_NORMAL, arg1, arg2,
		       NULL_TREE, NULL_TREE, &overload, complain);

  if (processing_template_decl && expr != error_mark_node)
    {
      if (overload != NULL_TREE)
	return (build_min_non_dep_op_overload
		(ARRAY_REF, expr, overload, orig_arg1, orig_arg2));

      return build_min_non_dep (ARRAY_REF, expr, orig_arg1, orig_arg2,
				NULL_TREE, NULL_TREE);
    }
  return expr;
}

/* Build an OpenMP array section reference, creating an exact type for the
   resulting expression based on the element type and bounds if possible.  If
   we have variable bounds, create an incomplete array type for the result
   instead.  */

tree
build_omp_array_section (location_t loc, tree array_expr, tree index,
			 tree length)
{
  if (TREE_CODE (array_expr) == TYPE_DECL
      || type_dependent_expression_p (array_expr))
    return build3_loc (loc, OMP_ARRAY_SECTION, NULL_TREE, array_expr, index,
		       length);

  tree type = TREE_TYPE (array_expr);
  gcc_assert (type);
  type = non_reference (type);

  tree sectype, eltype = TREE_TYPE (type);

  /* It's not an array or pointer type.  Just reuse the type of the
     original expression as the type of the array section (an error will be
     raised anyway, later).  */
  if (eltype == NULL_TREE)
    sectype = TREE_TYPE (array_expr);
  else
    {
      tree idxtype = NULL_TREE;

      /* If we know the integer bounds, create an index type with exact
	 low/high (or zero/length) bounds.  Otherwise, create an incomplete
	 array type.  (This mostly only affects diagnostics.)  */
      if (index != NULL_TREE
	  && length != NULL_TREE
	  && TREE_CODE (index) == INTEGER_CST
	  && TREE_CODE (length) == INTEGER_CST)
	{
	  tree low = fold_convert (sizetype, index);
	  tree high = fold_convert (sizetype, length);
	  high = size_binop (PLUS_EXPR, low, high);
	  high = size_binop (MINUS_EXPR, high, size_one_node);
	  idxtype = build_range_type (sizetype, low, high);
	}
      else if ((index == NULL_TREE || integer_zerop (index))
	       && length != NULL_TREE
	       && TREE_CODE (length) == INTEGER_CST)
	idxtype = build_index_type (length);

      sectype = build_array_type (eltype, idxtype);
    }

  return build3_loc (loc, OMP_ARRAY_SECTION, sectype, array_expr, index,
		     length);
}

/* Return whether OP is an expression of enum type cast to integer
   type.  In C++ even unsigned enum types are cast to signed integer
   types.  We do not want to issue warnings about comparisons between
   signed and unsigned types when one of the types is an enum type.
   Those warnings are always false positives in practice.  */

static bool
enum_cast_to_int (tree op)
{
  if (CONVERT_EXPR_P (op)
      && TREE_TYPE (op) == integer_type_node
      && TREE_CODE (TREE_TYPE (TREE_OPERAND (op, 0))) == ENUMERAL_TYPE
      && TYPE_UNSIGNED (TREE_TYPE (TREE_OPERAND (op, 0))))
    return true;

  /* The cast may have been pushed into a COND_EXPR.  */
  if (TREE_CODE (op) == COND_EXPR)
    return (enum_cast_to_int (TREE_OPERAND (op, 1))
	    || enum_cast_to_int (TREE_OPERAND (op, 2)));

  return false;
}

/* For the c-common bits.  */
tree
build_binary_op (location_t location, enum tree_code code, tree op0, tree op1,
		 bool /*convert_p*/)
{
  return cp_build_binary_op (location, code, op0, op1, tf_warning_or_error);
}

/* Build a vector comparison of ARG0 and ARG1 using CODE opcode
   into a value of TYPE type.  Comparison is done via VEC_COND_EXPR.  */

static tree
build_vec_cmp (tree_code code, tree type,
	       tree arg0, tree arg1)
{
  tree zero_vec = build_zero_cst (type);
  tree minus_one_vec = build_minus_one_cst (type);
  tree cmp_type = truth_type_for (TREE_TYPE (arg0));
  tree cmp = build2 (code, cmp_type, arg0, arg1);
  return build3 (VEC_COND_EXPR, type, cmp, minus_one_vec, zero_vec);
}

/* Possibly warn about an address never being NULL.  */

static void
warn_for_null_address (location_t location, tree op, tsubst_flags_t complain)
{
  /* Prevent warnings issued for macro expansion.  */
  if (!warn_address
      || (complain & tf_warning) == 0
      || c_inhibit_evaluation_warnings != 0
      || from_macro_expansion_at (location)
      || warning_suppressed_p (op, OPT_Waddress))
    return;

  tree cop = fold_for_warn (op);

  if (TREE_CODE (cop) == NON_LVALUE_EXPR)
    /* Unwrap the expression for C++ 98.  */
    cop = TREE_OPERAND (cop, 0);

  if (TREE_CODE (cop) == PTRMEM_CST)
    {
      /* The address of a nonstatic data member is never null.  */
      warning_at (location, OPT_Waddress,
		  "the address %qE will never be NULL",
		  cop);
      return;
    }

  if (TREE_CODE (cop) == NOP_EXPR)
    {
      /* Allow casts to intptr_t to suppress the warning.  */
      tree type = TREE_TYPE (cop);
      if (TREE_CODE (type) == INTEGER_TYPE)
	return;

      STRIP_NOPS (cop);
    }

  auto_diagnostic_group d;
  bool warned = false;
  if (TREE_CODE (cop) == ADDR_EXPR)
    {
      cop = TREE_OPERAND (cop, 0);

      /* Set to true in the loop below if OP dereferences its operand.
	 In such a case the ultimate target need not be a decl for
	 the null [in]equality test to be necessarily constant.  */
      bool deref = false;

      /* Get the outermost array or object, or member.  */
      while (handled_component_p (cop))
	{
	  if (TREE_CODE (cop) == COMPONENT_REF)
	    {
	      /* Get the member (its address is never null).  */
	      cop = TREE_OPERAND (cop, 1);
	      break;
	    }

	  /* Get the outer array/object to refer to in the warning.  */
	  cop = TREE_OPERAND (cop, 0);
	  deref = true;
	}

      if ((!deref && !decl_with_nonnull_addr_p (cop))
	  || from_macro_expansion_at (location)
	  || warning_suppressed_p (cop, OPT_Waddress))
	return;

      warned = warning_at (location, OPT_Waddress,
			   "the address of %qD will never be NULL", cop);
      op = cop;
    }
  else if (TREE_CODE (cop) == POINTER_PLUS_EXPR)
    {
      /* Adding zero to the null pointer is well-defined in C++.  When
	 the offset is unknown (i.e., not a constant) warn anyway since
	 it's less likely that the pointer operand is null than not.  */
      tree off = TREE_OPERAND (cop, 1);
      if (!integer_zerop (off)
	  && !warning_suppressed_p (cop, OPT_Waddress))
	{
	  tree base = TREE_OPERAND (cop, 0);
	  STRIP_NOPS (base);
	  if (TYPE_REF_P (TREE_TYPE (base)))
	    warning_at (location, OPT_Waddress, "the compiler can assume that "
			"the address of %qE will never be NULL", base);
	  else
	    warning_at (location, OPT_Waddress, "comparing the result of "
			"pointer addition %qE and NULL", cop);
	}
      return;
    }
  else if (CONVERT_EXPR_P (op)
	   && TYPE_REF_P (TREE_TYPE (TREE_OPERAND (op, 0))))
    {
      STRIP_NOPS (op);

      if (TREE_CODE (op) == COMPONENT_REF)
	op = TREE_OPERAND (op, 1);

      if (DECL_P (op))
	warned = warning_at (location, OPT_Waddress,
			     "the compiler can assume that the address of "
			     "%qD will never be NULL", op);
    }

  if (warned && DECL_P (op))
    inform (DECL_SOURCE_LOCATION (op), "%qD declared here", op);
}

/* Warn about [expr.arith.conv]/2: If one operand is of enumeration type and
   the other operand is of a different enumeration type or a floating-point
   type, this behavior is deprecated ([depr.arith.conv.enum]).  CODE is the
   code of the binary operation, TYPE0 and TYPE1 are the types of the operands,
   and LOC is the location for the whole binary expression.
   For C++26 this is ill-formed rather than deprecated.
   Return true for SFINAE errors.
   TODO: Consider combining this with -Wenum-compare in build_new_op_1.  */

static bool
do_warn_enum_conversions (location_t loc, enum tree_code code, tree type0,
			  tree type1, tsubst_flags_t complain)
{
  if (TREE_CODE (type0) == ENUMERAL_TYPE
      && TREE_CODE (type1) == ENUMERAL_TYPE
      && TYPE_MAIN_VARIANT (type0) != TYPE_MAIN_VARIANT (type1))
    {
      if (cxx_dialect >= cxx26)
	{
	  if ((complain & tf_warning_or_error) == 0)
	    return true;
	}
      else if ((complain & tf_warning) == 0)
	return false;
      /* In C++20, -Wdeprecated-enum-enum-conversion is on by default.
	 Otherwise, warn if -Wenum-conversion is on.  */
      enum opt_code opt;
      if (warn_deprecated_enum_enum_conv)
	opt = OPT_Wdeprecated_enum_enum_conversion;
      else if (warn_enum_conversion)
	opt = OPT_Wenum_conversion;
      else
	return false;

      switch (code)
	{
	case GT_EXPR:
	case LT_EXPR:
	case GE_EXPR:
	case LE_EXPR:
	case EQ_EXPR:
	case NE_EXPR:
	  /* Comparisons are handled by -Wenum-compare.  */
	  return false;
	case SPACESHIP_EXPR:
	  /* This is invalid, don't warn.  */
	  return false;
	case BIT_AND_EXPR:
	case BIT_IOR_EXPR:
	case BIT_XOR_EXPR:
	  if (cxx_dialect >= cxx26)
	    pedwarn (loc, opt, "bitwise operation between different "
		     "enumeration types %qT and %qT", type0, type1);
	  else
	    warning_at (loc, opt, "bitwise operation between different "
			"enumeration types %qT and %qT is deprecated",
			type0, type1);
	  return false;
	default:
	  if (cxx_dialect >= cxx26)
	    pedwarn (loc, opt, "arithmetic between different enumeration "
		     "types %qT and %qT", type0, type1);
	  else
	    warning_at (loc, opt, "arithmetic between different enumeration "
			"types %qT and %qT is deprecated", type0, type1);
	  return false;
	}
    }
  else if ((TREE_CODE (type0) == ENUMERAL_TYPE
	    && SCALAR_FLOAT_TYPE_P (type1))
	   || (SCALAR_FLOAT_TYPE_P (type0)
	       && TREE_CODE (type1) == ENUMERAL_TYPE))
    {
      if (cxx_dialect >= cxx26)
	{
	  if ((complain & tf_warning_or_error) == 0)
	    return true;
	}
      else if ((complain & tf_warning) == 0)
	return false;
      const bool enum_first_p = TREE_CODE (type0) == ENUMERAL_TYPE;
      /* In C++20, -Wdeprecated-enum-float-conversion is on by default.
	 Otherwise, warn if -Wenum-conversion is on.  */
      enum opt_code opt;
      if (warn_deprecated_enum_float_conv)
	opt = OPT_Wdeprecated_enum_float_conversion;
      else if (warn_enum_conversion)
	opt = OPT_Wenum_conversion;
      else
	return false;

      switch (code)
	{
	case GT_EXPR:
	case LT_EXPR:
	case GE_EXPR:
	case LE_EXPR:
	case EQ_EXPR:
	case NE_EXPR:
	  if (enum_first_p && cxx_dialect >= cxx26)
	    pedwarn (loc, opt, "comparison of enumeration type %qT with "
		     "floating-point type %qT", type0, type1);
	  else if (cxx_dialect >= cxx26)
	    pedwarn (loc, opt, "comparison of floating-point type %qT "
		      "with enumeration type %qT", type0, type1);
	  else if (enum_first_p)
	    warning_at (loc, opt, "comparison of enumeration type %qT with "
			"floating-point type %qT is deprecated",
			type0, type1);
	  else
	    warning_at (loc, opt, "comparison of floating-point type %qT "
			"with enumeration type %qT is deprecated",
			type0, type1);
	  return false;
	case SPACESHIP_EXPR:
	  /* This is invalid, don't warn.  */
	  return false;
	default:
	  if (enum_first_p && cxx_dialect >= cxx26)
	    pedwarn (loc, opt, "arithmetic between enumeration type %qT "
		     "and floating-point type %qT", type0, type1);
	  else if (cxx_dialect >= cxx26)
	    pedwarn (loc, opt, "arithmetic between floating-point type %qT "
		     "and enumeration type %qT", type0, type1);
	  else if (enum_first_p)
	    warning_at (loc, opt, "arithmetic between enumeration type %qT "
			"and floating-point type %qT is deprecated",
			type0, type1);
	  else
	    warning_at (loc, opt, "arithmetic between floating-point type %qT "
			"and enumeration type %qT is deprecated",
			type0, type1);
	  return false;
	}
    }
  return false;
}

/* Build a binary-operation expression without default conversions.
   CODE is the kind of expression to build.
   LOCATION is the location_t of the operator in the source code.
   This function differs from `build' in several ways:
   the data type of the result is computed and recorded in it,
   warnings are generated if arg data types are invalid,
   special handling for addition and subtraction of pointers is known,
   and some optimization is done (operations on narrow ints
   are done in the narrower type when that gives the same result).
   Constant folding is also done before the result is returned.

   Note that the operands will never have enumeral types
   because either they have just had the default conversions performed
   or they have both just been converted to some other type in which
   the arithmetic is to be done.

   C++: must do special pointer arithmetic when implementing
   multiple inheritance, and deal with pointer to member functions.  */

tree
cp_build_binary_op (const op_location_t &location,
		    enum tree_code code, tree orig_op0, tree orig_op1,
		    tsubst_flags_t complain)
{
  tree op0, op1;
  enum tree_code code0, code1;
  tree type0, type1, orig_type0, orig_type1;
  const char *invalid_op_diag;

  /* Expression code to give to the expression when it is built.
     Normally this is CODE, which is what the caller asked for,
     but in some special cases we change it.  */
  enum tree_code resultcode = code;

  /* Data type in which the computation is to be performed.
     In the simplest cases this is the common type of the arguments.  */
  tree result_type = NULL_TREE;

  /* When the computation is in excess precision, the type of the
     final EXCESS_PRECISION_EXPR.  */
  tree semantic_result_type = NULL;

  /* Nonzero means operands have already been type-converted
     in whatever way is necessary.
     Zero means they need to be converted to RESULT_TYPE.  */
  int converted = 0;

  /* Nonzero means create the expression with this type, rather than
     RESULT_TYPE.  */
  tree build_type = 0;

  /* Nonzero means after finally constructing the expression
     convert it to this type.  */
  tree final_type = 0;

  tree result;

  /* Nonzero if this is an operation like MIN or MAX which can
     safely be computed in short if both args are promoted shorts.
     Also implies COMMON.
     -1 indicates a bitwise operation; this makes a difference
     in the exact conditions for when it is safe to do the operation
     in a narrower mode.  */
  int shorten = 0;

  /* Nonzero if this is a comparison operation;
     if both args are promoted shorts, compare the original shorts.
     Also implies COMMON.  */
  int short_compare = 0;

  /* Nonzero if this is a right-shift operation, which can be computed on the
     original short and then promoted if the operand is a promoted short.  */
  int short_shift = 0;

  /* Nonzero means set RESULT_TYPE to the common type of the args.  */
  int common = 0;

  /* True if both operands have arithmetic type.  */
  bool arithmetic_types_p;

  /* Remember whether we're doing / or %.  */
  bool doing_div_or_mod = false;

  /* Remember whether we're doing << or >>.  */
  bool doing_shift = false;

  /* Tree holding instrumentation expression.  */
  tree instrument_expr = NULL_TREE;

  /* True means this is an arithmetic operation that may need excess
     precision.  */
  bool may_need_excess_precision;

  /* Apply default conversions.  */
  op0 = resolve_nondeduced_context (orig_op0, complain);
  op1 = resolve_nondeduced_context (orig_op1, complain);

  if (code == TRUTH_AND_EXPR || code == TRUTH_ANDIF_EXPR
      || code == TRUTH_OR_EXPR || code == TRUTH_ORIF_EXPR
      || code == TRUTH_XOR_EXPR)
    {
      if (!really_overloaded_fn (op0) && !VOID_TYPE_P (TREE_TYPE (op0)))
	op0 = decay_conversion (op0, complain);
      if (!really_overloaded_fn (op1) && !VOID_TYPE_P (TREE_TYPE (op1)))
	op1 = decay_conversion (op1, complain);
    }
  else
    {
      if (!really_overloaded_fn (op0) && !VOID_TYPE_P (TREE_TYPE (op0)))
	op0 = cp_default_conversion (op0, complain);
      if (!really_overloaded_fn (op1) && !VOID_TYPE_P (TREE_TYPE (op1)))
	op1 = cp_default_conversion (op1, complain);
    }

  /* Strip NON_LVALUE_EXPRs, etc., since we aren't using as an lvalue.  */
  STRIP_TYPE_NOPS (op0);
  STRIP_TYPE_NOPS (op1);

  /* DTRT if one side is an overloaded function, but complain about it.  */
  if (type_unknown_p (op0))
    {
      tree t = instantiate_type (TREE_TYPE (op1), op0, tf_none);
      if (t != error_mark_node)
	{
	  if (complain & tf_error)
	    permerror (location,
		       "assuming cast to type %qT from overloaded function",
		       TREE_TYPE (t));
	  op0 = t;
	}
    }
  if (type_unknown_p (op1))
    {
      tree t = instantiate_type (TREE_TYPE (op0), op1, tf_none);
      if (t != error_mark_node)
	{
	  if (complain & tf_error)
	    permerror (location,
		       "assuming cast to type %qT from overloaded function",
		       TREE_TYPE (t));
	  op1 = t;
	}
    }

  orig_type0 = type0 = TREE_TYPE (op0);
  orig_type1 = type1 = TREE_TYPE (op1);
  tree non_ep_op0 = op0;
  tree non_ep_op1 = op1;

  /* The expression codes of the data types of the arguments tell us
     whether the arguments are integers, floating, pointers, etc.  */
  code0 = TREE_CODE (type0);
  code1 = TREE_CODE (type1);

  /* If an error was already reported for one of the arguments,
     avoid reporting another error.  */
  if (code0 == ERROR_MARK || code1 == ERROR_MARK)
    return error_mark_node;

  if ((invalid_op_diag
       = targetm.invalid_binary_op (code, type0, type1)))
    {
      if (complain & tf_error)
	{
	  if (code0 == REAL_TYPE
	      && code1 == REAL_TYPE
	      && (extended_float_type_p (type0)
		  || extended_float_type_p (type1))
	      && cp_compare_floating_point_conversion_ranks (type0,
							     type1) == 3)
	    {
	      rich_location richloc (line_table, location);
	      binary_op_error (&richloc, code, type0, type1);
	    }
	  else
	    error (invalid_op_diag);
	}
      return error_mark_node;
    }

  switch (code)
    {
    case PLUS_EXPR:
    case MINUS_EXPR:
    case MULT_EXPR:
    case TRUNC_DIV_EXPR:
    case CEIL_DIV_EXPR:
    case FLOOR_DIV_EXPR:
    case ROUND_DIV_EXPR:
    case EXACT_DIV_EXPR:
      may_need_excess_precision = true;
      break;
    case EQ_EXPR:
    case NE_EXPR:
    case LE_EXPR:
    case GE_EXPR:
    case LT_EXPR:
    case GT_EXPR:
    case SPACESHIP_EXPR:
      /* Excess precision for implicit conversions of integers to
	 floating point.  */
      may_need_excess_precision = (ANY_INTEGRAL_TYPE_P (type0)
				   || ANY_INTEGRAL_TYPE_P (type1));
      break;
    default:
      may_need_excess_precision = false;
      break;
    }
  if (TREE_CODE (op0) == EXCESS_PRECISION_EXPR)
    {
      op0 = TREE_OPERAND (op0, 0);
      type0 = TREE_TYPE (op0);
    }
  else if (may_need_excess_precision
	   && (code0 == REAL_TYPE || code0 == COMPLEX_TYPE))
    if (tree eptype = excess_precision_type (type0))
      {
	type0 = eptype;
	op0 = convert (eptype, op0);
      }
  if (TREE_CODE (op1) == EXCESS_PRECISION_EXPR)
    {
      op1 = TREE_OPERAND (op1, 0);
      type1 = TREE_TYPE (op1);
    }
  else if (may_need_excess_precision
	   && (code1 == REAL_TYPE || code1 == COMPLEX_TYPE))
    if (tree eptype = excess_precision_type (type1))
      {
	type1 = eptype;
	op1 = convert (eptype, op1);
      }

  /* Issue warnings about peculiar, but valid, uses of NULL.  */
  if ((null_node_p (orig_op0) || null_node_p (orig_op1))
      /* It's reasonable to use pointer values as operands of &&
	 and ||, so NULL is no exception.  */
      && code != TRUTH_ANDIF_EXPR && code != TRUTH_ORIF_EXPR
      && ( /* Both are NULL (or 0) and the operation was not a
	      comparison or a pointer subtraction.  */
	  (null_ptr_cst_p (orig_op0) && null_ptr_cst_p (orig_op1)
	   && code != EQ_EXPR && code != NE_EXPR && code != MINUS_EXPR)
	  /* Or if one of OP0 or OP1 is neither a pointer nor NULL.  */
	  || (!null_ptr_cst_p (orig_op0)
	      && !TYPE_PTR_OR_PTRMEM_P (type0))
	  || (!null_ptr_cst_p (orig_op1)
	      && !TYPE_PTR_OR_PTRMEM_P (type1)))
      && (complain & tf_warning))
    {
      location_t loc =
	expansion_point_location_if_in_system_header (input_location);

      warning_at (loc, OPT_Wpointer_arith, "NULL used in arithmetic");
    }

  /* In case when one of the operands of the binary operation is
     a vector and another is a scalar -- convert scalar to vector.  */
  if ((gnu_vector_type_p (type0) && code1 != VECTOR_TYPE)
      || (gnu_vector_type_p (type1) && code0 != VECTOR_TYPE))
    {
      enum stv_conv convert_flag
	= scalar_to_vector (location, code, non_ep_op0, non_ep_op1,
			    complain & tf_error);

      switch (convert_flag)
        {
          case stv_error:
            return error_mark_node;
          case stv_firstarg:
            {
              op0 = convert (TREE_TYPE (type1), op0);
	      op0 = cp_save_expr (op0);
              op0 = build_vector_from_val (type1, op0);
	      orig_type0 = type0 = TREE_TYPE (op0);
              code0 = TREE_CODE (type0);
              converted = 1;
              break;
            }
          case stv_secondarg:
            {
              op1 = convert (TREE_TYPE (type0), op1);
	      op1 = cp_save_expr (op1);
              op1 = build_vector_from_val (type0, op1);
	      orig_type1 = type1 = TREE_TYPE (op1);
              code1 = TREE_CODE (type1);
              converted = 1;
              break;
            }
          default:
            break;
        }
    }

  switch (code)
    {
    case MINUS_EXPR:
      /* Subtraction of two similar pointers.
	 We must subtract them as integers, then divide by object size.  */
      if (code0 == POINTER_TYPE && code1 == POINTER_TYPE
	  && same_type_ignoring_top_level_qualifiers_p (TREE_TYPE (type0),
							TREE_TYPE (type1)))
	{
	  result = pointer_diff (location, op0, op1,
				 common_pointer_type (type0, type1), complain,
				 &instrument_expr);
	  if (instrument_expr != NULL)
	    result = build2 (COMPOUND_EXPR, TREE_TYPE (result),
			     instrument_expr, result);

	  return result;
	}
      /* In all other cases except pointer - int, the usual arithmetic
	 rules apply.  */
      else if (!(code0 == POINTER_TYPE && code1 == INTEGER_TYPE))
	{
	  common = 1;
	  break;
	}
      /* The pointer - int case is just like pointer + int; fall
	 through.  */
      gcc_fallthrough ();
    case PLUS_EXPR:
      if ((code0 == POINTER_TYPE || code1 == POINTER_TYPE)
	  && (code0 == INTEGER_TYPE || code1 == INTEGER_TYPE))
	{
	  tree ptr_operand;
	  tree int_operand;
	  ptr_operand = ((code0 == POINTER_TYPE) ? op0 : op1);
	  int_operand = ((code0 == INTEGER_TYPE) ? op0 : op1);
	  if (processing_template_decl)
	    {
	      result_type = TREE_TYPE (ptr_operand);
	      break;
	    }
	  return cp_pointer_int_sum (location, code,
				     ptr_operand,
				     int_operand,
				     complain);
	}
      common = 1;
      break;

    case MULT_EXPR:
      common = 1;
      break;

    case TRUNC_DIV_EXPR:
    case CEIL_DIV_EXPR:
    case FLOOR_DIV_EXPR:
    case ROUND_DIV_EXPR:
    case EXACT_DIV_EXPR:
      if (TREE_CODE (op0) == SIZEOF_EXPR && TREE_CODE (op1) == SIZEOF_EXPR)
	{
	  tree type0 = TREE_OPERAND (op0, 0);
	  tree type1 = TREE_OPERAND (op1, 0);
	  tree first_arg = tree_strip_any_location_wrapper (type0);
	  if (!TYPE_P (type0))
	    type0 = TREE_TYPE (type0);
	  if (!TYPE_P (type1))
	    type1 = TREE_TYPE (type1);
	  if (type0
	      && type1
	      && INDIRECT_TYPE_P (type0)
	      && same_type_p (TREE_TYPE (type0), type1))
	    {
	      if (!(TREE_CODE (first_arg) == PARM_DECL
		    && DECL_ARRAY_PARAMETER_P (first_arg)
		    && warn_sizeof_array_argument)
		  && (complain & tf_warning))
		{
		  auto_diagnostic_group d;
		  if (warning_at (location, OPT_Wsizeof_pointer_div,
				  "division %<sizeof (%T) / sizeof (%T)%> does "
				  "not compute the number of array elements",
				  type0, type1))
		    if (DECL_P (first_arg))
		      inform (DECL_SOURCE_LOCATION (first_arg),
			      "first %<sizeof%> operand was declared here");
		}
	    }
	  else if (!dependent_type_p (type0)
		   && !dependent_type_p (type1)
		   && TREE_CODE (type0) == ARRAY_TYPE
		   && !char_type_p (TYPE_MAIN_VARIANT (TREE_TYPE (type0)))
		   /* Set by finish_parenthesized_expr.  */
		   && !warning_suppressed_p (op1, OPT_Wsizeof_array_div)
		   && (complain & tf_warning))
	    maybe_warn_sizeof_array_div (location, first_arg, type0,
					 op1, non_reference (type1));
	}

      if ((code0 == INTEGER_TYPE || code0 == REAL_TYPE
	   || code0 == COMPLEX_TYPE || code0 == VECTOR_TYPE)
	  && (code1 == INTEGER_TYPE || code1 == REAL_TYPE
	      || code1 == COMPLEX_TYPE || code1 == VECTOR_TYPE))
	{
	  enum tree_code tcode0 = code0, tcode1 = code1;
	  doing_div_or_mod = true;
	  warn_for_div_by_zero (location, fold_for_warn (op1));

	  if (tcode0 == COMPLEX_TYPE || tcode0 == VECTOR_TYPE)
	    tcode0 = TREE_CODE (TREE_TYPE (TREE_TYPE (op0)));
	  if (tcode1 == COMPLEX_TYPE || tcode1 == VECTOR_TYPE)
	    tcode1 = TREE_CODE (TREE_TYPE (TREE_TYPE (op1)));

	  if (!(tcode0 == INTEGER_TYPE && tcode1 == INTEGER_TYPE))
	    resultcode = RDIV_EXPR;
	  else
	    {
	      /* When dividing two signed integers, we have to promote to int.
		 unless we divide by a constant != -1.  Note that default
		 conversion will have been performed on the operands at this
		 point, so we have to dig out the original type to find out if
		 it was unsigned.  */
	      tree stripped_op1 = tree_strip_any_location_wrapper (op1);
	      shorten = may_shorten_divmod (op0, stripped_op1);
	    }

	  common = 1;
	}
      break;

    case BIT_AND_EXPR:
    case BIT_IOR_EXPR:
    case BIT_XOR_EXPR:
      if ((code0 == INTEGER_TYPE && code1 == INTEGER_TYPE)
	  || (code0 == VECTOR_TYPE && code1 == VECTOR_TYPE
	      && !VECTOR_FLOAT_TYPE_P (type0)
	      && !VECTOR_FLOAT_TYPE_P (type1)))
	shorten = -1;
      break;

    case TRUNC_MOD_EXPR:
    case FLOOR_MOD_EXPR:
      doing_div_or_mod = true;
      warn_for_div_by_zero (location, fold_for_warn (op1));

      if (code0 == VECTOR_TYPE && code1 == VECTOR_TYPE
	  && TREE_CODE (TREE_TYPE (type0)) == INTEGER_TYPE
	  && TREE_CODE (TREE_TYPE (type1)) == INTEGER_TYPE)
	common = 1;
      else if (code0 == INTEGER_TYPE && code1 == INTEGER_TYPE)
	{
	  /* Although it would be tempting to shorten always here, that loses
	     on some targets, since the modulo instruction is undefined if the
	     quotient can't be represented in the computation mode.  We shorten
	     only if unsigned or if dividing by something we know != -1.  */
	  tree stripped_op1 = tree_strip_any_location_wrapper (op1);
	  shorten = may_shorten_divmod (op0, stripped_op1);
	  common = 1;
	}
      break;

    case TRUTH_ANDIF_EXPR:
    case TRUTH_ORIF_EXPR:
    case TRUTH_AND_EXPR:
    case TRUTH_OR_EXPR:
      if (!VECTOR_TYPE_P (type0) && gnu_vector_type_p (type1))
	{
	  if (!COMPARISON_CLASS_P (op1))
	    op1 = cp_build_binary_op (EXPR_LOCATION (op1), NE_EXPR, op1,
				      build_zero_cst (type1), complain);
	  if (code == TRUTH_ANDIF_EXPR)
	    {
	      tree z = build_zero_cst (TREE_TYPE (op1));
	      return build_conditional_expr (location, op0, op1, z, complain);
	    }
	  else if (code == TRUTH_ORIF_EXPR)
	    {
	      tree m1 = build_all_ones_cst (TREE_TYPE (op1));
	      return build_conditional_expr (location, op0, m1, op1, complain);
	    }
	  else
	    gcc_unreachable ();
	}
      if (gnu_vector_type_p (type0)
	  && (!VECTOR_TYPE_P (type1) || gnu_vector_type_p (type1)))
	{
	  if (!COMPARISON_CLASS_P (op0))
	    op0 = cp_build_binary_op (EXPR_LOCATION (op0), NE_EXPR, op0,
				      build_zero_cst (type0), complain);
	  if (!VECTOR_TYPE_P (type1))
	    {
	      tree m1 = build_all_ones_cst (TREE_TYPE (op0));
	      tree z = build_zero_cst (TREE_TYPE (op0));
	      op1 = build_conditional_expr (location, op1, m1, z, complain);
	    }
	  else if (!COMPARISON_CLASS_P (op1))
	    op1 = cp_build_binary_op (EXPR_LOCATION (op1), NE_EXPR, op1,
				      build_zero_cst (type1), complain);

	  if (code == TRUTH_ANDIF_EXPR)
	    code = BIT_AND_EXPR;
	  else if (code == TRUTH_ORIF_EXPR)
	    code = BIT_IOR_EXPR;
	  else
	    gcc_unreachable ();

	  return cp_build_binary_op (location, code, op0, op1, complain);
	}

      result_type = boolean_type_node;
      break;

      /* Shift operations: result has same type as first operand;
	 always convert second operand to int.
	 Also set SHORT_SHIFT if shifting rightward.  */

    case RSHIFT_EXPR:
      if (gnu_vector_type_p (type0)
	  && code1 == INTEGER_TYPE
	  && TREE_CODE (TREE_TYPE (type0)) == INTEGER_TYPE)
        {
          result_type = type0;
          converted = 1;
        }
      else if (gnu_vector_type_p (type0)
	       && gnu_vector_type_p (type1)
	       && TREE_CODE (TREE_TYPE (type0)) == INTEGER_TYPE
	       && TREE_CODE (TREE_TYPE (type1)) == INTEGER_TYPE
	       && known_eq (TYPE_VECTOR_SUBPARTS (type0),
			    TYPE_VECTOR_SUBPARTS (type1)))
	{
	  result_type = type0;
	  converted = 1;
	}
      else if (code0 == INTEGER_TYPE && code1 == INTEGER_TYPE)
	{
	  tree const_op1 = fold_for_warn (op1);
	  if (TREE_CODE (const_op1) != INTEGER_CST)
	    const_op1 = op1;
	  result_type = type0;
	  doing_shift = true;
	  if (TREE_CODE (const_op1) == INTEGER_CST)
	    {
	      if (tree_int_cst_lt (const_op1, integer_zero_node))
		{
		  if ((complain & tf_warning)
		      && c_inhibit_evaluation_warnings == 0)
		    warning_at (location, OPT_Wshift_count_negative,
				"right shift count is negative");
		}
	      else
		{
		  if (!integer_zerop (const_op1))
		    short_shift = 1;

		  if (compare_tree_int (const_op1, TYPE_PRECISION (type0)) >= 0
		      && (complain & tf_warning)
		      && c_inhibit_evaluation_warnings == 0)
		    warning_at (location, OPT_Wshift_count_overflow,
				"right shift count >= width of type");
		}
	    }
	  /* Avoid converting op1 to result_type later.  */
	  converted = 1;
	}
      break;

    case LSHIFT_EXPR:
      if (gnu_vector_type_p (type0)
	  && code1 == INTEGER_TYPE
          && TREE_CODE (TREE_TYPE (type0)) == INTEGER_TYPE)
        {
          result_type = type0;
          converted = 1;
        }
      else if (gnu_vector_type_p (type0)
	       && gnu_vector_type_p (type1)
	       && TREE_CODE (TREE_TYPE (type0)) == INTEGER_TYPE
	       && TREE_CODE (TREE_TYPE (type1)) == INTEGER_TYPE
	       && known_eq (TYPE_VECTOR_SUBPARTS (type0),
			    TYPE_VECTOR_SUBPARTS (type1)))
	{
	  result_type = type0;
	  converted = 1;
	}
      else if (code0 == INTEGER_TYPE && code1 == INTEGER_TYPE)
	{
	  tree const_op0 = fold_for_warn (op0);
	  if (TREE_CODE (const_op0) != INTEGER_CST)
	    const_op0 = op0;
	  tree const_op1 = fold_for_warn (op1);
	  if (TREE_CODE (const_op1) != INTEGER_CST)
	    const_op1 = op1;
	  result_type = type0;
	  doing_shift = true;
	  if (TREE_CODE (const_op0) == INTEGER_CST
	      && tree_int_cst_sgn (const_op0) < 0
	      && !TYPE_OVERFLOW_WRAPS (type0)
	      && (complain & tf_warning)
	      && c_inhibit_evaluation_warnings == 0)
	    warning_at (location, OPT_Wshift_negative_value,
			"left shift of negative value");
	  if (TREE_CODE (const_op1) == INTEGER_CST)
	    {
	      if (tree_int_cst_lt (const_op1, integer_zero_node))
		{
		  if ((complain & tf_warning)
		      && c_inhibit_evaluation_warnings == 0)
		    warning_at (location, OPT_Wshift_count_negative,
				"left shift count is negative");
		}
	      else if (compare_tree_int (const_op1,
					 TYPE_PRECISION (type0)) >= 0)
		{
		  if ((complain & tf_warning)
		      && c_inhibit_evaluation_warnings == 0)
		    warning_at (location, OPT_Wshift_count_overflow,
				"left shift count >= width of type");
		}
	      else if (TREE_CODE (const_op0) == INTEGER_CST
		       && (complain & tf_warning))
		maybe_warn_shift_overflow (location, const_op0, const_op1);
	    }
	  /* Avoid converting op1 to result_type later.  */
	  converted = 1;
	}
      break;

    case EQ_EXPR:
    case NE_EXPR:
      if (gnu_vector_type_p (type0) && gnu_vector_type_p (type1))
	goto vector_compare;
      if ((complain & tf_warning)
	  && c_inhibit_evaluation_warnings == 0
	  && (FLOAT_TYPE_P (type0) || FLOAT_TYPE_P (type1)))
	warning_at (location, OPT_Wfloat_equal,
		    "comparing floating-point with %<==%> "
		    "or %<!=%> is unsafe");
      {
	tree stripped_orig_op0 = tree_strip_any_location_wrapper (orig_op0);
	tree stripped_orig_op1 = tree_strip_any_location_wrapper (orig_op1);
	if ((complain & tf_warning_or_error)
	    && ((TREE_CODE (stripped_orig_op0) == STRING_CST
		 && !integer_zerop (cp_fully_fold (op1)))
		|| (TREE_CODE (stripped_orig_op1) == STRING_CST
		    && !integer_zerop (cp_fully_fold (op0)))))
	  warning_at (location, OPT_Waddress,
		      "comparison with string literal results in "
		      "unspecified behavior");
	else if (TREE_CODE (TREE_TYPE (orig_op0)) == ARRAY_TYPE
		 && TREE_CODE (TREE_TYPE (orig_op1)) == ARRAY_TYPE)
	  {
	    /* P2865R5 made array comparisons ill-formed in C++26.  */
	    if (complain & tf_warning_or_error)
	      do_warn_array_compare (location, code, stripped_orig_op0,
				     stripped_orig_op1);
	    else if (cxx_dialect >= cxx26)
	      return error_mark_node;
	  }
      }

      build_type = boolean_type_node;
      if ((code0 == INTEGER_TYPE || code0 == REAL_TYPE
	   || code0 == COMPLEX_TYPE || code0 == ENUMERAL_TYPE)
	  && (code1 == INTEGER_TYPE || code1 == REAL_TYPE
	      || code1 == COMPLEX_TYPE || code1 == ENUMERAL_TYPE))
	short_compare = 1;
      else if (((code0 == POINTER_TYPE || TYPE_PTRDATAMEM_P (type0))
		&& null_ptr_cst_p (orig_op1))
	       /* Handle, eg, (void*)0 (c++/43906), and more.  */
	       || (code0 == POINTER_TYPE
		   && TYPE_PTR_P (type1) && integer_zerop (op1)))
	{
	  if (TYPE_PTR_P (type1))
	    result_type = composite_pointer_type (location,
						  type0, type1, op0, op1,
						  CPO_COMPARISON, complain);
	  else
	    result_type = type0;

	  if (char_type_p (TREE_TYPE (orig_op1)))
	    {
	      auto_diagnostic_group d;
	      if (warning_at (location, OPT_Wpointer_compare,
			      "comparison between pointer and zero character "
			      "constant"))
		inform (location,
			"did you mean to dereference the pointer?");
	    }
	  warn_for_null_address (location, op0, complain);
	}
      else if (((code1 == POINTER_TYPE || TYPE_PTRDATAMEM_P (type1))
		&& null_ptr_cst_p (orig_op0))
	       /* Handle, eg, (void*)0 (c++/43906), and more.  */
	       || (code1 == POINTER_TYPE
		   && TYPE_PTR_P (type0) && integer_zerop (op0)))
	{
	  if (TYPE_PTR_P (type0))
	    result_type = composite_pointer_type (location,
						  type0, type1, op0, op1,
						  CPO_COMPARISON, complain);
	  else
	    result_type = type1;

	  if (char_type_p (TREE_TYPE (orig_op0)))
	    {
	      auto_diagnostic_group d;
	      if (warning_at (location, OPT_Wpointer_compare,
			     "comparison between pointer and zero character "
			     "constant"))
		inform (location,
			"did you mean to dereference the pointer?");
	    }
	  warn_for_null_address (location, op1, complain);
	}
      else if ((code0 == POINTER_TYPE && code1 == POINTER_TYPE)
	       || (TYPE_PTRDATAMEM_P (type0) && TYPE_PTRDATAMEM_P (type1)))
	result_type = composite_pointer_type (location,
					      type0, type1, op0, op1,
					      CPO_COMPARISON, complain);
      else if (null_ptr_cst_p (orig_op0) && null_ptr_cst_p (orig_op1))
	/* One of the operands must be of nullptr_t type.  */
        result_type = TREE_TYPE (nullptr_node);
      else if (code0 == POINTER_TYPE && code1 == INTEGER_TYPE)
	{
	  result_type = type0;
	  if (complain & tf_error)
	    permerror (location, "ISO C++ forbids comparison between "
		       "pointer and integer");
          else
            return error_mark_node;
	}
      else if (code0 == INTEGER_TYPE && code1 == POINTER_TYPE)
	{
	  result_type = type1;
	  if (complain & tf_error)
	    permerror (location, "ISO C++ forbids comparison between "
		       "pointer and integer");
          else
            return error_mark_node;
	}
      else if (TYPE_PTRMEMFUNC_P (type0) && null_ptr_cst_p (orig_op1))
	{
	  if (TARGET_PTRMEMFUNC_VBIT_LOCATION
	      == ptrmemfunc_vbit_in_delta)
	    {
	      tree pfn0, delta0, e1, e2;

	      if (TREE_SIDE_EFFECTS (op0))
		op0 = cp_save_expr (op0);

	      pfn0 = pfn_from_ptrmemfunc (op0);
	      delta0 = delta_from_ptrmemfunc (op0);
	      {
		/* If we will warn below about a null-address compare
		   involving the orig_op0 ptrmemfunc, we'd likely also
		   warn about the pfn0's null-address compare, and
		   that would be redundant, so suppress it.  */
		warning_sentinel ws (warn_address);
		e1 = cp_build_binary_op (location,
					 EQ_EXPR,
					 pfn0,
					 build_zero_cst (TREE_TYPE (pfn0)),
					 complain);
	      }
	      e2 = cp_build_binary_op (location,
				       BIT_AND_EXPR,
				       delta0,
				       integer_one_node,
				       complain);

	      if (complain & tf_warning)
		maybe_warn_zero_as_null_pointer_constant (op1, input_location);

	      e2 = cp_build_binary_op (location,
				       EQ_EXPR, e2, integer_zero_node,
				       complain);
	      op0 = cp_build_binary_op (location,
					TRUTH_ANDIF_EXPR, e1, e2,
					complain);
	      op1 = cp_convert (TREE_TYPE (op0), integer_one_node, complain);
	    }
     	  else
	    {
	      op0 = build_ptrmemfunc_access_expr (op0, pfn_identifier);
	      op1 = cp_convert (TREE_TYPE (op0), op1, complain);
	    }
	  result_type = TREE_TYPE (op0);

	  warn_for_null_address (location, orig_op0, complain);
	}
      else if (TYPE_PTRMEMFUNC_P (type1) && null_ptr_cst_p (orig_op0))
	return cp_build_binary_op (location, code, op1, op0, complain);
      else if (TYPE_PTRMEMFUNC_P (type0) && TYPE_PTRMEMFUNC_P (type1))
	{
	  tree type;
	  /* E will be the final comparison.  */
	  tree e;
	  /* E1 and E2 are for scratch.  */
	  tree e1;
	  tree e2;
	  tree pfn0;
	  tree pfn1;
	  tree delta0;
	  tree delta1;

	  type = composite_pointer_type (location, type0, type1, op0, op1,
					 CPO_COMPARISON, complain);

	  if (!same_type_p (TREE_TYPE (op0), type))
	    op0 = cp_convert_and_check (type, op0, complain);
	  if (!same_type_p (TREE_TYPE (op1), type))
	    op1 = cp_convert_and_check (type, op1, complain);

	  if (op0 == error_mark_node || op1 == error_mark_node)
	    return error_mark_node;

	  if (TREE_SIDE_EFFECTS (op0))
	    op0 = cp_save_expr (op0);
	  if (TREE_SIDE_EFFECTS (op1))
	    op1 = cp_save_expr (op1);

	  pfn0 = pfn_from_ptrmemfunc (op0);
	  pfn0 = cp_fully_fold (pfn0);
	  /* Avoid -Waddress warnings (c++/64877).  */
	  if (TREE_CODE (pfn0) == ADDR_EXPR)
	    suppress_warning (pfn0, OPT_Waddress);
	  pfn1 = pfn_from_ptrmemfunc (op1);
	  pfn1 = cp_fully_fold (pfn1);
	  delta0 = delta_from_ptrmemfunc (op0);
	  delta1 = delta_from_ptrmemfunc (op1);
	  if (TARGET_PTRMEMFUNC_VBIT_LOCATION
	      == ptrmemfunc_vbit_in_delta)
	    {
	      /* We generate:

		 (op0.pfn == op1.pfn
		  && ((op0.delta == op1.delta)
     		       || (!op0.pfn && op0.delta & 1 == 0
			   && op1.delta & 1 == 0))

	         The reason for the `!op0.pfn' bit is that a NULL
	         pointer-to-member is any member with a zero PFN and
	         LSB of the DELTA field is 0.  */

	      e1 = cp_build_binary_op (location, BIT_AND_EXPR,
				       delta0,
				       integer_one_node,
				       complain);
	      e1 = cp_build_binary_op (location,
				       EQ_EXPR, e1, integer_zero_node,
				       complain);
	      e2 = cp_build_binary_op (location, BIT_AND_EXPR,
				       delta1,
				       integer_one_node,
				       complain);
	      e2 = cp_build_binary_op (location,
				       EQ_EXPR, e2, integer_zero_node,
				       complain);
	      e1 = cp_build_binary_op (location,
				       TRUTH_ANDIF_EXPR, e2, e1,
				       complain);
	      e2 = cp_build_binary_op (location, EQ_EXPR,
				       pfn0,
				       build_zero_cst (TREE_TYPE (pfn0)),
				       complain);
	      e2 = cp_build_binary_op (location,
				       TRUTH_ANDIF_EXPR, e2, e1, complain);
	      e1 = cp_build_binary_op (location,
				       EQ_EXPR, delta0, delta1, complain);
	      e1 = cp_build_binary_op (location,
				       TRUTH_ORIF_EXPR, e1, e2, complain);
	    }
	  else
	    {
	      /* We generate:

	         (op0.pfn == op1.pfn
	         && (!op0.pfn || op0.delta == op1.delta))

	         The reason for the `!op0.pfn' bit is that a NULL
	         pointer-to-member is any member with a zero PFN; the
	         DELTA field is unspecified.  */

    	      e1 = cp_build_binary_op (location,
				       EQ_EXPR, delta0, delta1, complain);
	      e2 = cp_build_binary_op (location,
				       EQ_EXPR,
		      		       pfn0,
			   	       build_zero_cst (TREE_TYPE (pfn0)),
				       complain);
	      e1 = cp_build_binary_op (location,
				       TRUTH_ORIF_EXPR, e1, e2, complain);
	    }
	  e2 = build2 (EQ_EXPR, boolean_type_node, pfn0, pfn1);
	  e = cp_build_binary_op (location,
				  TRUTH_ANDIF_EXPR, e2, e1, complain);
	  if (code == EQ_EXPR)
	    return e;
	  return cp_build_binary_op (location,
				     EQ_EXPR, e, integer_zero_node, complain);
	}
      else
	{
	  gcc_assert (!TYPE_PTRMEMFUNC_P (type0)
		      || !same_type_p (TYPE_PTRMEMFUNC_FN_TYPE (type0),
				       type1));
	  gcc_assert (!TYPE_PTRMEMFUNC_P (type1)
		      || !same_type_p (TYPE_PTRMEMFUNC_FN_TYPE (type1),
				       type0));
	}

      break;

    case MAX_EXPR:
    case MIN_EXPR:
      if ((code0 == INTEGER_TYPE || code0 == REAL_TYPE)
	   && (code1 == INTEGER_TYPE || code1 == REAL_TYPE))
	shorten = 1;
      else if (code0 == POINTER_TYPE && code1 == POINTER_TYPE)
	result_type = composite_pointer_type (location,
					      type0, type1, op0, op1,
					      CPO_COMPARISON, complain);
      break;

    case LE_EXPR:
    case GE_EXPR:
    case LT_EXPR:
    case GT_EXPR:
    case SPACESHIP_EXPR:
      if (TREE_CODE (orig_op0) == STRING_CST
	  || TREE_CODE (orig_op1) == STRING_CST)
	{
	  if (complain & tf_warning)
	    warning_at (location, OPT_Waddress,
			"comparison with string literal results "
			"in unspecified behavior");
	}
      else if (TREE_CODE (TREE_TYPE (orig_op0)) == ARRAY_TYPE
	       && TREE_CODE (TREE_TYPE (orig_op1)) == ARRAY_TYPE
	       && code != SPACESHIP_EXPR)
	{
	  if (complain & tf_warning_or_error)
	    do_warn_array_compare (location, code,
				   tree_strip_any_location_wrapper (orig_op0),
				   tree_strip_any_location_wrapper (orig_op1));
	  else if (cxx_dialect >= cxx26)
	    return error_mark_node;
	}

      if (gnu_vector_type_p (type0) && gnu_vector_type_p (type1))
	{
	vector_compare:
	  tree intt;
	  if (!same_type_ignoring_top_level_qualifiers_p (TREE_TYPE (type0),
							  TREE_TYPE (type1))
	      && !vector_types_compatible_elements_p (type0, type1))
	    {
	      if (complain & tf_error)
		{
		  auto_diagnostic_group d;
		  error_at (location, "comparing vectors with different "
				      "element types");
		  inform (location, "operand types are %qT and %qT",
			  type0, type1);
		}
	      return error_mark_node;
	    }

	  if (maybe_ne (TYPE_VECTOR_SUBPARTS (type0),
			TYPE_VECTOR_SUBPARTS (type1)))
	    {
	      if (complain & tf_error)
		{
		  auto_diagnostic_group d;
		  error_at (location, "comparing vectors with different "
				      "number of elements");
		  inform (location, "operand types are %qT and %qT",
			  type0, type1);
		}
	      return error_mark_node;
	    }

	  /* It's not precisely specified how the usual arithmetic
	     conversions apply to the vector types.  Here, we use
	     the unsigned type if one of the operands is signed and
	     the other one is unsigned.  */
	  if (TYPE_UNSIGNED (type0) != TYPE_UNSIGNED (type1))
	    {
	      if (!TYPE_UNSIGNED (type0))
		op0 = build1 (VIEW_CONVERT_EXPR, type1, op0);
	      else
		op1 = build1 (VIEW_CONVERT_EXPR, type0, op1);
	      warning_at (location, OPT_Wsign_compare, "comparison between "
			  "types %qT and %qT", type0, type1);
	    }

	  if (resultcode == SPACESHIP_EXPR)
	    {
	      if (complain & tf_error)
		sorry_at (location, "three-way comparison of vectors");
	      return error_mark_node;
	    }

	  /* Always construct signed integer vector type.  */
	  intt = c_common_type_for_size
	    (GET_MODE_BITSIZE (SCALAR_TYPE_MODE (TREE_TYPE (type0))), 0);
	  if (!intt)
	    {
	      if (complain & tf_error)
		error_at (location, "could not find an integer type "
			  "of the same size as %qT", TREE_TYPE (type0));
	      return error_mark_node;
	    }
	  result_type = build_opaque_vector_type (intt,
						  TYPE_VECTOR_SUBPARTS (type0));
	  return build_vec_cmp (resultcode, result_type, op0, op1);
	}
      build_type = boolean_type_node;
      if ((code0 == INTEGER_TYPE || code0 == REAL_TYPE
	   || code0 == ENUMERAL_TYPE)
	   && (code1 == INTEGER_TYPE || code1 == REAL_TYPE
	       || code1 == ENUMERAL_TYPE))
	short_compare = 1;
      else if (code0 == POINTER_TYPE && code1 == POINTER_TYPE)
	result_type = composite_pointer_type (location,
					      type0, type1, op0, op1,
					      CPO_COMPARISON, complain);
      else if ((code0 == POINTER_TYPE && null_ptr_cst_p (orig_op1))
	       || (code1 == POINTER_TYPE && null_ptr_cst_p (orig_op0))
	       || (null_ptr_cst_p (orig_op0) && null_ptr_cst_p (orig_op1)))
	{
	  /* Core Issue 1512 made this ill-formed.  */
	  if (complain & tf_error)
	    error_at (location, "ordered comparison of pointer with "
		      "integer zero (%qT and %qT)", type0, type1);
	  return error_mark_node;
	}
      else if (code0 == POINTER_TYPE && code1 == INTEGER_TYPE)
	{
	  result_type = type0;
	  if (complain & tf_error)
	    permerror (location, "ISO C++ forbids comparison between "
		       "pointer and integer");
	  else
            return error_mark_node;
	}
      else if (code0 == INTEGER_TYPE && code1 == POINTER_TYPE)
	{
	  result_type = type1;
	  if (complain & tf_error)
	    permerror (location, "ISO C++ forbids comparison between "
		       "pointer and integer");
	  else
            return error_mark_node;
	}

      if ((code0 == POINTER_TYPE || code1 == POINTER_TYPE)
	  && !processing_template_decl
	  && sanitize_flags_p (SANITIZE_POINTER_COMPARE))
	{
	  op0 = cp_save_expr (op0);
	  op1 = cp_save_expr (op1);

	  tree tt = builtin_decl_explicit (BUILT_IN_ASAN_POINTER_COMPARE);
	  instrument_expr = build_call_expr_loc (location, tt, 2, op0, op1);
	}

      break;

    case UNORDERED_EXPR:
    case ORDERED_EXPR:
    case UNLT_EXPR:
    case UNLE_EXPR:
    case UNGT_EXPR:
    case UNGE_EXPR:
    case UNEQ_EXPR:
      build_type = integer_type_node;
      if (code0 != REAL_TYPE || code1 != REAL_TYPE)
	{
	  if (complain & tf_error)
	    error ("unordered comparison on non-floating-point argument");
	  return error_mark_node;
	}
      common = 1;
      break;

    default:
      break;
    }

  if (((code0 == INTEGER_TYPE || code0 == REAL_TYPE || code0 == COMPLEX_TYPE
	|| code0 == ENUMERAL_TYPE)
       && (code1 == INTEGER_TYPE || code1 == REAL_TYPE
	   || code1 == COMPLEX_TYPE || code1 == ENUMERAL_TYPE)))
    arithmetic_types_p = 1;
  else
    {
      arithmetic_types_p = 0;
      /* Vector arithmetic is only allowed when both sides are vectors.  */
      if (gnu_vector_type_p (type0) && gnu_vector_type_p (type1))
	{
	  if (!tree_int_cst_equal (TYPE_SIZE (type0), TYPE_SIZE (type1))
	      || !vector_types_compatible_elements_p (type0, type1))
	    {
	      if (complain & tf_error)
		{
		  /* "location" already embeds the locations of the
		     operands, so we don't need to add them separately
		     to richloc.  */
		  rich_location richloc (line_table, location);
		  binary_op_error (&richloc, code, type0, type1);
		}
	      return error_mark_node;
	    }
	  arithmetic_types_p = 1;
	}
    }
  /* Determine the RESULT_TYPE, if it is not already known.  */
  if (!result_type
      && arithmetic_types_p
      && (shorten || common || short_compare))
    {
      result_type = cp_common_type (type0, type1);
      if (result_type == error_mark_node)
	{
	  tree t1 = type0;
	  tree t2 = type1;
	  if (TREE_CODE (t1) == COMPLEX_TYPE)
	    t1 = TREE_TYPE (t1);
	  if (TREE_CODE (t2) == COMPLEX_TYPE)
	    t2 = TREE_TYPE (t2);
	  gcc_checking_assert (TREE_CODE (t1) == REAL_TYPE
			       && TREE_CODE (t2) == REAL_TYPE
			       && (extended_float_type_p (t1)
				   || extended_float_type_p (t2))
			       && cp_compare_floating_point_conversion_ranks
				    (t1, t2) == 3);
	  if (complain & tf_error)
	    {
	      rich_location richloc (line_table, location);
	      binary_op_error (&richloc, code, type0, type1);
	    }
	  return error_mark_node;
	}
      if (complain & tf_warning)
	do_warn_double_promotion (result_type, type0, type1,
				  "implicit conversion from %qH to %qI "
				  "to match other operand of binary "
				  "expression", location);
      if (do_warn_enum_conversions (location, code, TREE_TYPE (orig_op0),
				    TREE_TYPE (orig_op1), complain))
	return error_mark_node;
    }
  if (may_need_excess_precision
      && (orig_type0 != type0 || orig_type1 != type1)
      && build_type == NULL_TREE
      && result_type)
    {
      gcc_assert (common);
      semantic_result_type = cp_common_type (orig_type0, orig_type1);
      if (semantic_result_type == error_mark_node)
	{
	  tree t1 = orig_type0;
	  tree t2 = orig_type1;
	  if (TREE_CODE (t1) == COMPLEX_TYPE)
	    t1 = TREE_TYPE (t1);
	  if (TREE_CODE (t2) == COMPLEX_TYPE)
	    t2 = TREE_TYPE (t2);
	  gcc_checking_assert (TREE_CODE (t1) == REAL_TYPE
			       && TREE_CODE (t2) == REAL_TYPE
			       && (extended_float_type_p (t1)
				   || extended_float_type_p (t2))
			       && cp_compare_floating_point_conversion_ranks
				    (t1, t2) == 3);
	  if (complain & tf_error)
	    {
	      rich_location richloc (line_table, location);
	      binary_op_error (&richloc, code, type0, type1);
	    }
	  return error_mark_node;
	}
    }

  if (code == SPACESHIP_EXPR)
    {
      iloc_sentinel s (location);

      tree orig_type0 = TREE_TYPE (orig_op0);
      tree_code orig_code0 = TREE_CODE (orig_type0);
      tree orig_type1 = TREE_TYPE (orig_op1);
      tree_code orig_code1 = TREE_CODE (orig_type1);
      if (!result_type || result_type == error_mark_node)
	/* Nope.  */
	result_type = NULL_TREE;
      else if ((orig_code0 == BOOLEAN_TYPE) != (orig_code1 == BOOLEAN_TYPE))
	/* "If one of the operands is of type bool and the other is not, the
	   program is ill-formed."  */
	result_type = NULL_TREE;
      else if (code0 == POINTER_TYPE && orig_code0 != POINTER_TYPE
	       && code1 == POINTER_TYPE && orig_code1 != POINTER_TYPE)
	/* We only do array/function-to-pointer conversion if "at least one of
	   the operands is of pointer type".  */
	result_type = NULL_TREE;
      else if (TYPE_PTRFN_P (result_type) || NULLPTR_TYPE_P (result_type))
	/* <=> no longer supports equality relations.  */
	result_type = NULL_TREE;
      else if (orig_code0 == ENUMERAL_TYPE && orig_code1 == ENUMERAL_TYPE
	       && !(same_type_ignoring_top_level_qualifiers_p
		    (orig_type0, orig_type1)))
	/* "If both operands have arithmetic types, or one operand has integral
	   type and the other operand has unscoped enumeration type, the usual
	   arithmetic conversions are applied to the operands."  So we don't do
	   arithmetic conversions if the operands both have enumeral type.  */
	result_type = NULL_TREE;
      else if ((orig_code0 == ENUMERAL_TYPE && orig_code1 == REAL_TYPE)
	       || (orig_code0 == REAL_TYPE && orig_code1 == ENUMERAL_TYPE))
	/* [depr.arith.conv.enum]: Three-way comparisons between such operands
	   [where one is of enumeration type and the other is of a different
	   enumeration type or a floating-point type] are ill-formed.  */
	result_type = NULL_TREE;

      if (result_type)
	{
	  build_type = spaceship_type (result_type, complain);
	  if (build_type == error_mark_node)
	    return error_mark_node;
	}

      if (result_type && arithmetic_types_p)
	{
	  /* If a narrowing conversion is required, other than from an integral
	     type to a floating point type, the program is ill-formed.  */
	  bool ok = true;
	  if (TREE_CODE (result_type) == REAL_TYPE
	      && CP_INTEGRAL_TYPE_P (orig_type0))
	    /* OK */;
	  else if (!check_narrowing (result_type, orig_op0, complain))
	    ok = false;
	  if (TREE_CODE (result_type) == REAL_TYPE
	      && CP_INTEGRAL_TYPE_P (orig_type1))
	    /* OK */;
	  else if (!check_narrowing (result_type, orig_op1, complain))
	    ok = false;
	  if (!ok && !(complain & tf_error))
	    return error_mark_node;
	}
    }

  if (!result_type)
    {
      if (complain & tf_error)
	{
	  binary_op_rich_location richloc (location,
					   orig_op0, orig_op1, true);
	  error_at (&richloc,
		    "invalid operands of types %qT and %qT to binary %qO",
		    TREE_TYPE (orig_op0), TREE_TYPE (orig_op1), code);
	}
      return error_mark_node;
    }

  /* If we're in a template, the only thing we need to know is the
     RESULT_TYPE.  */
  if (processing_template_decl)
    {
      /* Since the middle-end checks the type when doing a build2, we
	 need to build the tree in pieces.  This built tree will never
	 get out of the front-end as we replace it when instantiating
	 the template.  */
      tree tmp = build2 (resultcode,
			 build_type ? build_type : result_type,
			 NULL_TREE, op1);
      TREE_OPERAND (tmp, 0) = op0;
      if (semantic_result_type)
	tmp = build1 (EXCESS_PRECISION_EXPR, semantic_result_type, tmp);
      return tmp;
    }

  /* Remember the original type; RESULT_TYPE might be changed later on
     by shorten_binary_op.  */
  tree orig_type = result_type;

  if (arithmetic_types_p)
    {
      bool first_complex = (code0 == COMPLEX_TYPE);
      bool second_complex = (code1 == COMPLEX_TYPE);
      int none_complex = (!first_complex && !second_complex);

      /* Adapted from patch for c/24581.  */
      if (first_complex != second_complex
	  && (code == PLUS_EXPR
	      || code == MINUS_EXPR
	      || code == MULT_EXPR
	      || (code == TRUNC_DIV_EXPR && first_complex))
	  && TREE_CODE (TREE_TYPE (result_type)) == REAL_TYPE
	  && flag_signed_zeros)
	{
	  /* An operation on mixed real/complex operands must be
	     handled specially, but the language-independent code can
	     more easily optimize the plain complex arithmetic if
	     -fno-signed-zeros.  */
	  tree real_type = TREE_TYPE (result_type);
	  tree real, imag;
	  if (first_complex)
	    {
	      if (TREE_TYPE (op0) != result_type)
		op0 = cp_convert_and_check (result_type, op0, complain);
	      if (TREE_TYPE (op1) != real_type)
		op1 = cp_convert_and_check (real_type, op1, complain);
	    }
	  else
	    {
	      if (TREE_TYPE (op0) != real_type)
		op0 = cp_convert_and_check (real_type, op0, complain);
	      if (TREE_TYPE (op1) != result_type)
		op1 = cp_convert_and_check (result_type, op1, complain);
	    }
	  if (TREE_CODE (op0) == ERROR_MARK || TREE_CODE (op1) == ERROR_MARK)
	    return error_mark_node;
	  if (first_complex)
	    {
	      op0 = cp_save_expr (op0);
	      real = cp_build_unary_op (REALPART_EXPR, op0, true, complain);
	      imag = cp_build_unary_op (IMAGPART_EXPR, op0, true, complain);
	      switch (code)
		{
		case MULT_EXPR:
		case TRUNC_DIV_EXPR:
		  op1 = cp_save_expr (op1);
		  imag = build2 (resultcode, real_type, imag, op1);
		  /* Fall through.  */
		case PLUS_EXPR:
		case MINUS_EXPR:
		  real = build2 (resultcode, real_type, real, op1);
		  break;
		default:
		  gcc_unreachable();
		}
	    }
	  else
	    {
	      op1 = cp_save_expr (op1);
	      real = cp_build_unary_op (REALPART_EXPR, op1, true, complain);
	      imag = cp_build_unary_op (IMAGPART_EXPR, op1, true, complain);
	      switch (code)
		{
		case MULT_EXPR:
		  op0 = cp_save_expr (op0);
		  imag = build2 (resultcode, real_type, op0, imag);
		  /* Fall through.  */
		case PLUS_EXPR:
		  real = build2 (resultcode, real_type, op0, real);
		  break;
		case MINUS_EXPR:
		  real = build2 (resultcode, real_type, op0, real);
		  imag = build1 (NEGATE_EXPR, real_type, imag);
		  break;
		default:
		  gcc_unreachable();
		}
	    }
	  result = build2 (COMPLEX_EXPR, result_type, real, imag);
	  if (semantic_result_type)
	    result = build1 (EXCESS_PRECISION_EXPR, semantic_result_type,
			     result);
	  return result;
	}

      /* For certain operations (which identify themselves by shorten != 0)
	 if both args were extended from the same smaller type,
	 do the arithmetic in that type and then extend.

	 shorten !=0 and !=1 indicates a bitwise operation.
	 For them, this optimization is safe only if
	 both args are zero-extended or both are sign-extended.
	 Otherwise, we might change the result.
	 E.g., (short)-1 | (unsigned short)-1 is (int)-1
	 but calculated in (unsigned short) it would be (unsigned short)-1.  */

      if (shorten && none_complex)
	{
	  final_type = result_type;
	  result_type = shorten_binary_op (result_type, op0, op1,
					   shorten == -1);
	}

      /* Shifts can be shortened if shifting right.  */

      if (short_shift)
	{
	  int unsigned_arg;
	  tree arg0 = get_narrower (op0, &unsigned_arg);
	  /* We're not really warning here but when we set short_shift we
	     used fold_for_warn to fold the operand.  */
	  tree const_op1 = fold_for_warn (op1);

	  final_type = result_type;

	  if (arg0 == op0 && final_type == TREE_TYPE (op0))
	    unsigned_arg = TYPE_UNSIGNED (TREE_TYPE (op0));

	  if (TYPE_PRECISION (TREE_TYPE (arg0)) < TYPE_PRECISION (result_type)
	      && tree_int_cst_sgn (const_op1) > 0
	      /* We can shorten only if the shift count is less than the
		 number of bits in the smaller type size.  */
	      && compare_tree_int (const_op1,
				   TYPE_PRECISION (TREE_TYPE (arg0))) < 0
	      /* We cannot drop an unsigned shift after sign-extension.  */
	      && (!TYPE_UNSIGNED (final_type) || unsigned_arg))
	    {
	      /* Do an unsigned shift if the operand was zero-extended.  */
	      result_type
		= c_common_signed_or_unsigned_type (unsigned_arg,
						    TREE_TYPE (arg0));
	      /* Convert value-to-be-shifted to that type.  */
	      if (TREE_TYPE (op0) != result_type)
		op0 = convert (result_type, op0);
	      converted = 1;
	    }
	}

      /* Comparison operations are shortened too but differently.
	 They identify themselves by setting short_compare = 1.  */

      if (short_compare)
	{
	  /* We call shorten_compare only for diagnostics.  */
	  tree xop0 = fold_simple (op0);
	  tree xop1 = fold_simple (op1);
	  tree xresult_type = result_type;
	  enum tree_code xresultcode = resultcode;
	  shorten_compare (location, &xop0, &xop1, &xresult_type,
			   &xresultcode);
	}

      if ((short_compare || code == MIN_EXPR || code == MAX_EXPR)
	  && warn_sign_compare
	  /* Do not warn until the template is instantiated; we cannot
	     bound the ranges of the arguments until that point.  */
	  && !processing_template_decl
          && (complain & tf_warning)
	  && c_inhibit_evaluation_warnings == 0
	  /* Even unsigned enum types promote to signed int.  We don't
	     want to issue -Wsign-compare warnings for this case.  */
	  && !enum_cast_to_int (orig_op0)
	  && !enum_cast_to_int (orig_op1))
	{
	  warn_for_sign_compare (location, orig_op0, orig_op1, op0, op1,
				 result_type, resultcode);
	}
    }

  /* If CONVERTED is zero, both args will be converted to type RESULT_TYPE.
     Then the expression will be built.
     It will be given type FINAL_TYPE if that is nonzero;
     otherwise, it will be given type RESULT_TYPE.  */
  if (! converted)
    {
      warning_sentinel w (warn_sign_conversion, short_compare);
      if (!same_type_p (TREE_TYPE (op0), result_type))
	op0 = cp_convert_and_check (result_type, op0, complain);
      if (!same_type_p (TREE_TYPE (op1), result_type))
	op1 = cp_convert_and_check (result_type, op1, complain);

      if (op0 == error_mark_node || op1 == error_mark_node)
	return error_mark_node;
    }

  if (build_type == NULL_TREE)
    build_type = result_type;

  if (doing_shift
      && flag_strong_eval_order == 2
      && TREE_SIDE_EFFECTS (op1)
      && !processing_template_decl)
    {
      /* In C++17, in both op0 << op1 and op0 >> op1 op0 is sequenced before
	 op1, so if op1 has side-effects, use SAVE_EXPR around op0.  */
      op0 = cp_save_expr (op0);
      instrument_expr = op0;
    }

  if (sanitize_flags_p ((SANITIZE_SHIFT
			 | SANITIZE_DIVIDE
			 | SANITIZE_FLOAT_DIVIDE
			 | SANITIZE_SI_OVERFLOW))
      && current_function_decl != NULL_TREE
      && !processing_template_decl
      && (doing_div_or_mod || doing_shift))
    {
      /* OP0 and/or OP1 might have side-effects.  */
      op0 = cp_save_expr (op0);
      op1 = cp_save_expr (op1);
      op0 = fold_non_dependent_expr (op0, complain);
      op1 = fold_non_dependent_expr (op1, complain);
      tree instrument_expr1 = NULL_TREE;
      if (doing_div_or_mod
	  && sanitize_flags_p (SANITIZE_DIVIDE
			       | SANITIZE_FLOAT_DIVIDE
			       | SANITIZE_SI_OVERFLOW))
	{
	  /* For diagnostics we want to use the promoted types without
	     shorten_binary_op.  So convert the arguments to the
	     original result_type.  */
	  tree cop0 = op0;
	  tree cop1 = op1;
	  if (TREE_TYPE (cop0) != orig_type)
	    cop0 = cp_convert (orig_type, op0, complain);
	  if (TREE_TYPE (cop1) != orig_type)
	    cop1 = cp_convert (orig_type, op1, complain);
	  instrument_expr1 = ubsan_instrument_division (location, cop0, cop1);
	}
      else if (doing_shift && sanitize_flags_p (SANITIZE_SHIFT))
	instrument_expr1 = ubsan_instrument_shift (location, code, op0, op1);
      if (instrument_expr != NULL)
	instrument_expr = add_stmt_to_compound (instrument_expr,
						instrument_expr1);
      else
	instrument_expr = instrument_expr1;
    }

  result = build2_loc (location, resultcode, build_type, op0, op1);
  if (final_type != 0)
    result = cp_convert (final_type, result, complain);

  if (instrument_expr != NULL)
    result = build2 (COMPOUND_EXPR, TREE_TYPE (result),
		     instrument_expr, result);

  if (resultcode == SPACESHIP_EXPR && !processing_template_decl)
    result = get_target_expr (result, complain);

  if (semantic_result_type)
    result = build1 (EXCESS_PRECISION_EXPR, semantic_result_type, result);

  if (!c_inhibit_evaluation_warnings)
    {
      if (!processing_template_decl)
	{
	  op0 = cp_fully_fold (op0);
	  /* Only consider the second argument if the first isn't overflowed.  */
	  if (!CONSTANT_CLASS_P (op0) || TREE_OVERFLOW_P (op0))
	    return result;
	  op1 = cp_fully_fold (op1);
	  if (!CONSTANT_CLASS_P (op1) || TREE_OVERFLOW_P (op1))
	    return result;
	}
      else if (!CONSTANT_CLASS_P (op0) || !CONSTANT_CLASS_P (op1)
	       || TREE_OVERFLOW_P (op0) || TREE_OVERFLOW_P (op1))
	return result;

      tree result_ovl = fold_build2 (resultcode, build_type, op0, op1);
      if (TREE_OVERFLOW_P (result_ovl))
	overflow_warning (location, result_ovl);
    }

  return result;
}

/* Build a VEC_PERM_EXPR.
   This is a simple wrapper for c_build_vec_perm_expr.  */
tree
build_x_vec_perm_expr (location_t loc,
			tree arg0, tree arg1, tree arg2,
			tsubst_flags_t complain)
{
  tree orig_arg0 = arg0;
  tree orig_arg1 = arg1;
  tree orig_arg2 = arg2;
  if (processing_template_decl)
    {
      if (type_dependent_expression_p (arg0)
	  || type_dependent_expression_p (arg1)
	  || type_dependent_expression_p (arg2))
	return build_min_nt_loc (loc, VEC_PERM_EXPR, arg0, arg1, arg2);
    }
  tree exp = c_build_vec_perm_expr (loc, arg0, arg1, arg2, complain & tf_error);
  if (processing_template_decl && exp != error_mark_node)
    return build_min_non_dep (VEC_PERM_EXPR, exp, orig_arg0,
			      orig_arg1, orig_arg2);
  return exp;
}

/* Build a VEC_PERM_EXPR.
   This is a simple wrapper for c_build_shufflevector.  */
tree
build_x_shufflevector (location_t loc, vec<tree, va_gc> *args,
		       tsubst_flags_t complain)
{
  tree arg0 = (*args)[0];
  tree arg1 = (*args)[1];
  if (processing_template_decl)
    {
      for (unsigned i = 0; i < args->length (); ++i)
	if (i <= 1
	    ? type_dependent_expression_p ((*args)[i])
	    : instantiation_dependent_expression_p ((*args)[i]))
	  {
	    tree exp = build_min_nt_call_vec (NULL, args);
	    CALL_EXPR_IFN (exp) = IFN_SHUFFLEVECTOR;
	    return exp;
	  }
    }
  auto_vec<tree, 16> mask;
  for (unsigned i = 2; i < args->length (); ++i)
    {
      tree idx = fold_non_dependent_expr ((*args)[i], complain);
      mask.safe_push (idx);
    }
  tree exp = c_build_shufflevector (loc, arg0, arg1, mask, complain & tf_error);
  if (processing_template_decl && exp != error_mark_node)
    {
      exp = build_min_non_dep_call_vec (exp, NULL, args);
      CALL_EXPR_IFN (exp) = IFN_SHUFFLEVECTOR;
    }
  return exp;
}

/* Return a tree for the sum or difference (RESULTCODE says which)
   of pointer PTROP and integer INTOP.  */

static tree
cp_pointer_int_sum (location_t loc, enum tree_code resultcode, tree ptrop,
		    tree intop, tsubst_flags_t complain)
{
  tree res_type = TREE_TYPE (ptrop);

  /* pointer_int_sum() uses size_in_bytes() on the TREE_TYPE(res_type)
     in certain circumstance (when it's valid to do so).  So we need
     to make sure it's complete.  We don't need to check here, if we
     can actually complete it at all, as those checks will be done in
     pointer_int_sum() anyway.  */
  complete_type (TREE_TYPE (res_type));

  return pointer_int_sum (loc, resultcode, ptrop,
			  intop, complain & tf_warning_or_error);
}

/* Return a tree for the difference of pointers OP0 and OP1.
   The resulting tree has type int.  If POINTER_SUBTRACT sanitization is
   enabled, assign to INSTRUMENT_EXPR call to libsanitizer.  */

static tree
pointer_diff (location_t loc, tree op0, tree op1, tree ptrtype,
	      tsubst_flags_t complain, tree *instrument_expr)
{
  tree result, inttype;
  tree restype = ptrdiff_type_node;
  tree target_type = TREE_TYPE (ptrtype);

  if (!complete_type_or_maybe_complain (target_type, NULL_TREE, complain))
    return error_mark_node;

  if (VOID_TYPE_P (target_type))
    {
      if (complain & tf_error)
	permerror (loc, "ISO C++ forbids using pointer of "
		   "type %<void *%> in subtraction");
      else
	return error_mark_node;
    }
  if (TREE_CODE (target_type) == FUNCTION_TYPE)
    {
      if (complain & tf_error)
	permerror (loc, "ISO C++ forbids using pointer to "
		   "a function in subtraction");
      else
	return error_mark_node;
    }
  if (TREE_CODE (target_type) == METHOD_TYPE)
    {
      if (complain & tf_error)
	permerror (loc, "ISO C++ forbids using pointer to "
		   "a method in subtraction");
      else
	return error_mark_node;
    }
  else if (!verify_type_context (loc, TCTX_POINTER_ARITH,
				 TREE_TYPE (TREE_TYPE (op0)),
				 !(complain & tf_error))
	   || !verify_type_context (loc, TCTX_POINTER_ARITH,
				    TREE_TYPE (TREE_TYPE (op1)),
				    !(complain & tf_error)))
    return error_mark_node;

  /* Determine integer type result of the subtraction.  This will usually
     be the same as the result type (ptrdiff_t), but may need to be a wider
     type if pointers for the address space are wider than ptrdiff_t.  */
  if (TYPE_PRECISION (restype) < TYPE_PRECISION (TREE_TYPE (op0)))
    inttype = c_common_type_for_size (TYPE_PRECISION (TREE_TYPE (op0)), 0);
  else
    inttype = restype;

  if (!processing_template_decl
      && sanitize_flags_p (SANITIZE_POINTER_SUBTRACT))
    {
      op0 = save_expr (op0);
      op1 = save_expr (op1);

      tree tt = builtin_decl_explicit (BUILT_IN_ASAN_POINTER_SUBTRACT);
      *instrument_expr = build_call_expr_loc (loc, tt, 2, op0, op1);
    }

  /* First do the subtraction, then build the divide operator
     and only convert at the very end.
     Do not do default conversions in case restype is a short type.  */

  /* POINTER_DIFF_EXPR requires a signed integer type of the same size as
     pointers.  If some platform cannot provide that, or has a larger
     ptrdiff_type to support differences larger than half the address
     space, cast the pointers to some larger integer type and do the
     computations in that type.  */
  if (TYPE_PRECISION (inttype) > TYPE_PRECISION (TREE_TYPE (op0)))
    op0 = cp_build_binary_op (loc,
			      MINUS_EXPR,
			      cp_convert (inttype, op0, complain),
			      cp_convert (inttype, op1, complain),
			      complain);
  else
    op0 = build2_loc (loc, POINTER_DIFF_EXPR, inttype, op0, op1);

  /* This generates an error if op1 is a pointer to an incomplete type.  */
  if (!COMPLETE_TYPE_P (TREE_TYPE (TREE_TYPE (op1))))
    {
      if (complain & tf_error)
	error_at (loc, "invalid use of a pointer to an incomplete type in "
		  "pointer arithmetic");
      else
	return error_mark_node;
    }

  if (pointer_to_zero_sized_aggr_p (TREE_TYPE (op1)))
    {
      if (complain & tf_error)
	error_at (loc, "arithmetic on pointer to an empty aggregate");
      else
	return error_mark_node;
    }

  op1 = (TYPE_PTROB_P (ptrtype)
	 ? size_in_bytes_loc (loc, target_type)
	 : integer_one_node);

  /* Do the division.  */

  result = build2_loc (loc, EXACT_DIV_EXPR, inttype, op0,
		       cp_convert (inttype, op1, complain));
  return cp_convert (restype, result, complain);
}

/* Construct and perhaps optimize a tree representation
   for a unary operation.  CODE, a tree_code, specifies the operation
   and XARG is the operand.  */

tree
build_x_unary_op (location_t loc, enum tree_code code, cp_expr xarg,
		  tree lookups, tsubst_flags_t complain)
{
  tree orig_expr = xarg;
  tree exp;
  int ptrmem = 0;
  tree overload = NULL_TREE;

  if (processing_template_decl)
    {
      if (type_dependent_expression_p (xarg))
	{
	  tree e = build_min_nt_loc (loc, code, xarg.get_value (), NULL_TREE);
	  TREE_TYPE (e) = build_dependent_operator_type (lookups, code, false);
	  return e;
	}
    }

  exp = NULL_TREE;

  /* [expr.unary.op] says:

       The address of an object of incomplete type can be taken.

     (And is just the ordinary address operator, not an overloaded
     "operator &".)  However, if the type is a template
     specialization, we must complete the type at this point so that
     an overloaded "operator &" will be available if required.  */
  if (code == ADDR_EXPR
      && TREE_CODE (xarg) != TEMPLATE_ID_EXPR
      && ((CLASS_TYPE_P (TREE_TYPE (xarg))
	   && !COMPLETE_TYPE_P (complete_type (TREE_TYPE (xarg))))
	  || (TREE_CODE (xarg) == OFFSET_REF)))
    /* Don't look for a function.  */;
  else
    exp = build_new_op (loc, code, LOOKUP_NORMAL, xarg, NULL_TREE,
			NULL_TREE, lookups, &overload, complain);

  if (!exp && code == ADDR_EXPR)
    {
      if (is_overloaded_fn (xarg))
	{
	  tree fn = get_first_fn (xarg);
	  if (DECL_CONSTRUCTOR_P (fn) || DECL_DESTRUCTOR_P (fn))
	    {
	      if (complain & tf_error)
		error_at (loc, DECL_CONSTRUCTOR_P (fn)
			  ? G_("taking address of constructor %qD")
			  : G_("taking address of destructor %qD"),
			  fn);
	      return error_mark_node;
	    }
	}

      /* A pointer to member-function can be formed only by saying
	 &X::mf.  */
      if (!flag_ms_extensions && TREE_CODE (TREE_TYPE (xarg)) == METHOD_TYPE
	  && (TREE_CODE (xarg) != OFFSET_REF || !PTRMEM_OK_P (xarg)))
	{
	  if (TREE_CODE (xarg) != OFFSET_REF
	      || !TYPE_P (TREE_OPERAND (xarg, 0)))
	    {
	      if (complain & tf_error)
		{
		  auto_diagnostic_group d;
		  error_at (loc, "invalid use of %qE to form a "
			    "pointer-to-member-function", xarg.get_value ());
		  if (TREE_CODE (xarg) != OFFSET_REF)
		    inform (loc, "  a qualified-id is required");
		}
	      return error_mark_node;
	    }
	  else
	    {
	      if (complain & tf_error)
		error_at (loc, "parentheses around %qE cannot be used to "
			  "form a pointer-to-member-function",
			  xarg.get_value ());
	      else
		return error_mark_node;
	      PTRMEM_OK_P (xarg) = 1;
	    }
	}

      if (TREE_CODE (xarg) == OFFSET_REF)
	{
	  ptrmem = PTRMEM_OK_P (xarg);

	  if (!ptrmem && !flag_ms_extensions
	      && TREE_CODE (TREE_TYPE (TREE_OPERAND (xarg, 1))) == METHOD_TYPE)
	    {
	      /* A single non-static member, make sure we don't allow a
		 pointer-to-member.  */
	      xarg = build2 (OFFSET_REF, TREE_TYPE (xarg),
			     TREE_OPERAND (xarg, 0),
			     ovl_make (TREE_OPERAND (xarg, 1)));
	      PTRMEM_OK_P (xarg) = ptrmem;
	    }
	}

      exp = cp_build_addr_expr_strict (xarg, complain);

      if (TREE_CODE (exp) == PTRMEM_CST)
	PTRMEM_CST_LOCATION (exp) = loc;
      else
	protected_set_expr_location (exp, loc);
    }

  if (processing_template_decl && exp != error_mark_node)
    {
      if (overload != NULL_TREE)
	return (build_min_non_dep_op_overload
		(code, exp, overload, orig_expr, integer_zero_node));

      exp = build_min_non_dep (code, exp, orig_expr,
			       /*For {PRE,POST}{INC,DEC}REMENT_EXPR*/NULL_TREE);
    }
  if (TREE_CODE (exp) == ADDR_EXPR)
    PTRMEM_OK_P (exp) = ptrmem;
  return exp;
}

/* Construct and perhaps optimize a tree representation
   for __builtin_addressof operation.  ARG specifies the operand.  */

tree
cp_build_addressof (location_t loc, tree arg, tsubst_flags_t complain)
{
  tree orig_expr = arg;

  if (processing_template_decl)
    {
      if (type_dependent_expression_p (arg))
	return build_min_nt_loc (loc, ADDRESSOF_EXPR, arg, NULL_TREE);
    }

  tree exp = cp_build_addr_expr_strict (arg, complain);

  if (processing_template_decl && exp != error_mark_node)
    exp = build_min_non_dep (ADDRESSOF_EXPR, exp, orig_expr, NULL_TREE);
  return exp;
}

/* Like c_common_truthvalue_conversion, but handle pointer-to-member
   constants, where a null value is represented by an INTEGER_CST of
   -1.  */

tree
cp_truthvalue_conversion (tree expr, tsubst_flags_t complain)
{
  tree type = TREE_TYPE (expr);
  location_t loc = cp_expr_loc_or_input_loc (expr);
  if (TYPE_PTR_OR_PTRMEM_P (type)
      /* Avoid ICE on invalid use of non-static member function.  */
      || TREE_CODE (expr) == FUNCTION_DECL)
    return cp_build_binary_op (loc, NE_EXPR, expr, nullptr_node, complain);
  else
    return c_common_truthvalue_conversion (loc, expr);
}

/* Returns EXPR contextually converted to bool.  */

tree
contextual_conv_bool (tree expr, tsubst_flags_t complain)
{
  return perform_implicit_conversion_flags (boolean_type_node, expr,
					    complain, LOOKUP_NORMAL);
}

/* Just like cp_truthvalue_conversion, but we want a CLEANUP_POINT_EXPR.  This
   is a low-level function; most callers should use maybe_convert_cond.  */

tree
condition_conversion (tree expr)
{
  tree t = contextual_conv_bool (expr, tf_warning_or_error);
  if (!processing_template_decl)
    t = fold_build_cleanup_point_expr (boolean_type_node, t);
  return t;
}

/* Returns the address of T.  This function will fold away
   ADDR_EXPR of INDIRECT_REF.  This is only for low-level usage;
   most places should use cp_build_addr_expr instead.  */

tree
build_address (tree t)
{
  if (error_operand_p (t) || !cxx_mark_addressable (t))
    return error_mark_node;
  gcc_checking_assert (TREE_CODE (t) != CONSTRUCTOR
		       || processing_template_decl);
  t = build_fold_addr_expr_loc (EXPR_LOCATION (t), t);
  if (TREE_CODE (t) != ADDR_EXPR)
    t = rvalue (t);
  return t;
}

/* Return a NOP_EXPR converting EXPR to TYPE.  */

tree
build_nop (tree type, tree expr MEM_STAT_DECL)
{
  if (type == error_mark_node || error_operand_p (expr))
    return expr;
  return build1_loc (EXPR_LOCATION (expr), NOP_EXPR, type, expr PASS_MEM_STAT);
}

/* Take the address of ARG, whatever that means under C++ semantics.
   If STRICT_LVALUE is true, require an lvalue; otherwise, allow xvalues
   and class rvalues as well.

   Nothing should call this function directly; instead, callers should use
   cp_build_addr_expr or cp_build_addr_expr_strict.  */

static tree
cp_build_addr_expr_1 (tree arg, bool strict_lvalue, tsubst_flags_t complain)
{
  tree argtype;
  tree val;

  if (!arg || error_operand_p (arg))
    return error_mark_node;

  arg = mark_lvalue_use (arg);
  if (error_operand_p (arg))
    return error_mark_node;

  argtype = lvalue_type (arg);
  location_t loc = cp_expr_loc_or_input_loc (arg);

  gcc_assert (!(identifier_p (arg) && IDENTIFIER_ANY_OP_P (arg)));

  if (TREE_CODE (arg) == COMPONENT_REF && type_unknown_p (arg)
      && !really_overloaded_fn (arg))
    {
      /* They're trying to take the address of a unique non-static
	 member function.  This is ill-formed (except in MS-land),
	 but let's try to DTRT.
	 Note: We only handle unique functions here because we don't
	 want to complain if there's a static overload; non-unique
	 cases will be handled by instantiate_type.  But we need to
	 handle this case here to allow casts on the resulting PMF.
	 We could defer this in non-MS mode, but it's easier to give
	 a useful error here.  */

      /* Inside constant member functions, the `this' pointer
	 contains an extra const qualifier.  TYPE_MAIN_VARIANT
	 is used here to remove this const from the diagnostics
	 and the created OFFSET_REF.  */
      tree base = TYPE_MAIN_VARIANT (TREE_TYPE (TREE_OPERAND (arg, 0)));
      tree fn = get_first_fn (TREE_OPERAND (arg, 1));
      if (!mark_used (fn, complain) && !(complain & tf_error))
	return error_mark_node;
      /* Until microsoft headers are known to incorrectly take the address of
	 unqualified xobj member functions we should not support this
	 extension.
	 See comment in class.cc:resolve_address_of_overloaded_function for
	 the extended reasoning.  */
      if (!flag_ms_extensions || DECL_XOBJ_MEMBER_FUNCTION_P (fn))
	{
	  auto_diagnostic_group d;
	  tree name = DECL_NAME (fn);
	  if (!(complain & tf_error))
	    return error_mark_node;
	  else if (current_class_type
		   && TREE_OPERAND (arg, 0) == current_class_ref)
	    /* An expression like &memfn.  */
	    if (!DECL_XOBJ_MEMBER_FUNCTION_P (fn))
	      permerror (loc,
			 "ISO C++ forbids taking the address of an unqualified"
			 " or parenthesized non-static member function to form"
			 " a pointer to member function.  Say %<&%T::%D%>",
			 base, name);
	    else
	      error_at (loc,
			"ISO C++ forbids taking the address of an unqualified"
			" or parenthesized non-static member function to form"
			" a pointer to explicit object member function");
	  else
	    if (!DECL_XOBJ_MEMBER_FUNCTION_P (fn))
	      permerror (loc,
			 "ISO C++ forbids taking the address of a bound member"
			 " function to form a pointer to member function."
			 "  Say %<&%T::%D%>",
			 base, name);
	    else
	      error_at (loc,
			"ISO C++ forbids taking the address of a bound member"
			" function to form a pointer to explicit object member"
			" function");
	  if (DECL_XOBJ_MEMBER_FUNCTION_P (fn))
	    inform (loc,
		    "a pointer to explicit object member function can only be "
		    "formed with %<&%T::%D%>", base, name);
	}
      arg = build_offset_ref (base, fn, /*address_p=*/true, complain);
    }

  /* Uninstantiated types are all functions.  Taking the
     address of a function is a no-op, so just return the
     argument.  */
  if (type_unknown_p (arg))
    return build1 (ADDR_EXPR, unknown_type_node, arg);

  if (TREE_CODE (arg) == OFFSET_REF)
    /* We want a pointer to member; bypass all the code for actually taking
       the address of something.  */
    goto offset_ref;

  /* Anything not already handled and not a true memory reference
     is an error.  */
  if (!FUNC_OR_METHOD_TYPE_P (argtype))
    {
      cp_lvalue_kind kind = lvalue_kind (arg);
      if (kind == clk_none)
	{
	  if (complain & tf_error)
	    lvalue_error (loc, lv_addressof);
	  return error_mark_node;
	}
      if (strict_lvalue && (kind & (clk_rvalueref|clk_class)))
	{
	  if (!(complain & tf_error))
	    return error_mark_node;
	  /* Make this a permerror because we used to accept it.  */
	  permerror (loc, "taking address of rvalue");
	}
    }

  if (TYPE_REF_P (argtype))
    {
      tree type = build_pointer_type (TREE_TYPE (argtype));
      arg = build1 (CONVERT_EXPR, type, arg);
      return arg;
    }
  else if (pedantic && DECL_MAIN_P (tree_strip_any_location_wrapper (arg)))
    {
      /* ARM $3.4 */
      /* Apparently a lot of autoconf scripts for C++ packages do this,
	 so only complain if -Wpedantic.  */
      if (complain & (flag_pedantic_errors ? tf_error : tf_warning))
	pedwarn (loc, OPT_Wpedantic,
		 "ISO C++ forbids taking address of function %<::main%>");
      else if (flag_pedantic_errors)
	return error_mark_node;
    }

  /* Let &* cancel out to simplify resulting code.  */
  if (INDIRECT_REF_P (arg))
    {
      arg = TREE_OPERAND (arg, 0);
      if (TYPE_REF_P (TREE_TYPE (arg)))
	{
	  tree type = build_pointer_type (TREE_TYPE (TREE_TYPE (arg)));
	  arg = build1 (CONVERT_EXPR, type, arg);
	}
      else
	/* Don't let this be an lvalue.  */
	arg = rvalue (arg);
      return arg;
    }

  /* Handle complex lvalues (when permitted)
     by reduction to simpler cases.  */
  val = unary_complex_lvalue (ADDR_EXPR, arg);
  if (val != 0)
    return val;

  switch (TREE_CODE (arg))
    {
    CASE_CONVERT:
    case FLOAT_EXPR:
    case FIX_TRUNC_EXPR:
      /* We should have handled this above in the lvalue_kind check.  */
      gcc_unreachable ();
      break;

    case BASELINK:
      arg = BASELINK_FUNCTIONS (arg);
      /* Fall through.  */

    case OVERLOAD:
      arg = OVL_FIRST (arg);
      break;

    case OFFSET_REF:
    offset_ref:
      /* Turn a reference to a non-static data member into a
	 pointer-to-member.  */
      {
	tree type;
	tree t;

	gcc_assert (PTRMEM_OK_P (arg));

	t = TREE_OPERAND (arg, 1);
	if (TYPE_REF_P (TREE_TYPE (t)))
	  {
	    if (complain & tf_error)
	      error_at (loc,
			"cannot create pointer to reference member %qD", t);
	    return error_mark_node;
	  }

	/* Forming a pointer-to-member is a use of non-pure-virtual fns.  */
	if (TREE_CODE (t) == FUNCTION_DECL
	    && !DECL_PURE_VIRTUAL_P (t)
	    && !mark_used (t, complain) && !(complain & tf_error))
	  return error_mark_node;

	/* Pull out the function_decl for a single xobj member function, and
	   let the rest of this function handle it.  This is similar to how
	   static member functions are handled in the BASELINK case above.  */
	if (DECL_XOBJ_MEMBER_FUNCTION_P (t))
	  {
	    arg = t;
	    break;
	  }

	type = build_ptrmem_type (context_for_name_lookup (t),
				  TREE_TYPE (t));
	t = make_ptrmem_cst (type, t);
	return t;
      }

    default:
      break;
    }

  if (argtype != error_mark_node)
    argtype = build_pointer_type (argtype);

  if (bitfield_p (arg))
    {
      if (complain & tf_error)
	error_at (loc, "attempt to take address of bit-field");
      return error_mark_node;
    }

  /* In a template, we are processing a non-dependent expression
     so we can just form an ADDR_EXPR with the correct type.  */
  if (processing_template_decl || TREE_CODE (arg) != COMPONENT_REF)
    {
      if (!mark_single_function (arg, complain))
	return error_mark_node;
      val = build_address (arg);
      if (TREE_CODE (arg) == OFFSET_REF)
	PTRMEM_OK_P (val) = PTRMEM_OK_P (arg);
    }
  else if (BASELINK_P (TREE_OPERAND (arg, 1)))
    {
      tree fn = BASELINK_FUNCTIONS (TREE_OPERAND (arg, 1));

      if (TREE_CODE (fn) == OVERLOAD && OVL_SINGLE_P (fn))
	fn = OVL_FIRST (fn);

      /* We can only get here with a single static member
	 function.  */
      gcc_assert (TREE_CODE (fn) == FUNCTION_DECL
		  && DECL_STATIC_FUNCTION_P (fn));
      if (!mark_used (fn, complain) && !(complain & tf_error))
	return error_mark_node;
      val = build_address (fn);
      if (TREE_SIDE_EFFECTS (TREE_OPERAND (arg, 0)))
	/* Do not lose object's side effects.  */
	val = build2 (COMPOUND_EXPR, TREE_TYPE (val),
		      TREE_OPERAND (arg, 0), val);
    }
  else
    {
      tree object = TREE_OPERAND (arg, 0);
      tree field = TREE_OPERAND (arg, 1);
      gcc_assert (same_type_ignoring_top_level_qualifiers_p
		  (TREE_TYPE (object), decl_type_context (field)));
      val = build_address (arg);
    }

  if (TYPE_PTR_P (argtype)
      && TREE_CODE (TREE_TYPE (argtype)) == METHOD_TYPE)
    {
      build_ptrmemfunc_type (argtype);
      val = build_ptrmemfunc (argtype, val, 0,
			      /*c_cast_p=*/false,
			      complain);
    }

  /* Ensure we have EXPR_LOCATION set for possible later diagnostics.  */
  if (TREE_CODE (val) == ADDR_EXPR
      && TREE_CODE (TREE_OPERAND (val, 0)) == FUNCTION_DECL)
    SET_EXPR_LOCATION (val, input_location);

  return val;
}

/* Take the address of ARG if it has one, even if it's an rvalue.  */

tree
cp_build_addr_expr (tree arg, tsubst_flags_t complain)
{
  return cp_build_addr_expr_1 (arg, 0, complain);
}

/* Take the address of ARG, but only if it's an lvalue.  */

static tree
cp_build_addr_expr_strict (tree arg, tsubst_flags_t complain)
{
  return cp_build_addr_expr_1 (arg, 1, complain);
}

/* C++: Must handle pointers to members.

   Perhaps type instantiation should be extended to handle conversion
   from aggregates to types we don't yet know we want?  (Or are those
   cases typically errors which should be reported?)

   NOCONVERT suppresses the default promotions (such as from short to int).  */

tree
cp_build_unary_op (enum tree_code code, tree xarg, bool noconvert,
                   tsubst_flags_t complain)
{
  /* No default_conversion here.  It causes trouble for ADDR_EXPR.  */
  tree arg = xarg;
  location_t location = cp_expr_loc_or_input_loc (arg);
  tree argtype = 0;
  tree eptype = NULL_TREE;
  const char *errstring = NULL;
  tree val;
  const char *invalid_op_diag;

  if (!arg || error_operand_p (arg))
    return error_mark_node;

  arg = resolve_nondeduced_context (arg, complain);

  if ((invalid_op_diag
       = targetm.invalid_unary_op ((code == UNARY_PLUS_EXPR
				    ? CONVERT_EXPR
				    : code),
				   TREE_TYPE (arg))))
    {
      if (complain & tf_error)
	error (invalid_op_diag);
      return error_mark_node;
    }

  if (TREE_CODE (arg) == EXCESS_PRECISION_EXPR)
    {
      eptype = TREE_TYPE (arg);
      arg = TREE_OPERAND (arg, 0);
    }

  switch (code)
    {
    case UNARY_PLUS_EXPR:
    case NEGATE_EXPR:
      {
	int flags = WANT_ARITH | WANT_ENUM;
	/* Unary plus (but not unary minus) is allowed on pointers.  */
	if (code == UNARY_PLUS_EXPR)
	  flags |= WANT_POINTER;
	arg = build_expr_type_conversion (flags, arg, true);
	if (!arg)
	  errstring = (code == NEGATE_EXPR
		       ? _("wrong type argument to unary minus")
		       : _("wrong type argument to unary plus"));
	else
	  {
	    if (!noconvert && INTEGRAL_OR_ENUMERATION_TYPE_P (TREE_TYPE (arg)))
	      arg = cp_perform_integral_promotions (arg, complain);

	    /* Make sure the result is not an lvalue: a unary plus or minus
	       expression is always a rvalue.  */
	    arg = rvalue (arg);
	  }
      }
      break;

    case BIT_NOT_EXPR:
      if (TREE_CODE (TREE_TYPE (arg)) == COMPLEX_TYPE)
	{
	  code = CONJ_EXPR;
	  if (!noconvert)
	    {
	      arg = cp_default_conversion (arg, complain);
	      if (arg == error_mark_node)
		return error_mark_node;
	    }
	}
      else if (!(arg = build_expr_type_conversion (WANT_INT | WANT_ENUM
						   | WANT_VECTOR_OR_COMPLEX,
						   arg, true)))
	errstring = _("wrong type argument to bit-complement");
      else if (!noconvert && INTEGRAL_OR_ENUMERATION_TYPE_P (TREE_TYPE (arg)))
	{
	  /* Warn if the expression has boolean value.  */
	  if (TREE_CODE (TREE_TYPE (arg)) == BOOLEAN_TYPE
	      && (complain & tf_warning))
	    {
	      auto_diagnostic_group d;
	      if (warning_at (location, OPT_Wbool_operation,
			      "%<~%> on an expression of type %<bool%>"))
		inform (location, "did you mean to use logical not (%<!%>)?");
	    }
	  arg = cp_perform_integral_promotions (arg, complain);
	}
      else if (!noconvert && VECTOR_TYPE_P (TREE_TYPE (arg)))
	arg = mark_rvalue_use (arg);
      break;

    case ABS_EXPR:
      if (!(arg = build_expr_type_conversion (WANT_ARITH | WANT_ENUM, arg, true)))
	errstring = _("wrong type argument to abs");
      else if (!noconvert)
	{
	  arg = cp_default_conversion (arg, complain);
	  if (arg == error_mark_node)
	    return error_mark_node;
	}
      break;

    case CONJ_EXPR:
      /* Conjugating a real value is a no-op, but allow it anyway.  */
      if (!(arg = build_expr_type_conversion (WANT_ARITH | WANT_ENUM, arg, true)))
	errstring = _("wrong type argument to conjugation");
      else if (!noconvert)
	{
	  arg = cp_default_conversion (arg, complain);
	  if (arg == error_mark_node)
	    return error_mark_node;
	}
      break;

    case TRUTH_NOT_EXPR:
      if (gnu_vector_type_p (TREE_TYPE (arg)))
	return cp_build_binary_op (input_location, EQ_EXPR, arg,
				   build_zero_cst (TREE_TYPE (arg)), complain);
      arg = perform_implicit_conversion (boolean_type_node, arg,
					 complain);
      if (arg != error_mark_node)
	{
	  if (processing_template_decl)
	    return build1_loc (location, TRUTH_NOT_EXPR, boolean_type_node, arg);
	  val = invert_truthvalue_loc (location, arg);
	  if (obvalue_p (val))
	    val = non_lvalue_loc (location, val);
	  return val;
	}
      errstring = _("in argument to unary !");
      break;

    case NOP_EXPR:
      break;

    case REALPART_EXPR:
    case IMAGPART_EXPR:
      val = build_real_imag_expr (input_location, code, arg);
      if (eptype && TREE_CODE (eptype) == COMPLEX_EXPR)
	val = build1_loc (input_location, EXCESS_PRECISION_EXPR,
			  TREE_TYPE (eptype), val);
      return val;

    case PREINCREMENT_EXPR:
    case POSTINCREMENT_EXPR:
    case PREDECREMENT_EXPR:
    case POSTDECREMENT_EXPR:
      /* Handle complex lvalues (when permitted)
	 by reduction to simpler cases.  */

      val = unary_complex_lvalue (code, arg);
      if (val != 0)
	goto return_build_unary_op;

      tree stripped_arg;
      stripped_arg = tree_strip_any_location_wrapper (arg);
      if ((VAR_P (stripped_arg) || TREE_CODE (stripped_arg) == PARM_DECL)
	  && !DECL_READ_P (stripped_arg)
	  && (VAR_P (stripped_arg) ? warn_unused_but_set_variable
				   : warn_unused_but_set_parameter) > 1)
	{
	  arg = mark_lvalue_use (arg);
	  DECL_READ_P (stripped_arg) = 0;
	}
      else
	arg = mark_lvalue_use (arg);

      /* Increment or decrement the real part of the value,
	 and don't change the imaginary part.  */
      if (TREE_CODE (TREE_TYPE (arg)) == COMPLEX_TYPE)
	{
	  tree real, imag;

	  arg = cp_stabilize_reference (arg);
	  real = cp_build_unary_op (REALPART_EXPR, arg, true, complain);
	  imag = cp_build_unary_op (IMAGPART_EXPR, arg, true, complain);
	  real = cp_build_unary_op (code, real, true, complain);
	  if (real == error_mark_node || imag == error_mark_node)
	    return error_mark_node;
	  val = build2 (COMPLEX_EXPR, TREE_TYPE (arg), real, imag);
	  goto return_build_unary_op;
	}

      /* Report invalid types.  */

      if (!(arg = build_expr_type_conversion (WANT_ARITH | WANT_POINTER,
					      arg, true)))
	{
	  if (code == PREINCREMENT_EXPR)
	    errstring = _("no pre-increment operator for type");
	  else if (code == POSTINCREMENT_EXPR)
	    errstring = _("no post-increment operator for type");
	  else if (code == PREDECREMENT_EXPR)
	    errstring = _("no pre-decrement operator for type");
	  else
	    errstring = _("no post-decrement operator for type");
	  break;
	}
      else if (arg == error_mark_node)
	return error_mark_node;

      /* Report something read-only.  */

      if (CP_TYPE_CONST_P (TREE_TYPE (arg))
	  || TREE_READONLY (arg))
        {
          if (complain & tf_error)
	    cxx_readonly_error (location, arg,
				((code == PREINCREMENT_EXPR
				  || code == POSTINCREMENT_EXPR)
				 ? lv_increment : lv_decrement));
          else
            return error_mark_node;
        }

      {
	tree inc;
	tree declared_type = unlowered_expr_type (arg);

	argtype = TREE_TYPE (arg);

	/* ARM $5.2.5 last annotation says this should be forbidden.  */
	if (TREE_CODE (argtype) == ENUMERAL_TYPE)
          {
            if (complain & tf_error)
              permerror (location, (code == PREINCREMENT_EXPR
				    || code == POSTINCREMENT_EXPR)
                         ? G_("ISO C++ forbids incrementing an enum")
                         : G_("ISO C++ forbids decrementing an enum"));
            else
              return error_mark_node;
          }

	/* Compute the increment.  */

	if (TYPE_PTR_P (argtype))
	  {
	    tree type = complete_type (TREE_TYPE (argtype));

	    if (!COMPLETE_OR_VOID_TYPE_P (type))
              {
                if (complain & tf_error)
                  error_at (location, ((code == PREINCREMENT_EXPR
					|| code == POSTINCREMENT_EXPR))
			    ? G_("cannot increment a pointer to incomplete "
				 "type %qT")
			    : G_("cannot decrement a pointer to incomplete "
				 "type %qT"),
			    TREE_TYPE (argtype));
                else
                  return error_mark_node;
              }
	    else if (!TYPE_PTROB_P (argtype))
              {
                if (complain & tf_error)
                  pedwarn (location, OPT_Wpointer_arith,
			   (code == PREINCREMENT_EXPR
                              || code == POSTINCREMENT_EXPR)
			   ? G_("ISO C++ forbids incrementing a pointer "
				"of type %qT")
			   : G_("ISO C++ forbids decrementing a pointer "
				"of type %qT"),
			   argtype);
                else
                  return error_mark_node;
              }
	    else if (!verify_type_context (location, TCTX_POINTER_ARITH,
					   TREE_TYPE (argtype),
					   !(complain & tf_error)))
	      return error_mark_node;

	    inc = cxx_sizeof_nowarn (TREE_TYPE (argtype));
	  }
	else
	  inc = VECTOR_TYPE_P (argtype)
	    ? build_one_cst (argtype)
	    : integer_one_node;

	inc = cp_convert (argtype, inc, complain);

	/* If 'arg' is an Objective-C PROPERTY_REF expression, then we
	   need to ask Objective-C to build the increment or decrement
	   expression for it.  */
	if (objc_is_property_ref (arg))
	  return objc_build_incr_expr_for_property_ref (input_location, code,
							arg, inc);

	/* Complain about anything else that is not a true lvalue.  */
	if (!lvalue_or_else (arg, ((code == PREINCREMENT_EXPR
				    || code == POSTINCREMENT_EXPR)
				   ? lv_increment : lv_decrement),
                             complain))
	  return error_mark_node;

	/* [depr.volatile.type] "Postfix ++ and -- expressions and
	   prefix ++ and -- expressions of volatile-qualified arithmetic
	   and pointer types are deprecated."  */
	if ((TREE_THIS_VOLATILE (arg) || CP_TYPE_VOLATILE_P (TREE_TYPE (arg)))
	    && (complain & tf_warning))
	  warning_at (location, OPT_Wvolatile,
		      "%qs expression of %<volatile%>-qualified type is "
		      "deprecated",
		      ((code == PREINCREMENT_EXPR
			|| code == POSTINCREMENT_EXPR)
		       ? "++" : "--"));

	/* Forbid using -- or ++ in C++17 on `bool'.  */
	if (TREE_CODE (declared_type) == BOOLEAN_TYPE)
	  {
	    if (code == POSTDECREMENT_EXPR || code == PREDECREMENT_EXPR)
	      {
                if (complain & tf_error)
		  error_at (location,
			    "use of an operand of type %qT in %<operator--%> "
			    "is forbidden", boolean_type_node);
		return error_mark_node;
	      }
	    else
	      {
		if (cxx_dialect >= cxx17)
		  {
		    if (complain & tf_error)
		      error_at (location,
				"use of an operand of type %qT in "
				"%<operator++%> is forbidden in C++17",
				boolean_type_node);
		    return error_mark_node;
		  }
		/* Otherwise, [depr.incr.bool] says this is deprecated.  */
		else if (complain & tf_warning)
		  warning_at (location, OPT_Wdeprecated,
			      "use of an operand of type %qT "
			      "in %<operator++%> is deprecated",
			      boolean_type_node);
	      }
	    val = boolean_increment (code, arg);
	  }
	else if (code == POSTINCREMENT_EXPR || code == POSTDECREMENT_EXPR)
	  /* An rvalue has no cv-qualifiers.  */
	  val = build2 (code, cv_unqualified (TREE_TYPE (arg)), arg, inc);
	else
	  val = build2 (code, TREE_TYPE (arg), arg, inc);

	TREE_SIDE_EFFECTS (val) = 1;
	goto return_build_unary_op;
      }

    case ADDR_EXPR:
      /* Note that this operation never does default_conversion
	 regardless of NOCONVERT.  */
      return cp_build_addr_expr (arg, complain);

    default:
      break;
    }

  if (!errstring)
    {
      if (argtype == 0)
	argtype = TREE_TYPE (arg);
      val = build1 (code, argtype, arg);
    return_build_unary_op:
      if (eptype)
	val = build1 (EXCESS_PRECISION_EXPR, eptype, val);
      return val;
    }

  if (complain & tf_error)
    error_at (location, "%s", errstring);
  return error_mark_node;
}

/* Hook for the c-common bits that build a unary op.  */
tree
build_unary_op (location_t /*location*/,
		enum tree_code code, tree xarg, bool noconvert)
{
  return cp_build_unary_op (code, xarg, noconvert, tf_warning_or_error);
}

/* Adjust LVALUE, an MODIFY_EXPR, PREINCREMENT_EXPR or PREDECREMENT_EXPR,
   so that it is a valid lvalue even for GENERIC by replacing
   (lhs = rhs) with ((lhs = rhs), lhs)
   (--lhs) with ((--lhs), lhs)
   (++lhs) with ((++lhs), lhs)
   and if lhs has side-effects, calling cp_stabilize_reference on it, so
   that it can be evaluated multiple times.  */

tree
genericize_compound_lvalue (tree lvalue)
{
  if (TREE_SIDE_EFFECTS (TREE_OPERAND (lvalue, 0)))
    lvalue = build2 (TREE_CODE (lvalue), TREE_TYPE (lvalue),
		     cp_stabilize_reference (TREE_OPERAND (lvalue, 0)),
		     TREE_OPERAND (lvalue, 1));
  return build2 (COMPOUND_EXPR, TREE_TYPE (TREE_OPERAND (lvalue, 0)),
		 lvalue, TREE_OPERAND (lvalue, 0));
}

/* Apply unary lvalue-demanding operator CODE to the expression ARG
   for certain kinds of expressions which are not really lvalues
   but which we can accept as lvalues.

   If ARG is not a kind of expression we can handle, return
   NULL_TREE.  */

tree
unary_complex_lvalue (enum tree_code code, tree arg)
{
  /* Inside a template, making these kinds of adjustments is
     pointless; we are only concerned with the type of the
     expression.  */
  if (processing_template_decl)
    return NULL_TREE;

  /* Handle (a, b) used as an "lvalue".  */
  if (TREE_CODE (arg) == COMPOUND_EXPR)
    {
      tree real_result = cp_build_unary_op (code, TREE_OPERAND (arg, 1), false,
                                            tf_warning_or_error);
      return build2 (COMPOUND_EXPR, TREE_TYPE (real_result),
		     TREE_OPERAND (arg, 0), real_result);
    }

  /* Handle (a ? b : c) used as an "lvalue".  */
  if (TREE_CODE (arg) == COND_EXPR
      || TREE_CODE (arg) == MIN_EXPR || TREE_CODE (arg) == MAX_EXPR)
    return rationalize_conditional_expr (code, arg, tf_warning_or_error);

  /* Handle (a = b), (++a), and (--a) used as an "lvalue".  */
  if (TREE_CODE (arg) == MODIFY_EXPR
      || TREE_CODE (arg) == PREINCREMENT_EXPR
      || TREE_CODE (arg) == PREDECREMENT_EXPR)
    return unary_complex_lvalue (code, genericize_compound_lvalue (arg));

  if (code != ADDR_EXPR)
    return NULL_TREE;

  /* Handle (a = b) used as an "lvalue" for `&'.  */
  if (TREE_CODE (arg) == MODIFY_EXPR
      || TREE_CODE (arg) == INIT_EXPR)
    {
      tree real_result = cp_build_unary_op (code, TREE_OPERAND (arg, 0), false,
                                            tf_warning_or_error);
      arg = build2 (COMPOUND_EXPR, TREE_TYPE (real_result),
		    arg, real_result);
      suppress_warning (arg /* What warning? */);
      return arg;
    }

  if (FUNC_OR_METHOD_TYPE_P (TREE_TYPE (arg))
      || TREE_CODE (arg) == OFFSET_REF)
    return NULL_TREE;

  /* We permit compiler to make function calls returning
     objects of aggregate type look like lvalues.  */
  {
    tree targ = arg;

    if (TREE_CODE (targ) == SAVE_EXPR)
      targ = TREE_OPERAND (targ, 0);

    if (TREE_CODE (targ) == CALL_EXPR && MAYBE_CLASS_TYPE_P (TREE_TYPE (targ)))
      {
	if (TREE_CODE (arg) == SAVE_EXPR)
	  targ = arg;
	else
	  targ = build_cplus_new (TREE_TYPE (arg), arg, tf_warning_or_error);
	return build1 (ADDR_EXPR, build_pointer_type (TREE_TYPE (arg)), targ);
      }

    if (TREE_CODE (arg) == SAVE_EXPR && INDIRECT_REF_P (targ))
      return build3 (SAVE_EXPR, build_pointer_type (TREE_TYPE (arg)),
		     TREE_OPERAND (targ, 0), current_function_decl, NULL);
  }

  /* Don't let anything else be handled specially.  */
  return NULL_TREE;
}

/* Mark EXP saying that we need to be able to take the
   address of it; it should not be allocated in a register.
   Value is true if successful.  ARRAY_REF_P is true if this
   is for ARRAY_REF construction - in that case we don't want
   to look through VIEW_CONVERT_EXPR from VECTOR_TYPE to ARRAY_TYPE,
   it is fine to use ARRAY_REFs for vector subscripts on vector
   register variables.

   C++: we do not allow `current_class_ptr' to be addressable.  */

bool
cxx_mark_addressable (tree exp, bool array_ref_p)
{
  tree x = exp;

  while (1)
    switch (TREE_CODE (x))
      {
      case VIEW_CONVERT_EXPR:
	if (array_ref_p
	    && TREE_CODE (TREE_TYPE (x)) == ARRAY_TYPE
	    && VECTOR_TYPE_P (TREE_TYPE (TREE_OPERAND (x, 0))))
	  return true;
	x = TREE_OPERAND (x, 0);
	break;

      case COMPONENT_REF:
	if (bitfield_p (x))
	  error ("attempt to take address of bit-field");
	/* FALLTHRU */
      case ADDR_EXPR:
      case ARRAY_REF:
      case REALPART_EXPR:
      case IMAGPART_EXPR:
	x = TREE_OPERAND (x, 0);
	break;

      case PARM_DECL:
	if (x == current_class_ptr)
	  {
	    error ("cannot take the address of %<this%>, which is an rvalue expression");
	    TREE_ADDRESSABLE (x) = 1; /* so compiler doesn't die later.  */
	    return true;
	  }
	/* Fall through.  */

      case VAR_DECL:
	/* Caller should not be trying to mark initialized
	   constant fields addressable.  */
	gcc_assert (DECL_LANG_SPECIFIC (x) == 0
		    || DECL_IN_AGGR_P (x) == 0
		    || TREE_STATIC (x)
		    || DECL_EXTERNAL (x));
	/* Fall through.  */

      case RESULT_DECL:
	if (DECL_REGISTER (x) && !TREE_ADDRESSABLE (x)
	    && !DECL_ARTIFICIAL (x))
	  {
	    if (VAR_P (x) && DECL_HARD_REGISTER (x))
	      {
		error
		  ("address of explicit register variable %qD requested", x);
		return false;
	      }
	    else if (extra_warnings)
	      warning
		(OPT_Wextra, "address requested for %qD, which is declared %<register%>", x);
	  }
	TREE_ADDRESSABLE (x) = 1;
	return true;

      case CONST_DECL:
      case FUNCTION_DECL:
	TREE_ADDRESSABLE (x) = 1;
	return true;

      case CONSTRUCTOR:
	TREE_ADDRESSABLE (x) = 1;
	return true;

      case TARGET_EXPR:
	TREE_ADDRESSABLE (x) = 1;
	cxx_mark_addressable (TARGET_EXPR_SLOT (x));
	return true;

      default:
	return true;
    }
}

/* Build and return a conditional expression IFEXP ? OP1 : OP2.  */

tree
build_x_conditional_expr (location_t loc, tree ifexp, tree op1, tree op2,
                          tsubst_flags_t complain)
{
  tree orig_ifexp = ifexp;
  tree orig_op1 = op1;
  tree orig_op2 = op2;
  tree expr;

  if (processing_template_decl)
    {
      /* The standard says that the expression is type-dependent if
	 IFEXP is type-dependent, even though the eventual type of the
	 expression doesn't dependent on IFEXP.  */
      if (type_dependent_expression_p (ifexp)
	  /* As a GNU extension, the middle operand may be omitted.  */
	  || (op1 && type_dependent_expression_p (op1))
	  || type_dependent_expression_p (op2))
	return build_min_nt_loc (loc, COND_EXPR, ifexp, op1, op2);
    }

  expr = build_conditional_expr (loc, ifexp, op1, op2, complain);
  if (processing_template_decl && expr != error_mark_node)
    {
      tree min = build_min_non_dep (COND_EXPR, expr,
				    orig_ifexp, orig_op1, orig_op2);
      expr = convert_from_reference (min);
    }
  return expr;
}

/* Given a list of expressions, return a compound expression
   that performs them all and returns the value of the last of them.  */

tree
build_x_compound_expr_from_list (tree list, expr_list_kind exp,
				 tsubst_flags_t complain)
{
  tree expr = TREE_VALUE (list);

  if (BRACE_ENCLOSED_INITIALIZER_P (expr)
      && !CONSTRUCTOR_IS_DIRECT_INIT (expr))
    {
      if (complain & tf_error)
	pedwarn (cp_expr_loc_or_input_loc (expr), 0,
		 "list-initializer for non-class type must not "
		 "be parenthesized");
      else
	return error_mark_node;
    }

  if (TREE_CHAIN (list))
    {
      if (complain & tf_error)
	switch (exp)
	  {
	  case ELK_INIT:
	    permerror (input_location, "expression list treated as compound "
				       "expression in initializer");
	    break;
	  case ELK_MEM_INIT:
	    permerror (input_location, "expression list treated as compound "
				       "expression in mem-initializer");
	    break;
	  case ELK_FUNC_CAST:
	    permerror (input_location, "expression list treated as compound "
				       "expression in functional cast");
	    break;
	  default:
	    gcc_unreachable ();
	  }
      else
	return error_mark_node;

      for (list = TREE_CHAIN (list); list; list = TREE_CHAIN (list))
	expr = build_x_compound_expr (EXPR_LOCATION (TREE_VALUE (list)),
				      expr, TREE_VALUE (list), NULL_TREE,
				      complain);
    }

  return expr;
}

/* Like build_x_compound_expr_from_list, but using a VEC.  */

tree
build_x_compound_expr_from_vec (vec<tree, va_gc> *vec, const char *msg,
				tsubst_flags_t complain)
{
  if (vec_safe_is_empty (vec))
    return NULL_TREE;
  else if (vec->length () == 1)
    return (*vec)[0];
  else
    {
      tree expr;
      unsigned int ix;
      tree t;

      if (msg != NULL)
	{
	  if (complain & tf_error)
	    permerror (input_location,
		       "%s expression list treated as compound expression",
		       msg);
	  else
	    return error_mark_node;
	}

      expr = (*vec)[0];
      for (ix = 1; vec->iterate (ix, &t); ++ix)
	expr = build_x_compound_expr (EXPR_LOCATION (t), expr,
				      t, NULL_TREE, complain);

      return expr;
    }
}

/* Handle overloading of the ',' operator when needed.  */

tree
build_x_compound_expr (location_t loc, tree op1, tree op2,
		       tree lookups, tsubst_flags_t complain)
{
  tree result;
  tree orig_op1 = op1;
  tree orig_op2 = op2;
  tree overload = NULL_TREE;

  if (processing_template_decl)
    {
      if (type_dependent_expression_p (op1)
	  || type_dependent_expression_p (op2))
	{
	  result = build_min_nt_loc (loc, COMPOUND_EXPR, op1, op2);
	  TREE_TYPE (result)
	    = build_dependent_operator_type (lookups, COMPOUND_EXPR, false);
	  return result;
	}
    }

  result = build_new_op (loc, COMPOUND_EXPR, LOOKUP_NORMAL, op1, op2,
			 NULL_TREE, lookups, &overload, complain);
  if (!result)
    result = cp_build_compound_expr (op1, op2, complain);

  if (processing_template_decl && result != error_mark_node)
    {
      if (overload != NULL_TREE)
	return (build_min_non_dep_op_overload
		(COMPOUND_EXPR, result, overload, orig_op1, orig_op2));

      return build_min_non_dep (COMPOUND_EXPR, result, orig_op1, orig_op2);
    }

  return result;
}

/* Like cp_build_compound_expr, but for the c-common bits.  */

tree
build_compound_expr (location_t /*loc*/, tree lhs, tree rhs)
{
  return cp_build_compound_expr (lhs, rhs, tf_warning_or_error);
}

/* Build a compound expression.  */

tree
cp_build_compound_expr (tree lhs, tree rhs, tsubst_flags_t complain)
{
  lhs = convert_to_void (lhs, ICV_LEFT_OF_COMMA, complain);

  if (lhs == error_mark_node || rhs == error_mark_node)
    return error_mark_node;

  if (TREE_CODE (lhs) == EXCESS_PRECISION_EXPR)
    lhs = TREE_OPERAND (lhs, 0);
  tree eptype = NULL_TREE;
  if (TREE_CODE (rhs) == EXCESS_PRECISION_EXPR)
    {
      eptype = TREE_TYPE (rhs);
      rhs = TREE_OPERAND (rhs, 0);
    }

  if (TREE_CODE (rhs) == TARGET_EXPR)
    {
      /* If the rhs is a TARGET_EXPR, then build the compound
	 expression inside the target_expr's initializer. This
	 helps the compiler to eliminate unnecessary temporaries.  */
      tree init = TARGET_EXPR_INITIAL (rhs);

      init = build2 (COMPOUND_EXPR, TREE_TYPE (init), lhs, init);
      TARGET_EXPR_INITIAL (rhs) = init;

      if (eptype)
	rhs = build1 (EXCESS_PRECISION_EXPR, eptype, rhs);
      return rhs;
    }

  rhs = resolve_nondeduced_context (rhs, complain);

  if (type_unknown_p (rhs))
    {
      if (complain & tf_error)
	error_at (cp_expr_loc_or_input_loc (rhs),
		  "no context to resolve type of %qE", rhs);
      return error_mark_node;
    }

  tree ret = build2 (COMPOUND_EXPR, TREE_TYPE (rhs), lhs, rhs);
  if (eptype)
    ret = build1 (EXCESS_PRECISION_EXPR, eptype, ret);
  return ret;
}

/* Issue a diagnostic message if casting from SRC_TYPE to DEST_TYPE
   casts away constness.  CAST gives the type of cast.  Returns true
   if the cast is ill-formed, false if it is well-formed.

   ??? This function warns for casting away any qualifier not just
   const.  We would like to specify exactly what qualifiers are casted
   away.
*/

static bool
check_for_casting_away_constness (location_t loc, tree src_type,
				  tree dest_type, enum tree_code cast,
				  tsubst_flags_t complain)
{
  /* C-style casts are allowed to cast away constness.  With
     WARN_CAST_QUAL, we still want to issue a warning.  */
  if (cast == CAST_EXPR && !warn_cast_qual)
    return false;

  if (!casts_away_constness (src_type, dest_type, complain))
    return false;

  switch (cast)
    {
    case CAST_EXPR:
      if (complain & tf_warning)
	warning_at (loc, OPT_Wcast_qual,
		    "cast from type %qT to type %qT casts away qualifiers",
		    src_type, dest_type);
      return false;

    case STATIC_CAST_EXPR:
      if (complain & tf_error)
	error_at (loc, "%<static_cast%> from type %qT to type %qT casts "
		  "away qualifiers",
		  src_type, dest_type);
      return true;

    case REINTERPRET_CAST_EXPR:
      if (complain & tf_error)
	error_at (loc, "%<reinterpret_cast%> from type %qT to type %qT "
		  "casts away qualifiers",
		  src_type, dest_type);
      return true;

    default:
      gcc_unreachable();
    }
}

/* Warns if the cast from expression EXPR to type TYPE is useless.  */
void
maybe_warn_about_useless_cast (location_t loc, tree type, tree expr,
			       tsubst_flags_t complain)
{
  if (warn_useless_cast
      && complain & tf_warning)
    {
      if (TYPE_REF_P (type)
	  ? ((TYPE_REF_IS_RVALUE (type)
	      ? xvalue_p (expr) : lvalue_p (expr))
	     && same_type_p (TREE_TYPE (expr), TREE_TYPE (type)))
	  /* Don't warn when converting a class object to a non-reference type,
	     because that's a common way to create a temporary.  */
	  : (!glvalue_p (expr) && same_type_p (TREE_TYPE (expr), type)))
	warning_at (loc, OPT_Wuseless_cast,
		    "useless cast to type %q#T", type);
    }
}

/* Warns if the cast ignores cv-qualifiers on TYPE.  */
static void
maybe_warn_about_cast_ignoring_quals (location_t loc, tree type,
				      tsubst_flags_t complain)
{
  if (warn_ignored_qualifiers
      && complain & tf_warning
      && !CLASS_TYPE_P (type)
      && (cp_type_quals (type) & (TYPE_QUAL_CONST|TYPE_QUAL_VOLATILE)))
    warning_at (loc, OPT_Wignored_qualifiers,
		"type qualifiers ignored on cast result type");
}

/* Convert EXPR (an expression with pointer-to-member type) to TYPE
   (another pointer-to-member type in the same hierarchy) and return
   the converted expression.  If ALLOW_INVERSE_P is permitted, a
   pointer-to-derived may be converted to pointer-to-base; otherwise,
   only the other direction is permitted.  If C_CAST_P is true, this
   conversion is taking place as part of a C-style cast.  */

tree
convert_ptrmem (tree type, tree expr, bool allow_inverse_p,
		bool c_cast_p, tsubst_flags_t complain)
{
  if (same_type_p (type, TREE_TYPE (expr)))
    return expr;

  if (TYPE_PTRDATAMEM_P (type))
    {
      tree obase = TYPE_PTRMEM_CLASS_TYPE (TREE_TYPE (expr));
      tree nbase = TYPE_PTRMEM_CLASS_TYPE (type);
      tree delta = (get_delta_difference
		    (obase, nbase,
		     allow_inverse_p, c_cast_p, complain));

      if (delta == error_mark_node)
	return error_mark_node;

      if (!same_type_p (obase, nbase))
	{
	  if (TREE_CODE (expr) == PTRMEM_CST)
	    expr = cplus_expand_constant (expr);

	  tree cond = cp_build_binary_op (input_location, EQ_EXPR, expr,
					  build_int_cst (TREE_TYPE (expr), -1),
					  complain);
	  tree op1 = build_nop (ptrdiff_type_node, expr);
	  tree op2 = cp_build_binary_op (input_location, PLUS_EXPR, op1, delta,
					 complain);

	  expr = fold_build3_loc (input_location,
				  COND_EXPR, ptrdiff_type_node, cond, op1, op2);
	}

      return build_nop (type, expr);
    }
  else
    return build_ptrmemfunc (TYPE_PTRMEMFUNC_FN_TYPE (type), expr,
			     allow_inverse_p, c_cast_p, complain);
}

/* Perform a static_cast from EXPR to TYPE.  When C_CAST_P is true,
   this static_cast is being attempted as one of the possible casts
   allowed by a C-style cast.  (In that case, accessibility of base
   classes is not considered, and it is OK to cast away
   constness.)  Return the result of the cast.  *VALID_P is set to
   indicate whether or not the cast was valid.  */

static tree
build_static_cast_1 (location_t loc, tree type, tree expr, bool c_cast_p,
		     bool *valid_p, tsubst_flags_t complain)
{
  tree intype;
  tree result;
  cp_lvalue_kind clk;

  /* Assume the cast is valid.  */
  *valid_p = true;

  intype = unlowered_expr_type (expr);

  /* Save casted types in the function's used types hash table.  */
  used_types_insert (type);

  /* A prvalue of non-class type is cv-unqualified.  */
  if (!CLASS_TYPE_P (type))
    type = cv_unqualified (type);

  /* [expr.static.cast]

     An lvalue of type "cv1 B", where B is a class type, can be cast
     to type "reference to cv2 D", where D is a class derived (clause
     _class.derived_) from B, if a valid standard conversion from
     "pointer to D" to "pointer to B" exists (_conv.ptr_), cv2 is the
     same cv-qualification as, or greater cv-qualification than, cv1,
     and B is not a virtual base class of D.  */
  /* We check this case before checking the validity of "TYPE t =
     EXPR;" below because for this case:

       struct B {};
       struct D : public B { D(const B&); };
       extern B& b;
       void f() { static_cast<const D&>(b); }

     we want to avoid constructing a new D.  The standard is not
     completely clear about this issue, but our interpretation is
     consistent with other compilers.  */
  if (TYPE_REF_P (type)
      && CLASS_TYPE_P (TREE_TYPE (type))
      && CLASS_TYPE_P (intype)
      && (TYPE_REF_IS_RVALUE (type) || lvalue_p (expr))
      && DERIVED_FROM_P (intype, TREE_TYPE (type))
      && can_convert (build_pointer_type (TYPE_MAIN_VARIANT (intype)),
		      build_pointer_type (TYPE_MAIN_VARIANT
					  (TREE_TYPE (type))),
		      complain)
      && (c_cast_p
	  || at_least_as_qualified_p (TREE_TYPE (type), intype)))
    {
      tree base;

      if (processing_template_decl)
	return expr;

      /* There is a standard conversion from "D*" to "B*" even if "B"
	 is ambiguous or inaccessible.  If this is really a
	 static_cast, then we check both for inaccessibility and
	 ambiguity.  However, if this is a static_cast being performed
	 because the user wrote a C-style cast, then accessibility is
	 not considered.  */
      base = lookup_base (TREE_TYPE (type), intype,
			  c_cast_p ? ba_unique : ba_check,
			  NULL, complain);
      expr = cp_build_addr_expr (expr, complain);

      if (sanitize_flags_p (SANITIZE_VPTR))
	{
	  tree ubsan_check
	    = cp_ubsan_maybe_instrument_downcast (loc, type,
						  intype, expr);
	  if (ubsan_check)
	    expr = ubsan_check;
	}

      /* Convert from "B*" to "D*".  This function will check that "B"
	 is not a virtual base of "D".  Even if we don't have a guarantee
	 that expr is NULL, if the static_cast is to a reference type,
	 it is UB if it would be NULL, so omit the non-NULL check.  */
      expr = build_base_path (MINUS_EXPR, expr, base,
			      /*nonnull=*/flag_delete_null_pointer_checks,
			      complain);

      /* Convert the pointer to a reference -- but then remember that
	 there are no expressions with reference type in C++.

         We call rvalue so that there's an actual tree code
         (NON_LVALUE_EXPR) for the static_cast; otherwise, if the operand
         is a variable with the same type, the conversion would get folded
         away, leaving just the variable and causing lvalue_kind to give
         the wrong answer.  */
      expr = cp_fold_convert (type, expr);

      /* When -fsanitize=null, make sure to diagnose reference binding to
	 NULL even when the reference is converted to pointer later on.  */
      if (sanitize_flags_p (SANITIZE_NULL)
	  && TREE_CODE (expr) == COND_EXPR
	  && TREE_OPERAND (expr, 2)
	  && TREE_CODE (TREE_OPERAND (expr, 2)) == INTEGER_CST
	  && TREE_TYPE (TREE_OPERAND (expr, 2)) == type)
	ubsan_maybe_instrument_reference (&TREE_OPERAND (expr, 2));

      return convert_from_reference (rvalue (expr));
    }

  /* "A glvalue of type cv1 T1 can be cast to type rvalue reference to
     cv2 T2 if cv2 T2 is reference-compatible with cv1 T1 (8.5.3)."  */
  if (TYPE_REF_P (type)
      && TYPE_REF_IS_RVALUE (type)
      && (clk = real_lvalue_p (expr))
      && reference_compatible_p (TREE_TYPE (type), intype)
      && (c_cast_p || at_least_as_qualified_p (TREE_TYPE (type), intype)))
    {
      if (processing_template_decl)
	return expr;
      if (clk == clk_ordinary)
	{
	  /* Handle the (non-bit-field) lvalue case here by casting to
	     lvalue reference and then changing it to an rvalue reference.
	     Casting an xvalue to rvalue reference will be handled by the
	     main code path.  */
	  tree lref = cp_build_reference_type (TREE_TYPE (type), false);
	  result = (perform_direct_initialization_if_possible
		    (lref, expr, c_cast_p, complain));
	  result = build1 (NON_LVALUE_EXPR, type, result);
	  return convert_from_reference (result);
	}
      else
	/* For a bit-field or packed field, bind to a temporary.  */
	expr = rvalue (expr);
    }

  /* Resolve overloaded address here rather than once in
     implicit_conversion and again in the inverse code below.  */
  if (TYPE_PTRMEMFUNC_P (type) && type_unknown_p (expr))
    {
      expr = instantiate_type (type, expr, complain);
      intype = TREE_TYPE (expr);
    }

  /* [expr.static.cast]

     Any expression can be explicitly converted to type cv void.  */
  if (VOID_TYPE_P (type))
    {
      if (TREE_CODE (expr) == EXCESS_PRECISION_EXPR)
	expr = TREE_OPERAND (expr, 0);
      return convert_to_void (expr, ICV_CAST, complain);
    }

  /* [class.abstract]
     An abstract class shall not be used ... as the type of an explicit
     conversion.  */
  if (abstract_virtuals_error (ACU_CAST, type, complain))
    return error_mark_node;

  /* [expr.static.cast]

     An expression e can be explicitly converted to a type T using a
     static_cast of the form static_cast<T>(e) if the declaration T
     t(e);" is well-formed, for some invented temporary variable
     t.  */
  result = perform_direct_initialization_if_possible (type, expr,
						      c_cast_p, complain);
  /* P1975 allows static_cast<Aggr>(42), as well as static_cast<T[5]>(42),
     which initialize the first element of the aggregate.  We need to handle
     the array case specifically.  */
  if (result == NULL_TREE
      && cxx_dialect >= cxx20
      && TREE_CODE (type) == ARRAY_TYPE)
    {
      /* Create { EXPR } and perform direct-initialization from it.  */
      tree e = build_constructor_single (init_list_type_node, NULL_TREE, expr);
      CONSTRUCTOR_IS_DIRECT_INIT (e) = true;
      CONSTRUCTOR_IS_PAREN_INIT (e) = true;
      result = perform_direct_initialization_if_possible (type, e, c_cast_p,
							  complain);
    }
  if (result)
    {
      if (processing_template_decl)
	return expr;

      result = convert_from_reference (result);

      /* [expr.static.cast]

	 If T is a reference type, the result is an lvalue; otherwise,
	 the result is an rvalue.  */
      if (!TYPE_REF_P (type))
	{
	  result = rvalue (result);

	  if (result == expr && SCALAR_TYPE_P (type))
	    /* Leave some record of the cast.  */
	    result = build_nop (type, expr);
	}
      return result;
    }

  /* [expr.static.cast]

     The inverse of any standard conversion sequence (clause _conv_),
     other than the lvalue-to-rvalue (_conv.lval_), array-to-pointer
     (_conv.array_), function-to-pointer (_conv.func_), and boolean
     (_conv.bool_) conversions, can be performed explicitly using
     static_cast subject to the restriction that the explicit
     conversion does not cast away constness (_expr.const.cast_), and
     the following additional rules for specific cases:  */
  /* For reference, the conversions not excluded are: integral
     promotions, floating-point promotion, integral conversions,
     floating-point conversions, floating-integral conversions,
     pointer conversions, and pointer to member conversions.  */
  /* DR 128

     A value of integral _or enumeration_ type can be explicitly
     converted to an enumeration type.  */
  /* The effect of all that is that any conversion between any two
     types which are integral, floating, or enumeration types can be
     performed.  */
  if ((INTEGRAL_OR_ENUMERATION_TYPE_P (type)
       || SCALAR_FLOAT_TYPE_P (type))
      && (INTEGRAL_OR_ENUMERATION_TYPE_P (intype)
	  || SCALAR_FLOAT_TYPE_P (intype)))
    {
      if (processing_template_decl)
	return expr;
      if (TREE_CODE (expr) == EXCESS_PRECISION_EXPR)
	expr = TREE_OPERAND (expr, 0);
      /* [expr.static.cast]: "If the value is not a bit-field, the result
	 refers to the object or the specified base class subobject thereof;
	 otherwise, the lvalue-to-rvalue conversion is applied to the
	 bit-field and the resulting prvalue is used as the operand of the
	 static_cast."  There are no prvalue bit-fields; the l-to-r conversion
	 will give us an object of the underlying type of the bit-field.  */
      expr = decay_conversion (expr, complain);
      return ocp_convert (type, expr, CONV_C_CAST, LOOKUP_NORMAL, complain);
    }

  if (TYPE_PTR_P (type) && TYPE_PTR_P (intype)
      && CLASS_TYPE_P (TREE_TYPE (type))
      && CLASS_TYPE_P (TREE_TYPE (intype))
      && can_convert (build_pointer_type (TYPE_MAIN_VARIANT
					  (TREE_TYPE (intype))),
		      build_pointer_type (TYPE_MAIN_VARIANT
					  (TREE_TYPE (type))),
		      complain))
    {
      tree base;

      if (processing_template_decl)
	return expr;

      if (!c_cast_p
	  && check_for_casting_away_constness (loc, intype, type,
					       STATIC_CAST_EXPR,
					       complain))
	return error_mark_node;
      base = lookup_base (TREE_TYPE (type), TREE_TYPE (intype),
			  c_cast_p ? ba_unique : ba_check,
			  NULL, complain);
      expr = build_base_path (MINUS_EXPR, expr, base, /*nonnull=*/false,
			      complain);

      if (sanitize_flags_p (SANITIZE_VPTR))
	{
	  tree ubsan_check
	    = cp_ubsan_maybe_instrument_downcast (loc, type,
						  intype, expr);
	  if (ubsan_check)
	    expr = ubsan_check;
	}

      return cp_fold_convert (type, expr);
    }

  if ((TYPE_PTRDATAMEM_P (type) && TYPE_PTRDATAMEM_P (intype))
      || (TYPE_PTRMEMFUNC_P (type) && TYPE_PTRMEMFUNC_P (intype)))
    {
      tree c1;
      tree c2;
      tree t1;
      tree t2;

      c1 = TYPE_PTRMEM_CLASS_TYPE (intype);
      c2 = TYPE_PTRMEM_CLASS_TYPE (type);

      if (TYPE_PTRDATAMEM_P (type))
	{
	  t1 = (build_ptrmem_type
		(c1,
		 TYPE_MAIN_VARIANT (TYPE_PTRMEM_POINTED_TO_TYPE (intype))));
	  t2 = (build_ptrmem_type
		(c2,
		 TYPE_MAIN_VARIANT (TYPE_PTRMEM_POINTED_TO_TYPE (type))));
	}
      else
	{
	  t1 = intype;
	  t2 = type;
	}
      if (can_convert (t1, t2, complain) || can_convert (t2, t1, complain))
	{
	  if (!c_cast_p
	      && check_for_casting_away_constness (loc, intype, type,
						   STATIC_CAST_EXPR,
						   complain))
	    return error_mark_node;
	  if (processing_template_decl)
	    return expr;
	  return convert_ptrmem (type, expr, /*allow_inverse_p=*/1,
				 c_cast_p, complain);
	}
    }

  /* [expr.static.cast]

     An rvalue of type "pointer to cv void" can be explicitly
     converted to a pointer to object type.  A value of type pointer
     to object converted to "pointer to cv void" and back to the
     original pointer type will have its original value.  */
  if (TYPE_PTR_P (intype)
      && VOID_TYPE_P (TREE_TYPE (intype))
      && TYPE_PTROB_P (type))
    {
      if (!c_cast_p
	  && check_for_casting_away_constness (loc, intype, type,
					       STATIC_CAST_EXPR,
					       complain))
	return error_mark_node;
      if (processing_template_decl)
	return expr;
      return build_nop (type, expr);
    }

  *valid_p = false;
  return error_mark_node;
}

/* Return an expression representing static_cast<TYPE>(EXPR).  */

tree
build_static_cast (location_t loc, tree type, tree oexpr,
		   tsubst_flags_t complain)
{
  tree expr = oexpr;
  tree result;
  bool valid_p;

  if (type == error_mark_node || expr == error_mark_node)
    return error_mark_node;

  bool dependent = (dependent_type_p (type)
		    || type_dependent_expression_p (expr));
  if (dependent)
    {
    tmpl:
      expr = build_min (STATIC_CAST_EXPR, type, oexpr);
      /* We don't know if it will or will not have side effects.  */
      TREE_SIDE_EFFECTS (expr) = 1;
      result = convert_from_reference (expr);
      protected_set_expr_location (result, loc);
      return result;
    }

  /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
     Strip such NOP_EXPRs if VALUE is being used in non-lvalue context.  */
  if (!TYPE_REF_P (type)
      && TREE_CODE (expr) == NOP_EXPR
      && TREE_TYPE (expr) == TREE_TYPE (TREE_OPERAND (expr, 0)))
    expr = TREE_OPERAND (expr, 0);

  result = build_static_cast_1 (loc, type, expr, /*c_cast_p=*/false,
				&valid_p, complain);
  if (valid_p)
    {
      if (result != error_mark_node)
	{
	  maybe_warn_about_useless_cast (loc, type, expr, complain);
	  maybe_warn_about_cast_ignoring_quals (loc, type, complain);
	}
      if (processing_template_decl)
	goto tmpl;
      protected_set_expr_location (result, loc);
      return result;
    }

  if (complain & tf_error)
    {
      auto_diagnostic_group d;
      error_at (loc, "invalid %<static_cast%> from type %qT to type %qT",
		TREE_TYPE (expr), type);
      if ((TYPE_PTR_P (type) || TYPE_REF_P (type))
	  && CLASS_TYPE_P (TREE_TYPE (type))
	    && !COMPLETE_TYPE_P (TREE_TYPE (type)))
	inform (DECL_SOURCE_LOCATION (TYPE_MAIN_DECL (TREE_TYPE (type))),
		"class type %qT is incomplete", TREE_TYPE (type));
      tree expr_type = TREE_TYPE (expr);
      if (TYPE_PTR_P (expr_type))
	expr_type = TREE_TYPE (expr_type);
      if (CLASS_TYPE_P (expr_type) && !COMPLETE_TYPE_P (expr_type))
	inform (DECL_SOURCE_LOCATION (TYPE_MAIN_DECL (expr_type)),
		"class type %qT is incomplete", expr_type);
    }
  return error_mark_node;
}

/* EXPR is an expression with member function or pointer-to-member
   function type.  TYPE is a pointer type.  Converting EXPR to TYPE is
   not permitted by ISO C++, but we accept it in some modes.  If we
   are not in one of those modes, issue a diagnostic.  Return the
   converted expression.  */

tree
convert_member_func_to_ptr (tree type, tree expr, tsubst_flags_t complain)
{
  tree intype;
  tree decl;

  intype = TREE_TYPE (expr);
  gcc_assert (TYPE_PTRMEMFUNC_P (intype)
	      || TREE_CODE (intype) == METHOD_TYPE);

  if (!(complain & tf_warning_or_error))
    return error_mark_node;

  location_t loc = cp_expr_loc_or_input_loc (expr);

  if (pedantic || warn_pmf2ptr)
    pedwarn (loc, pedantic ? OPT_Wpedantic : OPT_Wpmf_conversions,
	     "converting from %qH to %qI", intype, type);

  STRIP_ANY_LOCATION_WRAPPER (expr);

  if (TREE_CODE (intype) == METHOD_TYPE)
    expr = build_addr_func (expr, complain);
  else if (TREE_CODE (expr) == PTRMEM_CST)
    expr = build_address (PTRMEM_CST_MEMBER (expr));
  else
    {
      decl = maybe_dummy_object (TYPE_PTRMEM_CLASS_TYPE (intype), 0);
      decl = build_address (decl);
      expr = get_member_function_from_ptrfunc (&decl, expr, complain);
    }

  if (expr == error_mark_node)
    return error_mark_node;

  expr = build_nop (type, expr);
  SET_EXPR_LOCATION (expr, loc);
  return expr;
}

/* Build a NOP_EXPR to TYPE, but mark it as a reinterpret_cast so that
   constexpr evaluation knows to reject it.  */

static tree
build_nop_reinterpret (tree type, tree expr)
{
  tree ret = build_nop (type, expr);
  if (ret != expr)
    REINTERPRET_CAST_P (ret) = true;
  return ret;
}

/* Return a representation for a reinterpret_cast from EXPR to TYPE.
   If C_CAST_P is true, this reinterpret cast is being done as part of
   a C-style cast.  If VALID_P is non-NULL, *VALID_P is set to
   indicate whether or not reinterpret_cast was valid.  */

static tree
build_reinterpret_cast_1 (location_t loc, tree type, tree expr,
			  bool c_cast_p, bool *valid_p,
			  tsubst_flags_t complain)
{
  tree intype;

  /* Assume the cast is invalid.  */
  if (valid_p)
    *valid_p = true;

  if (type == error_mark_node || error_operand_p (expr))
    return error_mark_node;

  intype = TREE_TYPE (expr);

  /* Save casted types in the function's used types hash table.  */
  used_types_insert (type);

  /* A prvalue of non-class type is cv-unqualified.  */
  if (!CLASS_TYPE_P (type))
    type = cv_unqualified (type);

  /* [expr.reinterpret.cast]
     A glvalue of type T1, designating an object x, can be cast to the type
     "reference to T2" if an expression of type "pointer to T1" can be
     explicitly converted to the type "pointer to T2" using a reinterpret_cast.
     The result is that of *reinterpret_cast<T2 *>(p) where p is a pointer to x
     of type "pointer to T1". No temporary is created, no copy is made, and no
     constructors (11.4.4) or conversion functions (11.4.7) are called.  */
  if (TYPE_REF_P (type))
    {
      if (!glvalue_p (expr))
	{
          if (complain & tf_error)
	    error_at (loc, "invalid cast of a prvalue expression of type "
		      "%qT to type %qT",
		      intype, type);
	  return error_mark_node;
	}

      /* Warn about a reinterpret_cast from "A*" to "B&" if "A" and
	 "B" are related class types; the reinterpret_cast does not
	 adjust the pointer.  */
      if (TYPE_PTR_P (intype)
          && (complain & tf_warning)
	  && (comptypes (TREE_TYPE (intype), TREE_TYPE (type),
			 COMPARE_BASE | COMPARE_DERIVED)))
	warning_at (loc, 0, "casting %qT to %qT does not dereference pointer",
		    intype, type);

      expr = cp_build_addr_expr (expr, complain);

      if (warn_strict_aliasing > 2)
	cp_strict_aliasing_warning (EXPR_LOCATION (expr), type, expr);

      if (expr != error_mark_node)
	expr = build_reinterpret_cast_1
	  (loc, build_pointer_type (TREE_TYPE (type)), expr, c_cast_p,
	   valid_p, complain);
      if (expr != error_mark_node)
	/* cp_build_indirect_ref isn't right for rvalue refs.  */
	expr = convert_from_reference (fold_convert (type, expr));
      return expr;
    }

  /* As a G++ extension, we consider conversions from member
     functions, and pointers to member functions to
     pointer-to-function and pointer-to-void types.  If
     -Wno-pmf-conversions has not been specified,
     convert_member_func_to_ptr will issue an error message.  */
  if ((TYPE_PTRMEMFUNC_P (intype)
       || TREE_CODE (intype) == METHOD_TYPE)
      && TYPE_PTR_P (type)
      && (TREE_CODE (TREE_TYPE (type)) == FUNCTION_TYPE
	  || VOID_TYPE_P (TREE_TYPE (type))))
    return convert_member_func_to_ptr (type, expr, complain);

  /* If the cast is not to a reference type, the lvalue-to-rvalue,
     array-to-pointer, and function-to-pointer conversions are
     performed.  */
  expr = decay_conversion (expr, complain);

  /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
     Strip such NOP_EXPRs if VALUE is being used in non-lvalue context.  */
  if (TREE_CODE (expr) == NOP_EXPR
      && TREE_TYPE (expr) == TREE_TYPE (TREE_OPERAND (expr, 0)))
    expr = TREE_OPERAND (expr, 0);

  if (error_operand_p (expr))
    return error_mark_node;

  intype = TREE_TYPE (expr);

  /* [expr.reinterpret.cast]
     A pointer can be converted to any integral type large enough to
     hold it. ... A value of type std::nullptr_t can be converted to
     an integral type; the conversion has the same meaning and
     validity as a conversion of (void*)0 to the integral type.  */
  if (CP_INTEGRAL_TYPE_P (type)
      && (TYPE_PTR_P (intype) || NULLPTR_TYPE_P (intype)))
    {
      if (TYPE_PRECISION (type) < TYPE_PRECISION (intype))
        {
          if (complain & tf_error)
            permerror (loc, "cast from %qH to %qI loses precision",
                       intype, type);
          else
            return error_mark_node;
        }
      if (NULLPTR_TYPE_P (intype))
        return build_int_cst (type, 0);
    }
  /* [expr.reinterpret.cast]
     A value of integral or enumeration type can be explicitly
     converted to a pointer.  */
  else if (TYPE_PTR_P (type) && INTEGRAL_OR_ENUMERATION_TYPE_P (intype))
    /* OK */
    ;
  else if ((INTEGRAL_OR_ENUMERATION_TYPE_P (type)
	    || TYPE_PTR_OR_PTRMEM_P (type))
	   && same_type_p (type, intype))
    /* DR 799 */
    return rvalue (expr);
  else if (TYPE_PTRFN_P (type) && TYPE_PTRFN_P (intype))
    {
      if ((complain & tf_warning)
	  && !cxx_safe_function_type_cast_p (TREE_TYPE (type),
					     TREE_TYPE (intype)))
	warning_at (loc, OPT_Wcast_function_type,
		    "cast between incompatible function types"
		    " from %qH to %qI", intype, type);
      return build_nop_reinterpret (type, expr);
    }
  else if (TYPE_PTRMEMFUNC_P (type) && TYPE_PTRMEMFUNC_P (intype))
    {
      if ((complain & tf_warning)
	  && !cxx_safe_function_type_cast_p
		(TREE_TYPE (TYPE_PTRMEMFUNC_FN_TYPE_RAW (type)),
		 TREE_TYPE (TYPE_PTRMEMFUNC_FN_TYPE_RAW (intype))))
	warning_at (loc, OPT_Wcast_function_type,
		    "cast between incompatible pointer to member types"
		    " from %qH to %qI", intype, type);
      return build_nop_reinterpret (type, expr);
    }
  else if ((TYPE_PTRDATAMEM_P (type) && TYPE_PTRDATAMEM_P (intype))
	   || (TYPE_PTROBV_P (type) && TYPE_PTROBV_P (intype)))
    {
      if (!c_cast_p
	  && check_for_casting_away_constness (loc, intype, type,
					       REINTERPRET_CAST_EXPR,
					       complain))
	return error_mark_node;
      /* Warn about possible alignment problems.  */
      if ((STRICT_ALIGNMENT || warn_cast_align == 2)
	  && (complain & tf_warning)
	  && !VOID_TYPE_P (type)
	  && TREE_CODE (TREE_TYPE (intype)) != FUNCTION_TYPE
	  && COMPLETE_TYPE_P (TREE_TYPE (type))
	  && COMPLETE_TYPE_P (TREE_TYPE (intype))
	  && min_align_of_type (TREE_TYPE (type))
	     > min_align_of_type (TREE_TYPE (intype)))
	warning_at (loc, OPT_Wcast_align, "cast from %qH to %qI "
		    "increases required alignment of target type",
		    intype, type);

      if (warn_strict_aliasing <= 2)
	/* strict_aliasing_warning STRIP_NOPs its expr.  */
	cp_strict_aliasing_warning (EXPR_LOCATION (expr), type, expr);

      return build_nop_reinterpret (type, expr);
    }
  else if ((TYPE_PTRFN_P (type) && TYPE_PTROBV_P (intype))
	   || (TYPE_PTRFN_P (intype) && TYPE_PTROBV_P (type)))
    {
      if (complain & tf_warning)
	/* C++11 5.2.10 p8 says that "Converting a function pointer to an
	   object pointer type or vice versa is conditionally-supported."  */
	warning_at (loc, OPT_Wconditionally_supported,
		    "casting between pointer-to-function and "
		    "pointer-to-object is conditionally-supported");
      return build_nop_reinterpret (type, expr);
    }
  else if (gnu_vector_type_p (type) && scalarish_type_p (intype))
    return convert_to_vector (type, rvalue (expr));
  else if (gnu_vector_type_p (intype)
	   && INTEGRAL_OR_ENUMERATION_TYPE_P (type))
    return convert_to_integer_nofold (type, expr);
  else
    {
      if (valid_p)
	*valid_p = false;
      if (complain & tf_error)
        error_at (loc, "invalid cast from type %qT to type %qT",
		  intype, type);
      return error_mark_node;
    }

  expr = cp_convert (type, expr, complain);
  if (TREE_CODE (expr) == NOP_EXPR)
    /* Mark any nop_expr that created as a reintepret_cast.  */
    REINTERPRET_CAST_P (expr) = true;
  return expr;
}

tree
build_reinterpret_cast (location_t loc, tree type, tree expr,
			tsubst_flags_t complain)
{
  tree r;

  if (type == error_mark_node || expr == error_mark_node)
    return error_mark_node;

  if (processing_template_decl)
    {
      tree t = build_min (REINTERPRET_CAST_EXPR, type, expr);

      if (!TREE_SIDE_EFFECTS (t)
	  && type_dependent_expression_p (expr))
	/* There might turn out to be side effects inside expr.  */
	TREE_SIDE_EFFECTS (t) = 1;
      r = convert_from_reference (t);
      protected_set_expr_location (r, loc);
      return r;
    }

  r = build_reinterpret_cast_1 (loc, type, expr, /*c_cast_p=*/false,
				/*valid_p=*/NULL, complain);
  if (r != error_mark_node)
    {
      maybe_warn_about_useless_cast (loc, type, expr, complain);
      maybe_warn_about_cast_ignoring_quals (loc, type, complain);
    }
  protected_set_expr_location (r, loc);
  return r;
}

/* Perform a const_cast from EXPR to TYPE.  If the cast is valid,
   return an appropriate expression.  Otherwise, return
   error_mark_node.  If the cast is not valid, and COMPLAIN is true,
   then a diagnostic will be issued.  If VALID_P is non-NULL, we are
   performing a C-style cast, its value upon return will indicate
   whether or not the conversion succeeded.  */

static tree
build_const_cast_1 (location_t loc, tree dst_type, tree expr,
		    tsubst_flags_t complain, bool *valid_p)
{
  tree src_type;
  tree reference_type;

  /* Callers are responsible for handling error_mark_node as a
     destination type.  */
  gcc_assert (dst_type != error_mark_node);
  /* In a template, callers should be building syntactic
     representations of casts, not using this machinery.  */
  gcc_assert (!processing_template_decl);

  /* Assume the conversion is invalid.  */
  if (valid_p)
    *valid_p = false;

  if (!INDIRECT_TYPE_P (dst_type) && !TYPE_PTRDATAMEM_P (dst_type))
    {
      if (complain & tf_error)
	error_at (loc, "invalid use of %<const_cast%> with type %qT, "
		  "which is not a pointer, reference, "
		  "nor a pointer-to-data-member type", dst_type);
      return error_mark_node;
    }

  if (TREE_CODE (TREE_TYPE (dst_type)) == FUNCTION_TYPE)
    {
      if (complain & tf_error)
	error_at (loc, "invalid use of %<const_cast%> with type %qT, "
		  "which is a pointer or reference to a function type",
		  dst_type);
       return error_mark_node;
    }

  /* A prvalue of non-class type is cv-unqualified.  */
  dst_type = cv_unqualified (dst_type);

  /* Save casted types in the function's used types hash table.  */
  used_types_insert (dst_type);

  src_type = TREE_TYPE (expr);
  /* Expressions do not really have reference types.  */
  if (TYPE_REF_P (src_type))
    src_type = TREE_TYPE (src_type);

  /* [expr.const.cast]

     For two object types T1 and T2, if a pointer to T1 can be explicitly
     converted to the type "pointer to T2" using a const_cast, then the
     following conversions can also be made:

     -- an lvalue of type T1 can be explicitly converted to an lvalue of
     type T2 using the cast const_cast<T2&>;

     -- a glvalue of type T1 can be explicitly converted to an xvalue of
     type T2 using the cast const_cast<T2&&>; and

     -- if T1 is a class type, a prvalue of type T1 can be explicitly
     converted to an xvalue of type T2 using the cast const_cast<T2&&>.  */

  if (TYPE_REF_P (dst_type))
    {
      reference_type = dst_type;
      if (!TYPE_REF_IS_RVALUE (dst_type)
	  ? lvalue_p (expr)
	  : obvalue_p (expr))
	/* OK.  */;
      else
	{
	  if (complain & tf_error)
	    error_at (loc, "invalid %<const_cast%> of an rvalue of type %qT "
		      "to type %qT",
		      src_type, dst_type);
 	  return error_mark_node;
	}
      dst_type = build_pointer_type (TREE_TYPE (dst_type));
      src_type = build_pointer_type (src_type);
    }
  else
    {
      reference_type = NULL_TREE;
      /* If the destination type is not a reference type, the
	 lvalue-to-rvalue, array-to-pointer, and function-to-pointer
	 conversions are performed.  */
      src_type = type_decays_to (src_type);
      if (src_type == error_mark_node)
	return error_mark_node;
    }

  if (TYPE_PTR_P (src_type) || TYPE_PTRDATAMEM_P (src_type))
    {
      if (comp_ptr_ttypes_const (dst_type, src_type, bounds_none))
	{
	  if (valid_p)
	    {
	      *valid_p = true;
	      /* This cast is actually a C-style cast.  Issue a warning if
		 the user is making a potentially unsafe cast.  */
	      check_for_casting_away_constness (loc, src_type, dst_type,
						CAST_EXPR, complain);
	      /* ??? comp_ptr_ttypes_const ignores TYPE_ALIGN.  */
	      if ((STRICT_ALIGNMENT || warn_cast_align == 2)
		  && (complain & tf_warning)
		  && min_align_of_type (TREE_TYPE (dst_type))
		     > min_align_of_type (TREE_TYPE (src_type)))
		warning_at (loc, OPT_Wcast_align, "cast from %qH to %qI "
			    "increases required alignment of target type",
			    src_type, dst_type);
	    }
	  if (reference_type)
	    {
	      expr = cp_build_addr_expr (expr, complain);
	      if (expr == error_mark_node)
		return error_mark_node;
	      expr = build_nop (reference_type, expr);
	      return convert_from_reference (expr);
	    }
	  else
	    {
	      expr = decay_conversion (expr, complain);
	      if (expr == error_mark_node)
		return error_mark_node;

	      /* build_c_cast puts on a NOP_EXPR to make the result not an
		 lvalue.  Strip such NOP_EXPRs if VALUE is being used in
		 non-lvalue context.  */
	      if (TREE_CODE (expr) == NOP_EXPR
		  && TREE_TYPE (expr) == TREE_TYPE (TREE_OPERAND (expr, 0)))
		expr = TREE_OPERAND (expr, 0);
	      return build_nop (dst_type, expr);
	    }
	}
      else if (valid_p
	       && !at_least_as_qualified_p (TREE_TYPE (dst_type),
					    TREE_TYPE (src_type)))
	check_for_casting_away_constness (loc, src_type, dst_type,
					  CAST_EXPR, complain);
    }

  if (complain & tf_error)
    error_at (loc, "invalid %<const_cast%> from type %qT to type %qT",
	      src_type, dst_type);
  return error_mark_node;
}

tree
build_const_cast (location_t loc, tree type, tree expr,
		  tsubst_flags_t complain)
{
  tree r;

  if (type == error_mark_node || error_operand_p (expr))
    return error_mark_node;

  if (processing_template_decl)
    {
      tree t = build_min (CONST_CAST_EXPR, type, expr);

      if (!TREE_SIDE_EFFECTS (t)
	  && type_dependent_expression_p (expr))
	/* There might turn out to be side effects inside expr.  */
	TREE_SIDE_EFFECTS (t) = 1;
      r = convert_from_reference (t);
      protected_set_expr_location (r, loc);
      return r;
    }

  r = build_const_cast_1 (loc, type, expr, complain, /*valid_p=*/NULL);
  if (r != error_mark_node)
    {
      maybe_warn_about_useless_cast (loc, type, expr, complain);
      maybe_warn_about_cast_ignoring_quals (loc, type, complain);
    }
  protected_set_expr_location (r, loc);
  return r;
}

/* Like cp_build_c_cast, but for the c-common bits.  */

tree
build_c_cast (location_t loc, tree type, tree expr)
{
  return cp_build_c_cast (loc, type, expr, tf_warning_or_error);
}

/* Like the "build_c_cast" used for c-common, but using cp_expr to
   preserve location information even for tree nodes that don't
   support it.  */

cp_expr
build_c_cast (location_t loc, tree type, cp_expr expr)
{
  cp_expr result = cp_build_c_cast (loc, type, expr, tf_warning_or_error);
  result.set_location (loc);
  return result;
}

/* Build an expression representing an explicit C-style cast to type
   TYPE of expression EXPR.  */

tree
cp_build_c_cast (location_t loc, tree type, tree expr,
		 tsubst_flags_t complain)
{
  tree value = expr;
  tree result;
  bool valid_p;

  if (type == error_mark_node || error_operand_p (expr))
    return error_mark_node;

  if (processing_template_decl)
    {
      tree t = build_min (CAST_EXPR, type,
			  tree_cons (NULL_TREE, value, NULL_TREE));
      /* We don't know if it will or will not have side effects.  */
      TREE_SIDE_EFFECTS (t) = 1;
      return convert_from_reference (t);
    }

  /* Casts to a (pointer to a) specific ObjC class (or 'id' or
     'Class') should always be retained, because this information aids
     in method lookup.  */
  if (objc_is_object_ptr (type)
      && objc_is_object_ptr (TREE_TYPE (expr)))
    return build_nop (type, expr);

  /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
     Strip such NOP_EXPRs if VALUE is being used in non-lvalue context.  */
  if (!TYPE_REF_P (type)
      && TREE_CODE (value) == NOP_EXPR
      && TREE_TYPE (value) == TREE_TYPE (TREE_OPERAND (value, 0)))
    value = TREE_OPERAND (value, 0);

  if (TREE_CODE (type) == ARRAY_TYPE)
    {
      /* Allow casting from T1* to T2[] because Cfront allows it.
	 NIHCL uses it. It is not valid ISO C++ however.  */
      if (TYPE_PTR_P (TREE_TYPE (expr)))
	{
          if (complain & tf_error)
            permerror (loc, "ISO C++ forbids casting to an array type %qT",
		       type);
          else
            return error_mark_node;
	  type = build_pointer_type (TREE_TYPE (type));
	}
      else
	{
          if (complain & tf_error)
            error_at (loc, "ISO C++ forbids casting to an array type %qT",
		      type);
	  return error_mark_node;
	}
    }

  if (FUNC_OR_METHOD_TYPE_P (type))
    {
      if (complain & tf_error)
        error_at (loc, "invalid cast to function type %qT", type);
      return error_mark_node;
    }

  if (TYPE_PTR_P (type)
      && TREE_CODE (TREE_TYPE (value)) == INTEGER_TYPE
      /* Casting to an integer of smaller size is an error detected elsewhere.  */
      && TYPE_PRECISION (type) > TYPE_PRECISION (TREE_TYPE (value))
      /* Don't warn about converting any constant.  */
      && !TREE_CONSTANT (value))
    warning_at (loc, OPT_Wint_to_pointer_cast,
		"cast to pointer from integer of different size");

  /* A C-style cast can be a const_cast.  */
  result = build_const_cast_1 (loc, type, value, complain & tf_warning,
			       &valid_p);
  if (valid_p)
    {
      if (result != error_mark_node)
	{
	  maybe_warn_about_useless_cast (loc, type, value, complain);
	  maybe_warn_about_cast_ignoring_quals (loc, type, complain);
	}
      else if (complain & tf_error)
	build_const_cast_1 (loc, type, value, tf_error, &valid_p);
      return result;
    }

  /* Or a static cast.  */
  result = build_static_cast_1 (loc, type, value, /*c_cast_p=*/true,
				&valid_p, complain);
  /* Or a reinterpret_cast.  */
  if (!valid_p)
    result = build_reinterpret_cast_1 (loc, type, value, /*c_cast_p=*/true,
				       &valid_p, complain);
  /* The static_cast or reinterpret_cast may be followed by a
     const_cast.  */
  if (valid_p
      /* A valid cast may result in errors if, for example, a
	 conversion to an ambiguous base class is required.  */
      && !error_operand_p (result))
    {
      tree result_type;

      maybe_warn_about_useless_cast (loc, type, value, complain);
      maybe_warn_about_cast_ignoring_quals (loc, type, complain);

      /* Non-class rvalues always have cv-unqualified type.  */
      if (!CLASS_TYPE_P (type))
	type = TYPE_MAIN_VARIANT (type);
      result_type = TREE_TYPE (result);
      if (!CLASS_TYPE_P (result_type) && !TYPE_REF_P (type))
	result_type = TYPE_MAIN_VARIANT (result_type);
      /* If the type of RESULT does not match TYPE, perform a
	 const_cast to make it match.  If the static_cast or
	 reinterpret_cast succeeded, we will differ by at most
	 cv-qualification, so the follow-on const_cast is guaranteed
	 to succeed.  */
      if (!same_type_p (non_reference (type), non_reference (result_type)))
	{
	  result = build_const_cast_1 (loc, type, result, tf_none, &valid_p);
	  gcc_assert (valid_p);
	}
      return result;
    }

  return error_mark_node;
}

/* Warn when a value is moved to itself with std::move.  LHS is the target,
   RHS may be the std::move call, and LOC is the location of the whole
   assignment.  Return true if we warned.  */

bool
maybe_warn_self_move (location_t loc, tree lhs, tree rhs)
{
  if (!warn_self_move)
    return false;

  /* C++98 doesn't know move.  */
  if (cxx_dialect < cxx11)
    return false;

  if (processing_template_decl)
    return false;

  if (!REFERENCE_REF_P (rhs)
      || TREE_CODE (TREE_OPERAND (rhs, 0)) != CALL_EXPR)
    return false;
  tree fn = TREE_OPERAND (rhs, 0);
  if (!is_std_move_p (fn))
    return false;

  /* Just a little helper to strip * and various NOPs.  */
  auto extract_op = [] (tree &op) {
    STRIP_NOPS (op);
    while (INDIRECT_REF_P (op))
      op = TREE_OPERAND (op, 0);
    op = maybe_undo_parenthesized_ref (op);
    STRIP_ANY_LOCATION_WRAPPER (op);
  };

  tree arg = CALL_EXPR_ARG (fn, 0);
  extract_op (arg);
  if (TREE_CODE (arg) == ADDR_EXPR)
    arg = TREE_OPERAND (arg, 0);
  tree type = TREE_TYPE (lhs);
  tree orig_lhs = lhs;
  extract_op (lhs);
  if (cp_tree_equal (lhs, arg)
      /* Also warn in a member-initializer-list, as in : i(std::move(i)).  */
      || (TREE_CODE (lhs) == FIELD_DECL
	  && TREE_CODE (arg) == COMPONENT_REF
	  && cp_tree_equal (TREE_OPERAND (arg, 0), current_class_ref)
	  && TREE_OPERAND (arg, 1) == lhs))
    if (warning_at (loc, OPT_Wself_move,
		    "moving %qE of type %qT to itself", orig_lhs, type))
      return true;

  return false;
}

/* For use from the C common bits.  */
tree
build_modify_expr (location_t location,
		   tree lhs, tree /*lhs_origtype*/,
		   enum tree_code modifycode,
		   location_t /*rhs_location*/, tree rhs,
		   tree /*rhs_origtype*/)
{
  return cp_build_modify_expr (location, lhs, modifycode, rhs,
			       tf_warning_or_error);
}

/* Build an assignment expression of lvalue LHS from value RHS.
   MODIFYCODE is the code for a binary operator that we use
   to combine the old value of LHS with RHS to get the new value.
   Or else MODIFYCODE is NOP_EXPR meaning do a simple assignment.

   C++: If MODIFYCODE is INIT_EXPR, then leave references unbashed.  */

tree
cp_build_modify_expr (location_t loc, tree lhs, enum tree_code modifycode,
		      tree rhs, tsubst_flags_t complain)
{
  lhs = mark_lvalue_use_nonread (lhs);

  tree result = NULL_TREE;
  tree newrhs = rhs;
  tree lhstype = TREE_TYPE (lhs);
  tree olhs = lhs;
  tree olhstype = lhstype;
  bool plain_assign = (modifycode == NOP_EXPR);
  bool compound_side_effects_p = false;
  tree preeval = NULL_TREE;

  /* Avoid duplicate error messages from operands that had errors.  */
  if (error_operand_p (lhs) || error_operand_p (rhs))
    return error_mark_node;

  while (TREE_CODE (lhs) == COMPOUND_EXPR)
    {
      if (TREE_SIDE_EFFECTS (TREE_OPERAND (lhs, 0)))
	compound_side_effects_p = true;
      lhs = TREE_OPERAND (lhs, 1);
    }

  /* Handle control structure constructs used as "lvalues".  Note that we
     leave COMPOUND_EXPR on the LHS because it is sequenced after the RHS.  */
  switch (TREE_CODE (lhs))
    {
      /* Handle --foo = 5; as these are valid constructs in C++.  */
    case PREDECREMENT_EXPR:
    case PREINCREMENT_EXPR:
      if (compound_side_effects_p)
	newrhs = rhs = stabilize_expr (rhs, &preeval);
      lhs = genericize_compound_lvalue (lhs);
    maybe_add_compound:
      /* If we had (bar, --foo) = 5; or (bar, (baz, --foo)) = 5;
	 and looked through the COMPOUND_EXPRs, readd them now around
	 the resulting lhs.  */
      if (TREE_CODE (olhs) == COMPOUND_EXPR)
	{
	  lhs = build2 (COMPOUND_EXPR, lhstype, TREE_OPERAND (olhs, 0), lhs);
	  tree *ptr = &TREE_OPERAND (lhs, 1);
	  for (olhs = TREE_OPERAND (olhs, 1);
	       TREE_CODE (olhs) == COMPOUND_EXPR;
	       olhs = TREE_OPERAND (olhs, 1))
	    {
	      *ptr = build2 (COMPOUND_EXPR, lhstype,
			     TREE_OPERAND (olhs, 0), *ptr);
	      ptr = &TREE_OPERAND (*ptr, 1);
	    }
	}
      break;

    case MODIFY_EXPR:
      if (compound_side_effects_p)
	newrhs = rhs = stabilize_expr (rhs, &preeval);
      lhs = genericize_compound_lvalue (lhs);
      goto maybe_add_compound;

    case MIN_EXPR:
    case MAX_EXPR:
      /* MIN_EXPR and MAX_EXPR are currently only permitted as lvalues,
	 when neither operand has side-effects.  */
      if (!lvalue_or_else (lhs, lv_assign, complain))
	return error_mark_node;

      gcc_assert (!TREE_SIDE_EFFECTS (TREE_OPERAND (lhs, 0))
		  && !TREE_SIDE_EFFECTS (TREE_OPERAND (lhs, 1)));

      lhs = build3 (COND_EXPR, TREE_TYPE (lhs),
		    build2 (TREE_CODE (lhs) == MIN_EXPR ? LE_EXPR : GE_EXPR,
			    boolean_type_node,
			    TREE_OPERAND (lhs, 0),
			    TREE_OPERAND (lhs, 1)),
		    TREE_OPERAND (lhs, 0),
		    TREE_OPERAND (lhs, 1));
      gcc_fallthrough ();

      /* Handle (a ? b : c) used as an "lvalue".  */
    case COND_EXPR:
      {
	/* Produce (a ? (b = rhs) : (c = rhs))
	   except that the RHS goes through a save-expr
	   so the code to compute it is only emitted once.  */
	if (VOID_TYPE_P (TREE_TYPE (rhs)))
	  {
	    if (complain & tf_error)
	      error_at (cp_expr_loc_or_loc (rhs, loc),
			"void value not ignored as it ought to be");
	    return error_mark_node;
	  }

	rhs = stabilize_expr (rhs, &preeval);

	/* Check this here to avoid odd errors when trying to convert
	   a throw to the type of the COND_EXPR.  */
	if (!lvalue_or_else (lhs, lv_assign, complain))
	  return error_mark_node;

	tree op1 = TREE_OPERAND (lhs, 1);
	if (TREE_CODE (op1) != THROW_EXPR)
	  op1 = cp_build_modify_expr (loc, op1, modifycode, rhs, complain);
	/* When sanitizing undefined behavior, even when rhs doesn't need
	   stabilization at this point, the sanitization might add extra
	   SAVE_EXPRs in there and so make sure there is no tree sharing
	   in the rhs, otherwise those SAVE_EXPRs will have initialization
	   only in one of the two branches.  */
	if (sanitize_flags_p (SANITIZE_UNDEFINED
			      | SANITIZE_UNDEFINED_NONDEFAULT))
	  rhs = unshare_expr (rhs);
	tree op2 = TREE_OPERAND (lhs, 2);
	if (TREE_CODE (op2) != THROW_EXPR)
	  op2 = cp_build_modify_expr (loc, op2, modifycode, rhs, complain);
	tree cond = build_conditional_expr (input_location,
					    TREE_OPERAND (lhs, 0), op1, op2,
					    complain);

	if (cond == error_mark_node)
	  return cond;
	/* If we had (e, (a ? b : c)) = d; or (e, (f, (a ? b : c))) = d;
	   and looked through the COMPOUND_EXPRs, readd them now around
	   the resulting cond before adding the preevaluated rhs.  */
	if (TREE_CODE (olhs) == COMPOUND_EXPR)
	  {
	    cond = build2 (COMPOUND_EXPR, TREE_TYPE (cond),
			   TREE_OPERAND (olhs, 0), cond);
	    tree *ptr = &TREE_OPERAND (cond, 1);
	    for (olhs = TREE_OPERAND (olhs, 1);
		 TREE_CODE (olhs) == COMPOUND_EXPR;
		 olhs = TREE_OPERAND (olhs, 1))
	      {
		*ptr = build2 (COMPOUND_EXPR, TREE_TYPE (cond),
			       TREE_OPERAND (olhs, 0), *ptr);
		ptr = &TREE_OPERAND (*ptr, 1);
	      }
	  }
	/* Make sure the code to compute the rhs comes out
	   before the split.  */
	result = cond;
	goto ret;
      }

    default:
      lhs = olhs;
      break;
    }

  if (modifycode == INIT_EXPR)
    {
      if (BRACE_ENCLOSED_INITIALIZER_P (rhs))
	/* Do the default thing.  */;
      else if (TREE_CODE (rhs) == CONSTRUCTOR)
	{
	  /* Compound literal.  */
	  if (! same_type_p (TREE_TYPE (rhs), lhstype))
	    /* Call convert to generate an error; see PR 11063.  */
	    rhs = convert (lhstype, rhs);
	  result = cp_build_init_expr (lhs, rhs);
	  TREE_SIDE_EFFECTS (result) = 1;
	  goto ret;
	}
      else if (! MAYBE_CLASS_TYPE_P (lhstype))
	/* Do the default thing.  */;
      else
	{
	  releasing_vec rhs_vec = make_tree_vector_single (rhs);
	  result = build_special_member_call (lhs, complete_ctor_identifier,
					      &rhs_vec, lhstype, LOOKUP_NORMAL,
                                              complain);
	  if (result == NULL_TREE)
	    return error_mark_node;
	  goto ret;
	}
    }
  else
    {
      lhs = require_complete_type (lhs, complain);
      if (lhs == error_mark_node)
	return error_mark_node;

      if (modifycode == NOP_EXPR)
	{
	  maybe_warn_self_move (loc, lhs, rhs);

	  if (c_dialect_objc ())
	    {
	      result = objc_maybe_build_modify_expr (lhs, rhs);
	      if (result)
		goto ret;
	    }

	  /* `operator=' is not an inheritable operator.  */
	  if (! MAYBE_CLASS_TYPE_P (lhstype))
	    /* Do the default thing.  */;
	  else
	    {
	      result = build_new_op (input_location, MODIFY_EXPR,
				     LOOKUP_NORMAL, lhs, rhs,
				     make_node (NOP_EXPR), NULL_TREE,
				     /*overload=*/NULL, complain);
	      if (result == NULL_TREE)
		return error_mark_node;
	      goto ret;
	    }
	  lhstype = olhstype;
	}
      else
	{
	  tree init = NULL_TREE;

	  /* A binary op has been requested.  Combine the old LHS
	     value with the RHS producing the value we should actually
	     store into the LHS.  */
	  gcc_assert (!((TYPE_REF_P (lhstype)
			 && MAYBE_CLASS_TYPE_P (TREE_TYPE (lhstype)))
			|| MAYBE_CLASS_TYPE_P (lhstype)));

	  /* Preevaluate the RHS to make sure its evaluation is complete
	     before the lvalue-to-rvalue conversion of the LHS:

	     [expr.ass] With respect to an indeterminately-sequenced
	     function call, the operation of a compound assignment is a
	     single evaluation. [ Note: Therefore, a function call shall
	     not intervene between the lvalue-to-rvalue conversion and the
	     side effect associated with any single compound assignment
	     operator. -- end note ]  */
	  lhs = cp_stabilize_reference (lhs);
	  rhs = decay_conversion (rhs, complain);
	  if (rhs == error_mark_node)
	    return error_mark_node;

	  {
	    auto_diagnostic_group d;
	    rhs = stabilize_expr (rhs, &init);
	    bool clear_decl_read = false;
	    tree stripped_lhs = tree_strip_any_location_wrapper (lhs);
	    if ((VAR_P (stripped_lhs) || TREE_CODE (stripped_lhs) == PARM_DECL)
		&& !DECL_READ_P (stripped_lhs)
		&& (VAR_P (stripped_lhs) ? warn_unused_but_set_variable
					 : warn_unused_but_set_parameter) > 2
		&& !CLASS_TYPE_P (TREE_TYPE (lhs))
		&& !CLASS_TYPE_P (TREE_TYPE (rhs)))
	      {
		mark_exp_read (rhs);
		if (!DECL_READ_P (stripped_lhs))
		  clear_decl_read = true;
	      }
	    newrhs = cp_build_binary_op (loc, modifycode, lhs, rhs, complain);
	    if (clear_decl_read)
	      DECL_READ_P (stripped_lhs) = 0;
	    if (newrhs == error_mark_node)
	      {
		if (complain & tf_error)
		  inform (loc, "  in evaluation of %<%Q(%#T, %#T)%>",
			  modifycode, TREE_TYPE (lhs), TREE_TYPE (rhs));
		return error_mark_node;
	      }
	  }

	  if (init)
	    newrhs = build2 (COMPOUND_EXPR, TREE_TYPE (newrhs), init, newrhs);

	  /* Now it looks like a plain assignment.  */
	  modifycode = NOP_EXPR;
	  if (c_dialect_objc ())
	    {
	      result = objc_maybe_build_modify_expr (lhs, newrhs);
	      if (result)
		goto ret;
	    }
	}
      gcc_assert (!TYPE_REF_P (lhstype));
      gcc_assert (!TYPE_REF_P (TREE_TYPE (newrhs)));
    }

  /* The left-hand side must be an lvalue.  */
  if (!lvalue_or_else (lhs, lv_assign, complain))
    return error_mark_node;

  /* Warn about modifying something that is `const'.  Don't warn if
     this is initialization.  */
  if (modifycode != INIT_EXPR
      && (TREE_READONLY (lhs) || CP_TYPE_CONST_P (lhstype)
	  /* Functions are not modifiable, even though they are
	     lvalues.  */
	  || FUNC_OR_METHOD_TYPE_P (TREE_TYPE (lhs))
	  /* If it's an aggregate and any field is const, then it is
	     effectively const.  */
	  || (CLASS_TYPE_P (lhstype)
	      && C_TYPE_FIELDS_READONLY (lhstype))))
    {
      if (complain & tf_error)
	cxx_readonly_error (loc, lhs, lv_assign);
      return error_mark_node;
    }

  /* If storing into a structure or union member, it may have been given a
     lowered bitfield type.  We need to convert to the declared type first,
     so retrieve it now.  */

  olhstype = unlowered_expr_type (lhs);

  /* Convert new value to destination type.  */

  if (TREE_CODE (lhstype) == ARRAY_TYPE)
    {
      int from_array;

      if (BRACE_ENCLOSED_INITIALIZER_P (newrhs))
	{
	  if (modifycode != INIT_EXPR)
	    {
	      if (complain & tf_error)
		error_at (loc,
			  "assigning to an array from an initializer list");
	      return error_mark_node;
	    }
	  if (check_array_initializer (lhs, lhstype, newrhs))
	    return error_mark_node;
	  newrhs = digest_init (lhstype, newrhs, complain);
	  if (newrhs == error_mark_node)
	    return error_mark_node;
	}

      /* C++11 8.5/17: "If the destination type is an array of characters,
	 an array of char16_t, an array of char32_t, or an array of wchar_t,
	 and the initializer is a string literal...".  */
      else if ((TREE_CODE (tree_strip_any_location_wrapper (newrhs))
		== STRING_CST)
	       && char_type_p (TREE_TYPE (TYPE_MAIN_VARIANT (lhstype)))
	       && modifycode == INIT_EXPR)
	{
	  newrhs = digest_init (lhstype, newrhs, complain);
	  if (newrhs == error_mark_node)
	    return error_mark_node;
	}

      else if (!same_or_base_type_p (TYPE_MAIN_VARIANT (lhstype),
				     TYPE_MAIN_VARIANT (TREE_TYPE (newrhs))))
	{
	  if (complain & tf_error)
	    error_at (loc, "incompatible types in assignment of %qT to %qT",
		      TREE_TYPE (rhs), lhstype);
	  return error_mark_node;
	}

      /* Allow array assignment in compiler-generated code.  */
      else if (DECL_P (lhs) && DECL_ARTIFICIAL (lhs))
	/* OK, used by coroutines (co-await-initlist1.C).  */;
      else if (!current_function_decl
	       || !DECL_DEFAULTED_FN (current_function_decl))
	{
          /* This routine is used for both initialization and assignment.
             Make sure the diagnostic message differentiates the context.  */
	  if (complain & tf_error)
	    {
	      if (modifycode == INIT_EXPR)
		error_at (loc, "array used as initializer");
	      else
		error_at (loc, "invalid array assignment");
	    }
	  return error_mark_node;
	}

      from_array = TREE_CODE (TREE_TYPE (newrhs)) == ARRAY_TYPE
		   ? 1 + (modifycode != INIT_EXPR) : 0;
      result = build_vec_init (lhs, NULL_TREE, newrhs,
			       /*explicit_value_init_p=*/false,
			       from_array, complain);
      goto ret;
    }

  if (modifycode == INIT_EXPR)
    /* Calls with INIT_EXPR are all direct-initialization, so don't set
       LOOKUP_ONLYCONVERTING.  */
    newrhs = convert_for_initialization (lhs, olhstype, newrhs, LOOKUP_NORMAL,
					 ICR_INIT, NULL_TREE, 0,
					 complain | tf_no_cleanup);
  else
    newrhs = convert_for_assignment (olhstype, newrhs, ICR_ASSIGN,
				     NULL_TREE, 0, complain, LOOKUP_IMPLICIT);

  if (!same_type_p (lhstype, olhstype))
    newrhs = cp_convert_and_check (lhstype, newrhs, complain);

  if (modifycode != INIT_EXPR)
    {
      if (TREE_CODE (newrhs) == CALL_EXPR
	  && TYPE_NEEDS_CONSTRUCTING (lhstype))
	newrhs = build_cplus_new (lhstype, newrhs, complain);

      /* Can't initialize directly from a TARGET_EXPR, since that would
	 cause the lhs to be constructed twice, and possibly result in
	 accidental self-initialization.  So we force the TARGET_EXPR to be
	 expanded without a target.  */
      if (TREE_CODE (newrhs) == TARGET_EXPR)
	newrhs = build2 (COMPOUND_EXPR, TREE_TYPE (newrhs), newrhs,
			 TARGET_EXPR_SLOT (newrhs));
    }

  if (newrhs == error_mark_node)
    return error_mark_node;

  if (c_dialect_objc () && flag_objc_gc)
    {
      result = objc_generate_write_barrier (lhs, modifycode, newrhs);

      if (result)
	goto ret;
    }

  result = build2_loc (loc, modifycode == NOP_EXPR ? MODIFY_EXPR : INIT_EXPR,
		       lhstype, lhs, newrhs);
  if (modifycode == INIT_EXPR)
    set_target_expr_eliding (newrhs);

  TREE_SIDE_EFFECTS (result) = 1;
  if (!plain_assign)
    suppress_warning (result, OPT_Wparentheses);

 ret:
  if (preeval)
    result = build2 (COMPOUND_EXPR, TREE_TYPE (result), preeval, result);
  return result;
}

cp_expr
build_x_modify_expr (location_t loc, tree lhs, enum tree_code modifycode,
		     tree rhs, tree lookups, tsubst_flags_t complain)
{
  tree orig_lhs = lhs;
  tree orig_rhs = rhs;
  tree overload = NULL_TREE;

  if (lhs == error_mark_node || rhs == error_mark_node)
    return cp_expr (error_mark_node, loc);

  tree op = build_min (modifycode, void_type_node, NULL_TREE, NULL_TREE);

  if (processing_template_decl)
    {
      if (type_dependent_expression_p (lhs)
	  || type_dependent_expression_p (rhs))
	{
	  tree rval = build_min_nt_loc (loc, MODOP_EXPR, lhs, op, rhs);
	  if (modifycode != NOP_EXPR)
	    TREE_TYPE (rval)
	      = build_dependent_operator_type (lookups, modifycode, true);
	  return rval;
	}
    }

  tree rval;
  if (modifycode == NOP_EXPR)
    rval = cp_build_modify_expr (loc, lhs, modifycode, rhs, complain);
  else
    rval = build_new_op (loc, MODIFY_EXPR, LOOKUP_NORMAL,
			 lhs, rhs, op, lookups, &overload, complain);
  if (rval == error_mark_node)
    return error_mark_node;
  if (processing_template_decl)
    {
      if (overload != NULL_TREE)
	return (build_min_non_dep_op_overload
		(MODIFY_EXPR, rval, overload, orig_lhs, orig_rhs));

      return (build_min_non_dep
	      (MODOP_EXPR, rval, orig_lhs, op, orig_rhs));
    }
  return rval;
}

/* Helper function for get_delta_difference which assumes FROM is a base
   class of TO.  Returns a delta for the conversion of pointer-to-member
   of FROM to pointer-to-member of TO.  If the conversion is invalid and
   tf_error is not set in COMPLAIN returns error_mark_node, otherwise
   returns zero.  If FROM is not a base class of TO, returns NULL_TREE.
   If C_CAST_P is true, this conversion is taking place as part of a
   C-style cast.  */

static tree
get_delta_difference_1 (tree from, tree to, bool c_cast_p,
			tsubst_flags_t complain)
{
  tree binfo;
  base_kind kind;

  binfo = lookup_base (to, from, c_cast_p ? ba_unique : ba_check,
		       &kind, complain);

  if (binfo == error_mark_node)
    {
      if (!(complain & tf_error))
	return error_mark_node;

      inform (input_location, "   in pointer to member function conversion");
      return size_zero_node;
    }
  else if (binfo)
    {
      if (kind != bk_via_virtual)
	return BINFO_OFFSET (binfo);
      else
	/* FROM is a virtual base class of TO.  Issue an error or warning
	   depending on whether or not this is a reinterpret cast.  */
	{
	  if (!(complain & tf_error))
	    return error_mark_node;

	  error ("pointer to member conversion via virtual base %qT",
		 BINFO_TYPE (binfo_from_vbase (binfo)));

	  return size_zero_node;
	}
      }
  else
    return NULL_TREE;
}

/* Get difference in deltas for different pointer to member function
   types.  If the conversion is invalid and tf_error is not set in
   COMPLAIN, returns error_mark_node, otherwise returns an integer
   constant of type PTRDIFF_TYPE_NODE and its value is zero if the
   conversion is invalid.  If ALLOW_INVERSE_P is true, then allow reverse
   conversions as well.  If C_CAST_P is true this conversion is taking
   place as part of a C-style cast.

   Note that the naming of FROM and TO is kind of backwards; the return
   value is what we add to a TO in order to get a FROM.  They are named
   this way because we call this function to find out how to convert from
   a pointer to member of FROM to a pointer to member of TO.  */

static tree
get_delta_difference (tree from, tree to,
		      bool allow_inverse_p,
		      bool c_cast_p, tsubst_flags_t complain)
{
  auto_diagnostic_group d;
  tree result;

  if (same_type_ignoring_top_level_qualifiers_p (from, to))
    /* Pointer to member of incomplete class is permitted*/
    result = size_zero_node;
  else
    result = get_delta_difference_1 (from, to, c_cast_p, complain);

  if (result == error_mark_node)
    return error_mark_node;

  if (!result)
  {
    if (!allow_inverse_p)
      {
	if (!(complain & tf_error))
	  return error_mark_node;

	error_not_base_type (from, to);
	inform (input_location, "   in pointer to member conversion");
      	result = size_zero_node;
      }
    else
      {
	result = get_delta_difference_1 (to, from, c_cast_p, complain);

	if (result == error_mark_node)
	  return error_mark_node;

	if (result)
	  result = size_diffop_loc (input_location,
				    size_zero_node, result);
	else
	  {
	    if (!(complain & tf_error))
	      return error_mark_node;

	    error_not_base_type (from, to);
	    inform (input_location, "   in pointer to member conversion");
	    result = size_zero_node;
	  }
      }
  }

  return convert_to_integer (ptrdiff_type_node, result);
}

/* Return a constructor for the pointer-to-member-function TYPE using
   the other components as specified.  */

tree
build_ptrmemfunc1 (tree type, tree delta, tree pfn)
{
  tree u = NULL_TREE;
  tree delta_field;
  tree pfn_field;
  vec<constructor_elt, va_gc> *v;

  /* Pull the FIELD_DECLs out of the type.  */
  pfn_field = TYPE_FIELDS (type);
  delta_field = DECL_CHAIN (pfn_field);

  /* Make sure DELTA has the type we want.  */
  delta = convert_and_check (input_location, delta_type_node, delta);

  /* Convert to the correct target type if necessary.  */
  pfn = fold_convert (TREE_TYPE (pfn_field), pfn);

  /* Finish creating the initializer.  */
  vec_alloc (v, 2);
  CONSTRUCTOR_APPEND_ELT(v, pfn_field, pfn);
  CONSTRUCTOR_APPEND_ELT(v, delta_field, delta);
  u = build_constructor (type, v);
  TREE_CONSTANT (u) = TREE_CONSTANT (pfn) & TREE_CONSTANT (delta);
  TREE_STATIC (u) = (TREE_CONSTANT (u)
		     && (initializer_constant_valid_p (pfn, TREE_TYPE (pfn))
			 != NULL_TREE)
		     && (initializer_constant_valid_p (delta, TREE_TYPE (delta))
			 != NULL_TREE));
  return u;
}

/* Build a constructor for a pointer to member function.  It can be
   used to initialize global variables, local variable, or used
   as a value in expressions.  TYPE is the POINTER to METHOD_TYPE we
   want to be.

   If FORCE is nonzero, then force this conversion, even if
   we would rather not do it.  Usually set when using an explicit
   cast.  A C-style cast is being processed iff C_CAST_P is true.

   Return error_mark_node, if something goes wrong.  */

tree
build_ptrmemfunc (tree type, tree pfn, int force, bool c_cast_p,
		  tsubst_flags_t complain)
{
  tree fn;
  tree pfn_type;
  tree to_type;

  if (error_operand_p (pfn))
    return error_mark_node;

  pfn_type = TREE_TYPE (pfn);
  to_type = build_ptrmemfunc_type (type);

  /* Handle multiple conversions of pointer to member functions.  */
  if (TYPE_PTRMEMFUNC_P (pfn_type))
    {
      tree delta = NULL_TREE;
      tree npfn = NULL_TREE;
      tree n;

      if (!force
	  && !can_convert_arg (to_type, TREE_TYPE (pfn), pfn,
			       LOOKUP_NORMAL, complain))
	{
	  if (complain & tf_error)
	    error ("invalid conversion to type %qT from type %qT",
		   to_type, pfn_type);
	  else
	    return error_mark_node;
	}

      n = get_delta_difference (TYPE_PTRMEMFUNC_OBJECT_TYPE (pfn_type),
				TYPE_PTRMEMFUNC_OBJECT_TYPE (to_type),
				force,
				c_cast_p, complain);
      if (n == error_mark_node)
	return error_mark_node;

      STRIP_ANY_LOCATION_WRAPPER (pfn);

      /* We don't have to do any conversion to convert a
	 pointer-to-member to its own type.  But, we don't want to
	 just return a PTRMEM_CST if there's an explicit cast; that
	 cast should make the expression an invalid template argument.  */
      if (TREE_CODE (pfn) != PTRMEM_CST
	  && same_type_p (to_type, pfn_type))
	return pfn;

      if (TREE_SIDE_EFFECTS (pfn))
	pfn = save_expr (pfn);

      /* Obtain the function pointer and the current DELTA.  */
      if (TREE_CODE (pfn) == PTRMEM_CST)
	expand_ptrmemfunc_cst (pfn, &delta, &npfn);
      else
	{
	  npfn = build_ptrmemfunc_access_expr (pfn, pfn_identifier);
	  delta = build_ptrmemfunc_access_expr (pfn, delta_identifier);
	}

      /* Just adjust the DELTA field.  */
      gcc_assert  (same_type_ignoring_top_level_qualifiers_p
		   (TREE_TYPE (delta), ptrdiff_type_node));
      if (!integer_zerop (n))
	{
	  if (TARGET_PTRMEMFUNC_VBIT_LOCATION == ptrmemfunc_vbit_in_delta)
	    n = cp_build_binary_op (input_location,
				    LSHIFT_EXPR, n, integer_one_node,
				    complain);
	  delta = cp_build_binary_op (input_location,
				      PLUS_EXPR, delta, n, complain);
	}
      return build_ptrmemfunc1 (to_type, delta, npfn);
    }

  /* Handle null pointer to member function conversions.  */
  if (null_ptr_cst_p (pfn))
    {
      pfn = cp_build_c_cast (input_location,
			     TYPE_PTRMEMFUNC_FN_TYPE_RAW (to_type),
			     pfn, complain);
      return build_ptrmemfunc1 (to_type,
				integer_zero_node,
				pfn);
    }

  if (type_unknown_p (pfn))
    return instantiate_type (type, pfn, complain);

  fn = TREE_OPERAND (pfn, 0);
  gcc_assert (TREE_CODE (fn) == FUNCTION_DECL
	      /* In a template, we will have preserved the
		 OFFSET_REF.  */
	      || (processing_template_decl && TREE_CODE (fn) == OFFSET_REF));
  return make_ptrmem_cst (to_type, fn);
}

/* Return the DELTA, IDX, PFN, and DELTA2 values for the PTRMEM_CST
   given by CST.

   ??? There is no consistency as to the types returned for the above
   values.  Some code acts as if it were a sizetype and some as if it were
   integer_type_node.  */

void
expand_ptrmemfunc_cst (tree cst, tree *delta, tree *pfn)
{
  tree type = TREE_TYPE (cst);
  tree fn = PTRMEM_CST_MEMBER (cst);
  tree ptr_class, fn_class;

  gcc_assert (TREE_CODE (fn) == FUNCTION_DECL);

  /* The class that the function belongs to.  */
  fn_class = DECL_CONTEXT (fn);

  /* The class that we're creating a pointer to member of.  */
  ptr_class = TYPE_PTRMEMFUNC_OBJECT_TYPE (type);

  /* First, calculate the adjustment to the function's class.  */
  *delta = get_delta_difference (fn_class, ptr_class, /*force=*/0,
				 /*c_cast_p=*/0, tf_warning_or_error);

  if (!DECL_VIRTUAL_P (fn))
    {
      tree t = build_addr_func (fn, tf_warning_or_error);
      if (TREE_CODE (t) == ADDR_EXPR)
	SET_EXPR_LOCATION (t, PTRMEM_CST_LOCATION (cst));
      *pfn = convert (TYPE_PTRMEMFUNC_FN_TYPE (type), t);
    }
  else
    {
      /* If we're dealing with a virtual function, we have to adjust 'this'
	 again, to point to the base which provides the vtable entry for
	 fn; the call will do the opposite adjustment.  */
      tree orig_class = DECL_CONTEXT (fn);
      tree binfo = binfo_or_else (orig_class, fn_class);
      *delta = fold_build2 (PLUS_EXPR, TREE_TYPE (*delta),
			    *delta, BINFO_OFFSET (binfo));

      /* We set PFN to the vtable offset at which the function can be
	 found, plus one (unless ptrmemfunc_vbit_in_delta, in which
	 case delta is shifted left, and then incremented).  */
      *pfn = DECL_VINDEX (fn);
      *pfn = fold_build2 (MULT_EXPR, integer_type_node, *pfn,
			  TYPE_SIZE_UNIT (vtable_entry_type));

      switch (TARGET_PTRMEMFUNC_VBIT_LOCATION)
	{
	case ptrmemfunc_vbit_in_pfn:
	  *pfn = fold_build2 (PLUS_EXPR, integer_type_node, *pfn,
			      integer_one_node);
	  break;

	case ptrmemfunc_vbit_in_delta:
	  *delta = fold_build2 (LSHIFT_EXPR, TREE_TYPE (*delta),
				*delta, integer_one_node);
	  *delta = fold_build2 (PLUS_EXPR, TREE_TYPE (*delta),
				*delta, integer_one_node);
	  break;

	default:
	  gcc_unreachable ();
	}

      *pfn = fold_convert (TYPE_PTRMEMFUNC_FN_TYPE (type), *pfn);
    }
}

/* Return an expression for PFN from the pointer-to-member function
   given by T.  */

static tree
pfn_from_ptrmemfunc (tree t)
{
  if (TREE_CODE (t) == PTRMEM_CST)
    {
      tree delta;
      tree pfn;

      expand_ptrmemfunc_cst (t, &delta, &pfn);
      if (pfn)
	return pfn;
    }

  return build_ptrmemfunc_access_expr (t, pfn_identifier);
}

/* Return an expression for DELTA from the pointer-to-member function
   given by T.  */

static tree
delta_from_ptrmemfunc (tree t)
{
  if (TREE_CODE (t) == PTRMEM_CST)
    {
      tree delta;
      tree pfn;

      expand_ptrmemfunc_cst (t, &delta, &pfn);
      if (delta)
	return delta;
    }

  return build_ptrmemfunc_access_expr (t, delta_identifier);
}

/* Convert value RHS to type TYPE as preparation for an assignment to
   an lvalue of type TYPE.  ERRTYPE indicates what kind of error the
   implicit conversion is.  If FNDECL is non-NULL, we are doing the
   conversion in order to pass the PARMNUMth argument of FNDECL.
   If FNDECL is NULL, we are doing the conversion in function pointer
   argument passing, conversion in initialization, etc. */

static tree
convert_for_assignment (tree type, tree rhs,
			impl_conv_rhs errtype, tree fndecl, int parmnum,
			tsubst_flags_t complain, int flags)
{
  tree rhstype;
  enum tree_code coder;

  location_t rhs_loc = cp_expr_loc_or_input_loc (rhs);
  bool has_loc = EXPR_LOCATION (rhs) != UNKNOWN_LOCATION;
  /* Strip NON_LVALUE_EXPRs since we aren't using as an lvalue,
     but preserve location wrappers.  */
  if (TREE_CODE (rhs) == NON_LVALUE_EXPR
      && !location_wrapper_p (rhs))
    rhs = TREE_OPERAND (rhs, 0);

  /* Handle [dcl.init.list] direct-list-initialization from
     single element of enumeration with a fixed underlying type.  */
  if (is_direct_enum_init (type, rhs))
    {
      tree elt = CONSTRUCTOR_ELT (rhs, 0)->value;
      if (check_narrowing (ENUM_UNDERLYING_TYPE (type), elt, complain))
	{
	  warning_sentinel w (warn_useless_cast);
	  warning_sentinel w2 (warn_ignored_qualifiers);
	  rhs = cp_build_c_cast (rhs_loc, type, elt, complain);
	}
      else
	rhs = error_mark_node;
    }

  rhstype = TREE_TYPE (rhs);
  coder = TREE_CODE (rhstype);

  if (VECTOR_TYPE_P (type) && coder == VECTOR_TYPE
      && vector_types_convertible_p (type, rhstype, true))
    {
      rhs = mark_rvalue_use (rhs);
      return convert (type, rhs);
    }

  if (rhs == error_mark_node || rhstype == error_mark_node)
    return error_mark_node;
  if (TREE_CODE (rhs) == TREE_LIST && TREE_VALUE (rhs) == error_mark_node)
    return error_mark_node;

  /* The RHS of an assignment cannot have void type.  */
  if (coder == VOID_TYPE)
    {
      if (complain & tf_error)
	error_at (rhs_loc, "void value not ignored as it ought to be");
      return error_mark_node;
    }

  if (c_dialect_objc ())
    {
      int parmno;
      tree selector;
      tree rname = fndecl;

      switch (errtype)
        {
	  case ICR_ASSIGN:
	    parmno = -1;
	    break;
	  case ICR_INIT:
	    parmno = -2;
	    break;
	  default:
	    selector = objc_message_selector ();
	    parmno = parmnum;
	    if (selector && parmno > 1)
	      {
		rname = selector;
		parmno -= 1;
	      }
	}

      if (objc_compare_types (type, rhstype, parmno, rname))
	{
	  rhs = mark_rvalue_use (rhs);
	  return convert (type, rhs);
	}
    }

  /* [expr.ass]

     The expression is implicitly converted (clause _conv_) to the
     cv-unqualified type of the left operand.

     We allow bad conversions here because by the time we get to this point
     we are committed to doing the conversion.  If we end up doing a bad
     conversion, convert_like will complain.  */
  if (!can_convert_arg_bad (type, rhstype, rhs, flags, complain))
    {
      /* When -Wno-pmf-conversions is use, we just silently allow
	 conversions from pointers-to-members to plain pointers.  If
	 the conversion doesn't work, cp_convert will complain.  */
      if (!warn_pmf2ptr
	  && TYPE_PTR_P (type)
	  && TYPE_PTRMEMFUNC_P (rhstype))
	rhs = cp_convert (strip_top_quals (type), rhs, complain);
      else
	{
	  if (complain & tf_error)
	    {
	      auto_diagnostic_group d;

	      /* If the right-hand side has unknown type, then it is an
		 overloaded function.  Call instantiate_type to get error
		 messages.  */
	      if (rhstype == unknown_type_node)
		{
		  tree r = instantiate_type (type, rhs, tf_warning_or_error);
		  /* -fpermissive might allow this; recurse.  */
		  if (!seen_error ())
		    return convert_for_assignment (type, r, errtype, fndecl,
						   parmnum, complain, flags);
		}
	      else if (fndecl)
		complain_about_bad_argument (rhs_loc,
					     rhstype, type,
					     fndecl, parmnum);
	      else
		{
		  range_label_for_type_mismatch label (rhstype, type);
		  gcc_rich_location richloc
		    (rhs_loc,
		     has_loc ? &label : NULL,
		     has_loc ? highlight_colors::percent_h : NULL);

		  switch (errtype)
		    {
		    case ICR_DEFAULT_ARGUMENT:
		      error_at (&richloc,
				"cannot convert %qH to %qI in default argument",
				rhstype, type);
		      break;
		    case ICR_ARGPASS:
		      error_at (&richloc,
				"cannot convert %qH to %qI in argument passing",
				rhstype, type);
		      break;
		    case ICR_CONVERTING:
		      error_at (&richloc, "cannot convert %qH to %qI",
				rhstype, type);
		      break;
		    case ICR_INIT:
		      error_at (&richloc,
				"cannot convert %qH to %qI in initialization",
				rhstype, type);
		      break;
		    case ICR_RETURN:
		      error_at (&richloc, "cannot convert %qH to %qI in return",
				rhstype, type);
		      break;
		    case ICR_ASSIGN:
		      error_at (&richloc,
				"cannot convert %qH to %qI in assignment",
				rhstype, type);
		      break;
		    default:
		      gcc_unreachable();
		  }
		}

	      /* See if we can be more helpful.  */
	      maybe_show_nonconverting_candidate (type, rhstype, rhs, flags);

	      if (TYPE_PTR_P (rhstype)
		  && TYPE_PTR_P (type)
		  && CLASS_TYPE_P (TREE_TYPE (rhstype))
		  && CLASS_TYPE_P (TREE_TYPE (type))
		  && !COMPLETE_TYPE_P (TREE_TYPE (rhstype)))
		inform (DECL_SOURCE_LOCATION (TYPE_MAIN_DECL
					      (TREE_TYPE (rhstype))),
			"class type %qT is incomplete", TREE_TYPE (rhstype));
	    }
	  return error_mark_node;
	}
    }
  if (warn_suggest_attribute_format)
    {
      const enum tree_code codel = TREE_CODE (type);
      if ((codel == POINTER_TYPE || codel == REFERENCE_TYPE)
	  && coder == codel
	  && check_missing_format_attribute (type, rhstype)
	  && (complain & tf_warning))
	switch (errtype)
	  {
	    case ICR_ARGPASS:
	    case ICR_DEFAULT_ARGUMENT:
	      if (fndecl)
		warning (OPT_Wsuggest_attribute_format,
			 "parameter %qP of %qD might be a candidate "
			 "for a format attribute", parmnum, fndecl);
	      else
		warning (OPT_Wsuggest_attribute_format,
			 "parameter might be a candidate "
			 "for a format attribute");
	      break;
	    case ICR_CONVERTING:
	      warning (OPT_Wsuggest_attribute_format,
		       "target of conversion might be a candidate "
		       "for a format attribute");
	      break;
	    case ICR_INIT:
	      warning (OPT_Wsuggest_attribute_format,
		       "target of initialization might be a candidate "
		       "for a format attribute");
	      break;
	    case ICR_RETURN:
	      warning (OPT_Wsuggest_attribute_format,
		       "return type might be a candidate "
		       "for a format attribute");
	      break;
	    case ICR_ASSIGN:
	      warning (OPT_Wsuggest_attribute_format,
		       "left-hand side of assignment might be a candidate "
		       "for a format attribute");
	      break;
	    default:
	      gcc_unreachable();
	  }
    }

  if (TREE_CODE (type) == BOOLEAN_TYPE)
    maybe_warn_unparenthesized_assignment (rhs, /*nested_p=*/true, complain);

  if (complain & tf_warning)
    warn_for_address_of_packed_member (type, rhs);

  return perform_implicit_conversion_flags (strip_top_quals (type), rhs,
					    complain, flags);
}

/* Convert RHS to be of type TYPE.
   If EXP is nonzero, it is the target of the initialization.
   ERRTYPE indicates what kind of error the implicit conversion is.

   Two major differences between the behavior of
   `convert_for_assignment' and `convert_for_initialization'
   are that references are bashed in the former, while
   copied in the latter, and aggregates are assigned in
   the former (operator=) while initialized in the
   latter (X(X&)).

   If using constructor make sure no conversion operator exists, if one does
   exist, an ambiguity exists.  */

tree
convert_for_initialization (tree exp, tree type, tree rhs, int flags,
			    impl_conv_rhs errtype, tree fndecl, int parmnum,
                            tsubst_flags_t complain)
{
  enum tree_code codel = TREE_CODE (type);
  tree rhstype;
  enum tree_code coder;

  /* build_c_cast puts on a NOP_EXPR to make the result not an lvalue.
     Strip such NOP_EXPRs, since RHS is used in non-lvalue context.  */
  if (TREE_CODE (rhs) == NOP_EXPR
      && TREE_TYPE (rhs) == TREE_TYPE (TREE_OPERAND (rhs, 0))
      && codel != REFERENCE_TYPE)
    rhs = TREE_OPERAND (rhs, 0);

  if (type == error_mark_node
      || rhs == error_mark_node
      || (TREE_CODE (rhs) == TREE_LIST && TREE_VALUE (rhs) == error_mark_node))
    return error_mark_node;

  if (MAYBE_CLASS_TYPE_P (non_reference (type)))
    ;
  else if ((TREE_CODE (TREE_TYPE (rhs)) == ARRAY_TYPE
	    && TREE_CODE (type) != ARRAY_TYPE
	    && (!TYPE_REF_P (type)
		|| TREE_CODE (TREE_TYPE (type)) != ARRAY_TYPE))
	   || (TREE_CODE (TREE_TYPE (rhs)) == FUNCTION_TYPE
	       && !TYPE_REFFN_P (type))
	   || TREE_CODE (TREE_TYPE (rhs)) == METHOD_TYPE)
    rhs = decay_conversion (rhs, complain);

  rhstype = TREE_TYPE (rhs);
  coder = TREE_CODE (rhstype);

  if (coder == ERROR_MARK)
    return error_mark_node;

  /* We accept references to incomplete types, so we can
     return here before checking if RHS is of complete type.  */

  if (codel == REFERENCE_TYPE)
    {
      auto_diagnostic_group d;
      /* This should eventually happen in convert_arguments.  */
      int savew = 0, savee = 0;

      if (fndecl)
	savew = warningcount + werrorcount, savee = errorcount;
      rhs = initialize_reference (type, rhs, flags, complain);

      if (fndecl
	  && (warningcount + werrorcount > savew || errorcount > savee))
	inform (get_fndecl_argument_location (fndecl, parmnum),
		"in passing argument %P of %qD", parmnum, fndecl);
      return rhs;
    }

  if (exp != 0)
    exp = require_complete_type (exp, complain);
  if (exp == error_mark_node)
    return error_mark_node;

  type = complete_type (type);

  if (DIRECT_INIT_EXPR_P (type, rhs))
    /* Don't try to do copy-initialization if we already have
       direct-initialization.  */
    return rhs;

  if (MAYBE_CLASS_TYPE_P (type))
    return perform_implicit_conversion_flags (type, rhs, complain, flags);

  return convert_for_assignment (type, rhs, errtype, fndecl, parmnum,
				 complain, flags);
}

/* If RETVAL is the address of, or a reference to, a local variable or
   temporary give an appropriate warning and return true.  */

static bool
maybe_warn_about_returning_address_of_local (tree retval, location_t loc)
{
  tree valtype = TREE_TYPE (DECL_RESULT (current_function_decl));
  tree whats_returned = fold_for_warn (retval);
  if (!loc)
    loc = cp_expr_loc_or_input_loc (retval);

  for (;;)
    {
      if (TREE_CODE (whats_returned) == COMPOUND_EXPR)
	whats_returned = TREE_OPERAND (whats_returned, 1);
      else if (CONVERT_EXPR_P (whats_returned)
	       || TREE_CODE (whats_returned) == NON_LVALUE_EXPR)
	whats_returned = TREE_OPERAND (whats_returned, 0);
      else
	break;
    }

  if (TREE_CODE (whats_returned) == TARGET_EXPR
      && is_std_init_list (TREE_TYPE (whats_returned)))
    {
      tree init = TARGET_EXPR_INITIAL (whats_returned);
      if (TREE_CODE (init) == CONSTRUCTOR)
	/* Pull out the array address.  */
	whats_returned = CONSTRUCTOR_ELT (init, 0)->value;
      else if (TREE_CODE (init) == INDIRECT_REF)
	/* The source of a trivial copy looks like *(T*)&var.  */
	whats_returned = TREE_OPERAND (init, 0);
      else
	return false;
      STRIP_NOPS (whats_returned);
    }

  /* As a special case, we handle a call to std::move or std::forward.  */
  if (TREE_CODE (whats_returned) == CALL_EXPR
      && (is_std_move_p (whats_returned)
	  || is_std_forward_p (whats_returned)))
    {
      tree arg = CALL_EXPR_ARG (whats_returned, 0);
      return maybe_warn_about_returning_address_of_local (arg, loc);
    }

  if (TREE_CODE (whats_returned) != ADDR_EXPR)
    return false;
  whats_returned = TREE_OPERAND (whats_returned, 0);

  while (TREE_CODE (whats_returned) == COMPONENT_REF
	 || TREE_CODE (whats_returned) == ARRAY_REF)
    whats_returned = TREE_OPERAND (whats_returned, 0);

  if (TREE_CODE (whats_returned) == AGGR_INIT_EXPR
      || TREE_CODE (whats_returned) == TARGET_EXPR)
    {
      if (TYPE_REF_P (valtype))
	/* P2748 made this an error in C++26.  */
	emit_diagnostic ((cxx_dialect >= cxx26
			  ? diagnostics::kind::permerror
			  : diagnostics::kind::warning),
			 loc, OPT_Wreturn_local_addr,
			 "returning reference to temporary");
      else if (TYPE_PTR_P (valtype))
	warning_at (loc, OPT_Wreturn_local_addr,
		    "returning pointer to temporary");
      else if (is_std_init_list (valtype))
	warning_at (loc, OPT_Winit_list_lifetime,
		    "returning temporary %<initializer_list%> does not extend "
		    "the lifetime of the underlying array");
      return true;
    }

  STRIP_ANY_LOCATION_WRAPPER (whats_returned);

  if (DECL_P (whats_returned)
      && DECL_NAME (whats_returned)
      && DECL_FUNCTION_SCOPE_P (whats_returned)
      && !is_capture_proxy (whats_returned)
      && !(TREE_STATIC (whats_returned)
	   || TREE_PUBLIC (whats_returned)))
    {
      if (DECL_DECOMPOSITION_P (whats_returned)
	  && !DECL_DECOMP_IS_BASE (whats_returned)
	  && DECL_HAS_VALUE_EXPR_P (whats_returned))
	{
	  /* When returning address of a structured binding, if the structured
	     binding is not a reference, continue normally, if it is a
	     reference, recurse on the initializer of the structured
	     binding.  */
	  tree base = DECL_DECOMP_BASE (whats_returned);
	  if (TYPE_REF_P (TREE_TYPE (base)))
	    {
	      if (tree init = DECL_INITIAL (base))
		return maybe_warn_about_returning_address_of_local (init, loc);
	      else
		return false;
	    }
	}
      bool w = false;
      auto_diagnostic_group d;
      if (TYPE_REF_P (valtype))
	w = warning_at (loc, OPT_Wreturn_local_addr,
			"reference to local variable %qD returned",
			whats_returned);
      else if (is_std_init_list (valtype))
	w = warning_at (loc, OPT_Winit_list_lifetime,
			"returning local %<initializer_list%> variable %qD "
			"does not extend the lifetime of the underlying array",
			whats_returned);
      else if (POINTER_TYPE_P (valtype)
	       && TREE_CODE (whats_returned) == LABEL_DECL)
	w = warning_at (loc, OPT_Wreturn_local_addr,
			"address of label %qD returned",
			whats_returned);
      else if (POINTER_TYPE_P (valtype))
	w = warning_at (loc, OPT_Wreturn_local_addr,
			"address of local variable %qD returned",
			whats_returned);
      if (w)
	inform (DECL_SOURCE_LOCATION (whats_returned),
		"declared here");
      return true;
    }

  return false;
}

/* Returns true if DECL is in the std namespace.  */

bool
decl_in_std_namespace_p (tree decl)
{
  while (decl)
    {
      decl = decl_namespace_context (decl);
      if (DECL_NAMESPACE_STD_P (decl))
	return true;
      /* Allow inline namespaces inside of std namespace, e.g. with
	 --enable-symvers=gnu-versioned-namespace std::forward would be
	 actually std::_8::forward.  */
      if (!DECL_NAMESPACE_INLINE_P (decl))
	return false;
      decl = CP_DECL_CONTEXT (decl);
    }
  return false;
}

/* Returns true if FN, a CALL_EXPR, is a call to std::forward.  */

static bool
is_std_forward_p (tree fn)
{
  /* std::forward only takes one argument.  */
  if (call_expr_nargs (fn) != 1)
    return false;

  tree fndecl = cp_get_callee_fndecl_nofold (fn);
  if (!decl_in_std_namespace_p (fndecl))
    return false;

  tree name = DECL_NAME (fndecl);
  return name && id_equal (name, "forward");
}

/* Returns true if FN, a CALL_EXPR, is a call to std::move.  */

static bool
is_std_move_p (tree fn)
{
  /* std::move only takes one argument.  */
  if (call_expr_nargs (fn) != 1)
    return false;

  tree fndecl = cp_get_callee_fndecl_nofold (fn);
  if (!decl_in_std_namespace_p (fndecl))
    return false;

  tree name = DECL_NAME (fndecl);
  return name && id_equal (name, "move");
}

/* Returns true if RETVAL is a good candidate for the NRVO as per
   [class.copy.elision].  FUNCTYPE is the type the function is declared
   to return.  */

static bool
can_do_nrvo_p (tree retval, tree functype)
{
  if (functype == error_mark_node)
    return false;
  if (retval)
    STRIP_ANY_LOCATION_WRAPPER (retval);
  tree result = DECL_RESULT (current_function_decl);
  return (retval != NULL_TREE
	  && !processing_template_decl
	  /* Must be a local, automatic variable.  */
	  && VAR_P (retval)
	  && DECL_CONTEXT (retval) == current_function_decl
	  && !TREE_STATIC (retval)
	  /* And not a lambda or anonymous union proxy.  */
	  && !DECL_HAS_VALUE_EXPR_P (retval)
	  && (DECL_ALIGN (retval) <= DECL_ALIGN (result))
	  /* The cv-unqualified type of the returned value must be the
	     same as the cv-unqualified return type of the
	     function.  */
	  && same_type_p (TYPE_MAIN_VARIANT (TREE_TYPE (retval)),
			  TYPE_MAIN_VARIANT (functype))
	  /* And the returned value must be non-volatile.  */
	  && !TYPE_VOLATILE (TREE_TYPE (retval)));
}

/* True if we would like to perform NRVO, i.e. can_do_nrvo_p is true and we
   would otherwise return in memory.  */

static bool
want_nrvo_p (tree retval, tree functype)
{
  return (can_do_nrvo_p (retval, functype)
	  && aggregate_value_p (functype, current_function_decl));
}

/* Like can_do_nrvo_p, but we check if we're trying to move a class
   prvalue.  */

static bool
can_elide_copy_prvalue_p (tree retval, tree functype)
{
  if (functype == error_mark_node)
    return false;
  if (retval)
    STRIP_ANY_LOCATION_WRAPPER (retval);
  return (retval != NULL_TREE
	  && !glvalue_p (retval)
	  && same_type_p (TYPE_MAIN_VARIANT (TREE_TYPE (retval)),
			  TYPE_MAIN_VARIANT (functype))
	  && !TYPE_VOLATILE (TREE_TYPE (retval)));
}

/* If we should treat RETVAL, an expression being returned, as if it were
   designated by an rvalue, returns it adjusted accordingly; otherwise, returns
   NULL_TREE.  See [class.copy.elision].  RETURN_P is true if this is a return
   context (rather than throw).  */

tree
treat_lvalue_as_rvalue_p (tree expr, bool return_p)
{
  if (cxx_dialect == cxx98)
    return NULL_TREE;

  tree retval = expr;
  STRIP_ANY_LOCATION_WRAPPER (retval);
  if (REFERENCE_REF_P (retval))
    retval = TREE_OPERAND (retval, 0);

  /* An implicitly movable entity is a variable of automatic storage duration
     that is either a non-volatile object or (C++20) an rvalue reference to a
     non-volatile object type.  */
  if (!(((VAR_P (retval) && !DECL_HAS_VALUE_EXPR_P (retval))
	 || TREE_CODE (retval) == PARM_DECL)
	&& !TREE_STATIC (retval)
	&& !CP_TYPE_VOLATILE_P (non_reference (TREE_TYPE (retval)))
	&& (TREE_CODE (TREE_TYPE (retval)) != REFERENCE_TYPE
	    || (cxx_dialect >= cxx20
		&& TYPE_REF_IS_RVALUE (TREE_TYPE (retval))))))
    return NULL_TREE;

  /* If the expression in a return or co_return statement is a (possibly
     parenthesized) id-expression that names an implicitly movable entity
     declared in the body or parameter-declaration-clause of the innermost
     enclosing function or lambda-expression, */
  if (return_p)
    {
      if (DECL_CONTEXT (retval) != current_function_decl)
	return NULL_TREE;
      expr = move (expr);
      if (expr == error_mark_node)
	return NULL_TREE;
      return set_implicit_rvalue_p (expr);
    }

  /* if the id-expression (possibly parenthesized) is the operand of
     a throw-expression, and names an implicitly movable entity that belongs
     to a scope that does not contain the compound-statement of the innermost
     lambda-expression, try-block, or function-try-block (if any) whose
     compound-statement or ctor-initializer contains the throw-expression.  */

  /* C++20 added move on throw of parms.  */
  if (TREE_CODE (retval) == PARM_DECL && cxx_dialect < cxx20)
    return NULL_TREE;

  /* We don't check for lambda-expression here, because we should not get past
     the DECL_HAS_VALUE_EXPR_P check above.  */
  for (cp_binding_level *b = current_binding_level;
       b->kind != sk_namespace; b = b->level_chain)
    {
      for (tree decl = b->names; decl; decl = TREE_CHAIN (decl))
	if (decl == retval)
	  return set_implicit_rvalue_p (move (expr));
      if (b->kind == sk_try)
	return NULL_TREE;
    }

  return set_implicit_rvalue_p (move (expr));
}

/* Warn about dubious usage of std::move (in a return statement, if RETURN_P
   is true).  EXPR is the std::move expression; TYPE is the type of the object
   being initialized.  */

void
maybe_warn_pessimizing_move (tree expr, tree type, bool return_p)
{
  if (!(warn_pessimizing_move || warn_redundant_move))
    return;

  const location_t loc = cp_expr_loc_or_input_loc (expr);

  /* C++98 doesn't know move.  */
  if (cxx_dialect < cxx11)
    return;

  /* Wait until instantiation time, since we can't gauge if we should do
     the NRVO until then.  */
  if (processing_template_decl)
    return;

  /* This is only interesting for class types.  */
  if (!CLASS_TYPE_P (type))
    return;

  bool wrapped_p = false;
  /* A a = std::move (A());  */
  if (TREE_CODE (expr) == TREE_LIST)
    {
      if (list_length (expr) == 1)
	{
	  expr = TREE_VALUE (expr);
	  wrapped_p = true;
	}
      else
	return;
    }
  /* A a = {std::move (A())};
     A a{std::move (A())};  */
  else if (TREE_CODE (expr) == CONSTRUCTOR)
    {
      if (CONSTRUCTOR_NELTS (expr) == 1)
	{
	  expr = CONSTRUCTOR_ELT (expr, 0)->value;
	  wrapped_p = true;
	}
      else
	return;
    }

  /* First, check if this is a call to std::move.  */
  if (!REFERENCE_REF_P (expr)
      || TREE_CODE (TREE_OPERAND (expr, 0)) != CALL_EXPR)
    return;
  tree fn = TREE_OPERAND (expr, 0);
  if (!is_std_move_p (fn))
    return;
  tree arg = CALL_EXPR_ARG (fn, 0);
  if (TREE_CODE (arg) != NOP_EXPR)
    return;
  /* If we're looking at *std::move<T&> ((T &) &arg), do the pessimizing N/RVO
     and implicitly-movable warnings.  */
  if (TREE_CODE (TREE_OPERAND (arg, 0)) == ADDR_EXPR)
    {
      arg = TREE_OPERAND (arg, 0);
      arg = TREE_OPERAND (arg, 0);
      arg = convert_from_reference (arg);
      if (can_elide_copy_prvalue_p (arg, type))
	{
	  auto_diagnostic_group d;
	  if (warning_at (loc, OPT_Wpessimizing_move,
			  "moving a temporary object prevents copy elision"))
	    inform (loc, "remove %<std::move%> call");
	}
      /* The rest of the warnings is only relevant for when we are returning
	 from a function.  */
      if (!return_p)
	return;

      tree moved;
      /* Warn if we could do copy elision were it not for the move.  */
      if (can_do_nrvo_p (arg, type))
	{
	  auto_diagnostic_group d;
	  if (!warning_suppressed_p (expr, OPT_Wpessimizing_move)
	      && warning_at (loc, OPT_Wpessimizing_move,
			     "moving a local object in a return statement "
			     "prevents copy elision"))
	    inform (loc, "remove %<std::move%> call");
	}
      /* Warn if the move is redundant.  It is redundant when we would
	 do maybe-rvalue overload resolution even without std::move.  */
      else if (warn_redundant_move
	       /* This doesn't apply for return {std::move (t)};.  */
	       && !wrapped_p
	       && !warning_suppressed_p (expr, OPT_Wredundant_move)
	       && (moved = treat_lvalue_as_rvalue_p (arg, /*return*/true)))
	{
	  /* Make sure that overload resolution would actually succeed
	     if we removed the std::move call.  */
	  tree t = convert_for_initialization (NULL_TREE, type,
					       moved,
					       (LOOKUP_NORMAL
						| LOOKUP_ONLYCONVERTING),
					       ICR_RETURN, NULL_TREE, 0,
					       tf_none);
	  /* If this worked, implicit rvalue would work, so the call to
	     std::move is redundant.  */
	  if (t != error_mark_node)
	    {
	      auto_diagnostic_group d;
	      if (warning_at (loc, OPT_Wredundant_move,
			      "redundant move in return statement"))
		inform (loc, "remove %<std::move%> call");
	    }
	}
     }
  /* Also try to warn about redundant std::move in code such as
      T f (const T& t)
      {
	return std::move(t);
      }
    for which EXPR will be something like
      *std::move<const T&> ((const struct T &) (const struct T *) t)
     and where the std::move does nothing if T does not have a T(const T&&)
     constructor, because the argument is const.  It will not use T(T&&)
     because that would mean losing the const.  */
  else if (warn_redundant_move
	   && !warning_suppressed_p (expr, OPT_Wredundant_move)
	   && TYPE_REF_P (TREE_TYPE (arg))
	   && CP_TYPE_CONST_P (TREE_TYPE (TREE_TYPE (arg))))
    {
      tree rtype = TREE_TYPE (TREE_TYPE (arg));
      if (!same_type_ignoring_top_level_qualifiers_p (rtype, type))
	return;
      /* Check for the unlikely case there's T(const T&&) (we don't care if
	 it's deleted).  */
      for (tree fn : ovl_range (CLASSTYPE_CONSTRUCTORS (rtype)))
	if (move_fn_p (fn))
	  {
	    tree t = TREE_VALUE (FUNCTION_FIRST_USER_PARMTYPE (fn));
	    if (UNLIKELY (CP_TYPE_CONST_P (TREE_TYPE (t))))
	      return;
	  }
      auto_diagnostic_group d;
      if (return_p
	  ? warning_at (loc, OPT_Wredundant_move,
			"redundant move in return statement")
	  : warning_at (loc, OPT_Wredundant_move,
			"redundant move in initialization"))
	inform (loc, "remove %<std::move%> call");
    }
}

/* Check that returning RETVAL from the current function is valid.
   Return an expression explicitly showing all conversions required to
   change RETVAL into the function return type, and to assign it to
   the DECL_RESULT for the function.  Set *NO_WARNING to true if
   code reaches end of non-void function warning shouldn't be issued
   on this RETURN_EXPR.  Set *DANGLING to true if code returns the
   address of a local variable.  */

tree
check_return_expr (tree retval, bool *no_warning, bool *dangling)
{
  tree result;
  /* The type actually returned by the function.  */
  tree valtype;
  /* The type the function is declared to return, or void if
     the declared type is incomplete.  */
  tree functype;
  int fn_returns_value_p;
  location_t loc = cp_expr_loc_or_input_loc (retval);

  *no_warning = false;
  *dangling = false;

  /* A `volatile' function is one that isn't supposed to return, ever.
     (This is a G++ extension, used to get better code for functions
     that call the `volatile' function.)  */
  if (TREE_THIS_VOLATILE (current_function_decl))
    warning (0, "function declared %<noreturn%> has a %<return%> statement");

  /* Check for various simple errors.  */
  if (DECL_DESTRUCTOR_P (current_function_decl))
    {
      if (retval)
	error_at (loc, "returning a value from a destructor");

      if (targetm.cxx.cdtor_returns_this () && !processing_template_decl)
	retval = current_class_ptr;
      else
	return NULL_TREE;
    }
  else if (DECL_CONSTRUCTOR_P (current_function_decl))
    {
      if (in_function_try_handler)
	/* If a return statement appears in a handler of the
	   function-try-block of a constructor, the program is ill-formed.  */
	error ("cannot return from a handler of a function-try-block of a constructor");
      else if (retval)
	/* You can't return a value from a constructor.  */
	error_at (loc, "returning a value from a constructor");

      if (targetm.cxx.cdtor_returns_this () && !processing_template_decl)
	retval = current_class_ptr;
      else
	return NULL_TREE;
    }

  const tree saved_retval = retval;

  if (processing_template_decl)
    {
      current_function_returns_value = 1;

      if (check_for_bare_parameter_packs (retval))
	return error_mark_node;

      /* If one of the types might be void, we can't tell whether we're
	 returning a value.  */
      if ((WILDCARD_TYPE_P (TREE_TYPE (DECL_RESULT (current_function_decl)))
	   && !FNDECL_USED_AUTO (current_function_decl))
	  || (retval != NULL_TREE
	      && (TREE_TYPE (retval) == NULL_TREE
		  || WILDCARD_TYPE_P (TREE_TYPE (retval)))))
	goto dependent;
    }

  functype = TREE_TYPE (TREE_TYPE (current_function_decl));

  /* Deduce auto return type from a return statement.  */
  if (FNDECL_USED_AUTO (current_function_decl))
    {
      tree pattern = DECL_SAVED_AUTO_RETURN_TYPE (current_function_decl);
      tree auto_node;
      tree type;

      if (!retval && !is_auto (pattern))
	{
	  /* Give a helpful error message.  */
	  auto_diagnostic_group d;
	  error ("return-statement with no value, in function returning %qT",
		 pattern);
	  inform (input_location, "only plain %<auto%> return type can be "
		  "deduced to %<void%>");
	  type = error_mark_node;
	}
      else if (retval && BRACE_ENCLOSED_INITIALIZER_P (retval))
	{
	  error ("returning initializer list");
	  type = error_mark_node;
	}
      else
	{
	  if (!retval)
	    retval = void_node;
	  auto_node = type_uses_auto (pattern);
	  type = do_auto_deduction (pattern, retval, auto_node,
				    tf_warning_or_error, adc_return_type);
	}

      if (type == error_mark_node)
	/* Leave it.  */;
      else if (functype == pattern)
	apply_deduced_return_type (current_function_decl, type);
      else if (!same_type_p (type, functype))
	{
	  if (LAMBDA_FUNCTION_P (current_function_decl))
	    error_at (loc, "inconsistent types %qT and %qT deduced for "
		      "lambda return type", functype, type);
	  else
	    error_at (loc, "inconsistent deduction for auto return type: "
		      "%qT and then %qT", functype, type);
	}
      functype = type;
    }

  result = DECL_RESULT (current_function_decl);
  valtype = TREE_TYPE (result);
  gcc_assert (valtype != NULL_TREE);
  fn_returns_value_p = !VOID_TYPE_P (valtype);

  /* Check for a return statement with no return value in a function
     that's supposed to return a value.  */
  if (!retval && fn_returns_value_p)
    {
      if (functype != error_mark_node)
	permerror (input_location, "return-statement with no value, in "
		   "function returning %qT", valtype);
      /* Remember that this function did return.  */
      current_function_returns_value = 1;
      /* But suppress NRV  .*/
      current_function_return_value = error_mark_node;
      /* And signal caller that TREE_NO_WARNING should be set on the
	 RETURN_EXPR to avoid control reaches end of non-void function
	 warnings in tree-cfg.cc.  */
      *no_warning = true;
    }
  /* Check for a return statement with a value in a function that
     isn't supposed to return a value.  */
  else if (retval && !fn_returns_value_p)
    {
      if (VOID_TYPE_P (TREE_TYPE (retval)))
	/* You can return a `void' value from a function of `void'
	   type.  In that case, we have to evaluate the expression for
	   its side-effects.  */
	finish_expr_stmt (retval);
      else if (retval != error_mark_node)
	permerror (loc, "return-statement with a value, in function "
		   "returning %qT", valtype);
      current_function_returns_null = 1;

      /* There's really no value to return, after all.  */
      return NULL_TREE;
    }
  else if (!retval)
    /* Remember that this function can sometimes return without a
       value.  */
    current_function_returns_null = 1;
  else
    /* Remember that this function did return a value.  */
    current_function_returns_value = 1;

  /* Check for erroneous operands -- but after giving ourselves a
     chance to provide an error about returning a value from a void
     function.  */
  if (error_operand_p (retval))
    {
      current_function_return_value = error_mark_node;
      return error_mark_node;
    }

  /* Only operator new(...) throw(), can return NULL [expr.new/13].  */
  if (IDENTIFIER_NEW_OP_P (DECL_NAME (current_function_decl))
      && !TYPE_NOTHROW_P (TREE_TYPE (current_function_decl))
      && ! flag_check_new
      && retval && null_ptr_cst_p (retval))
    warning (0, "%<operator new%> must not return NULL unless it is "
	     "declared %<throw()%> (or %<-fcheck-new%> is in effect)");

  /* Effective C++ rule 15.  See also start_function.  */
  if (warn_ecpp
      && DECL_NAME (current_function_decl) == assign_op_identifier
      && !type_dependent_expression_p (retval))
    {
      bool warn = true;

      /* The function return type must be a reference to the current
	class.  */
      if (TYPE_REF_P (valtype)
	  && same_type_ignoring_top_level_qualifiers_p
	      (TREE_TYPE (valtype), TREE_TYPE (current_class_ref)))
	{
	  /* Returning '*this' is obviously OK.  */
	  if (retval == current_class_ref)
	    warn = false;
	  /* If we are calling a function whose return type is the same of
	     the current class reference, it is ok.  */
	  else if (INDIRECT_REF_P (retval)
		   && TREE_CODE (TREE_OPERAND (retval, 0)) == CALL_EXPR)
	    warn = false;
	}

      if (warn)
	warning_at (loc, OPT_Weffc__,
		    "%<operator=%> should return a reference to %<*this%>");
    }

  if (dependent_type_p (functype)
      || type_dependent_expression_p (retval))
    {
    dependent:
      /* We should not have changed the return value.  */
      gcc_assert (retval == saved_retval);
      /* We don't know if this is an lvalue or rvalue use, but
	 either way we can mark it as read.  */
      mark_exp_read (retval);
      return retval;
    }

  /* The fabled Named Return Value optimization, as per [class.copy]/15:

     [...]      For  a function with a class return type, if the expression
     in the return statement is the name of a local  object,  and  the  cv-
     unqualified  type  of  the  local  object  is the same as the function
     return type, an implementation is permitted to omit creating the  tem-
     porary  object  to  hold  the function return value [...]

     So, if this is a value-returning function that always returns the same
     local variable, remember it.

     We choose the first suitable variable even if the function sometimes
     returns something else, but only if the variable is out of scope at the
     other return sites, or else we run the risk of clobbering the variable we
     chose if the other returned expression uses the chosen variable somehow.

     We don't currently do this if the first return is a non-variable, as it
     would be complicated to determine whether an NRV selected later was in
     scope at the point of the earlier return.  We also don't currently support
     multiple variables with non-overlapping scopes (53637).

     See finish_function and finalize_nrv for the rest of this optimization.  */
  tree bare_retval = NULL_TREE;
  if (retval)
    {
      retval = maybe_undo_parenthesized_ref (retval);
      bare_retval = tree_strip_any_location_wrapper (retval);
    }

  bool named_return_value_okay_p = want_nrvo_p (bare_retval, functype);
  if (fn_returns_value_p && flag_elide_constructors
      && current_function_return_value != bare_retval)
    {
      if (named_return_value_okay_p
	  && current_function_return_value == NULL_TREE)
	current_function_return_value = bare_retval;
      else if (current_function_return_value
	       && VAR_P (current_function_return_value)
	       && DECL_NAME (current_function_return_value)
	       && !decl_in_scope_p (current_function_return_value))
	{
	  /* The earlier NRV is out of scope at this point, so it's safe to
	     leave it alone; the current return can't refer to it.  */;
	  if (named_return_value_okay_p
	      && !warning_suppressed_p (current_function_decl, OPT_Wnrvo))
	    {
	      warning (OPT_Wnrvo, "not eliding copy on return from %qD",
		       bare_retval);
	      suppress_warning (current_function_decl, OPT_Wnrvo);
	    }
	}
      else
	{
	  if ((named_return_value_okay_p
	       || (current_function_return_value
		   && current_function_return_value != error_mark_node))
	      && !warning_suppressed_p (current_function_decl, OPT_Wnrvo))
	    {
	      warning (OPT_Wnrvo, "not eliding copy on return in %qD",
		       current_function_decl);
	      suppress_warning (current_function_decl, OPT_Wnrvo);
	    }
	  current_function_return_value = error_mark_node;
	}
    }

  /* We don't need to do any conversions when there's nothing being
     returned.  */
  if (!retval)
    return NULL_TREE;

  if (!named_return_value_okay_p)
    maybe_warn_pessimizing_move (retval, functype, /*return_p*/true);

  /* Do any required conversions.  */
  if (bare_retval == result || DECL_CONSTRUCTOR_P (current_function_decl))
    /* No conversions are required.  */
    ;
  else
    {
      int flags = LOOKUP_NORMAL | LOOKUP_ONLYCONVERTING;

      /* The functype's return type will have been set to void, if it
	 was an incomplete type.  Just treat this as 'return;' */
      if (VOID_TYPE_P (functype))
	return error_mark_node;

      /* Under C++11 [12.8/32 class.copy], a returned lvalue is sometimes
	 treated as an rvalue for the purposes of overload resolution to
	 favor move constructors over copy constructors.

         Note that these conditions are similar to, but not as strict as,
	 the conditions for the named return value optimization.  */
      bool converted = false;
      tree moved;
      /* Until C++23, this was only interesting for class type, but in C++23,
	 we should do the below when we're converting from/to a class/reference
	 (a non-scalar type).  */
	if ((cxx_dialect < cxx23
	     ? CLASS_TYPE_P (functype)
	     : !SCALAR_TYPE_P (functype) || !SCALAR_TYPE_P (TREE_TYPE (retval)))
	    && (moved = treat_lvalue_as_rvalue_p (retval, /*return*/true)))
	  /* In C++20 and earlier we treat the return value as an rvalue
	     that can bind to lvalue refs.  In C++23, such an expression is just
	     an xvalue (see reference_binding).  */
	  retval = moved;

      /* The call in a (lambda) thunk needs no conversions.  */
      if (TREE_CODE (retval) == CALL_EXPR
	  && call_from_lambda_thunk_p (retval))
	converted = true;

      /* Don't check copy-initialization for NRV in a coroutine ramp; we
	 implement this case as NRV, but it's specified as directly
	 initializing the return value from get_return_object().  */
      if (DECL_RAMP_P (current_function_decl) && named_return_value_okay_p)
	converted = true;

      /* First convert the value to the function's return type, then
	 to the type of return value's location to handle the
	 case that functype is smaller than the valtype.  */
      if (!converted)
	retval = convert_for_initialization
	  (NULL_TREE, functype, retval, flags, ICR_RETURN, NULL_TREE, 0,
	   tf_warning_or_error);
      retval = convert (valtype, retval);

      /* If the conversion failed, treat this just like `return;'.  */
      if (retval == error_mark_node)
	{
	  /* And suppress NRV.  */
	  current_function_return_value = error_mark_node;
	  return retval;
	}
      /* We can't initialize a register from a AGGR_INIT_EXPR.  */
      else if (! cfun->returns_struct
	       && TREE_CODE (retval) == TARGET_EXPR
	       && TREE_CODE (TARGET_EXPR_INITIAL (retval)) == AGGR_INIT_EXPR)
	retval = build2 (COMPOUND_EXPR, TREE_TYPE (retval), retval,
			 TARGET_EXPR_SLOT (retval));
      else if (!processing_template_decl
	       && maybe_warn_about_returning_address_of_local (retval, loc)
	       && INDIRECT_TYPE_P (valtype))
	*dangling = true;
    }

  /* A naive attempt to reduce the number of -Wdangling-reference false
     positives: if we know that this function can return a variable with
     static storage duration rather than one of its parameters, suppress
     the warning.  */
  if (warn_dangling_reference
      && TYPE_REF_P (functype)
      && bare_retval
      && VAR_P (bare_retval)
      && TREE_STATIC (bare_retval))
    suppress_warning (current_function_decl, OPT_Wdangling_reference);

  if (processing_template_decl)
    return saved_retval;

  /* Actually copy the value returned into the appropriate location.  */
  if (retval && retval != result)
    retval = cp_build_init_expr (result, retval);

  if (current_function_return_value == bare_retval)
    INIT_EXPR_NRV_P (retval) = true;

  if (tree set = maybe_set_retval_sentinel ())
    retval = build2 (COMPOUND_EXPR, void_type_node, retval, set);

  return retval;
}


/* Returns nonzero if the pointer-type FROM can be converted to the
   pointer-type TO via a qualification conversion.  If CONSTP is -1,
   then we return nonzero if the pointers are similar, and the
   cv-qualification signature of FROM is a proper subset of that of TO.

   If CONSTP is positive, then all outer pointers have been
   const-qualified.  */

static bool
comp_ptr_ttypes_real (tree to, tree from, int constp)
{
  bool to_more_cv_qualified = false;
  bool is_opaque_pointer = false;

  for (; ; to = TREE_TYPE (to), from = TREE_TYPE (from))
    {
      if (TREE_CODE (to) != TREE_CODE (from))
	return false;

      if (TREE_CODE (from) == OFFSET_TYPE
	  && !same_type_p (TYPE_OFFSET_BASETYPE (from),
			   TYPE_OFFSET_BASETYPE (to)))
	return false;

      /* Const and volatile mean something different for function and
	 array types, so the usual checks are not appropriate.  We'll
	 check the array type elements in further iterations.  */
      if (!FUNC_OR_METHOD_TYPE_P (to) && TREE_CODE (to) != ARRAY_TYPE)
	{
	  if (!at_least_as_qualified_p (to, from))
	    return false;

	  if (!at_least_as_qualified_p (from, to))
	    {
	      if (constp == 0)
		return false;
	      to_more_cv_qualified = true;
	    }

	  if (constp > 0)
	    constp &= TYPE_READONLY (to);
	}

      if (VECTOR_TYPE_P (to))
	is_opaque_pointer = vector_targets_convertible_p (to, from);

      /* P0388R4 allows a conversion from int[N] to int[] but not the
	 other way round.  When both arrays have bounds but they do
	 not match, then no conversion is possible.  */
      if (TREE_CODE (to) == ARRAY_TYPE
	  && !comp_array_types (to, from, bounds_first, /*strict=*/false))
	return false;

      if (!TYPE_PTR_P (to)
	  && !TYPE_PTRDATAMEM_P (to)
	  /* CWG 330 says we need to look through arrays.  */
	  && TREE_CODE (to) != ARRAY_TYPE)
	return ((constp >= 0 || to_more_cv_qualified)
		&& (is_opaque_pointer
		    || same_type_ignoring_top_level_qualifiers_p (to, from)));
    }
}

/* When comparing, say, char ** to char const **, this function takes
   the 'char *' and 'char const *'.  Do not pass non-pointer/reference
   types to this function.  */

int
comp_ptr_ttypes (tree to, tree from)
{
  return comp_ptr_ttypes_real (to, from, 1);
}

/* Returns true iff FNTYPE is a non-class type that involves
   error_mark_node.  We can get FUNCTION_TYPE with buried error_mark_node
   if a parameter type is ill-formed.  */

bool
error_type_p (const_tree type)
{
  tree t;

  switch (TREE_CODE (type))
    {
    case ERROR_MARK:
      return true;

    case POINTER_TYPE:
    case REFERENCE_TYPE:
    case OFFSET_TYPE:
      return error_type_p (TREE_TYPE (type));

    case FUNCTION_TYPE:
    case METHOD_TYPE:
      if (error_type_p (TREE_TYPE (type)))
	return true;
      for (t = TYPE_ARG_TYPES (type); t; t = TREE_CHAIN (t))
	if (error_type_p (TREE_VALUE (t)))
	  return true;
      return false;

    case RECORD_TYPE:
      if (TYPE_PTRMEMFUNC_P (type))
	return error_type_p (TYPE_PTRMEMFUNC_FN_TYPE (type));
      return false;

    default:
      return false;
    }
}

/* Returns true if to and from are (possibly multi-level) pointers to the same
   type or inheritance-related types, regardless of cv-quals.  */

bool
ptr_reasonably_similar (const_tree to, const_tree from)
{
  for (; ; to = TREE_TYPE (to), from = TREE_TYPE (from))
    {
      /* Any target type is similar enough to void.  */
      if (VOID_TYPE_P (to))
	return !error_type_p (from);
      if (VOID_TYPE_P (from))
	return !error_type_p (to);

      if (TREE_CODE (to) != TREE_CODE (from))
	return false;

      if (TREE_CODE (from) == OFFSET_TYPE
	  && comptypes (TYPE_OFFSET_BASETYPE (to),
			TYPE_OFFSET_BASETYPE (from),
			COMPARE_BASE | COMPARE_DERIVED))
	continue;

      if (VECTOR_TYPE_P (to)
	  && vector_types_convertible_p (to, from, false))
	return true;

      if (TREE_CODE (to) == INTEGER_TYPE
	  && TYPE_PRECISION (to) == TYPE_PRECISION (from))
	return true;

      if (TREE_CODE (to) == FUNCTION_TYPE)
	return !error_type_p (to) && !error_type_p (from);

      if (!TYPE_PTR_P (to))
	{
	  /* When either type is incomplete avoid DERIVED_FROM_P,
	     which may call complete_type (c++/57942).  */
	  bool b = !COMPLETE_TYPE_P (to) || !COMPLETE_TYPE_P (from);
	  return comptypes
	    (TYPE_MAIN_VARIANT (to), TYPE_MAIN_VARIANT (from),
	     b ? COMPARE_STRICT : COMPARE_BASE | COMPARE_DERIVED);
	}
    }
}

/* Return true if TO and FROM (both of which are POINTER_TYPEs or
   pointer-to-member types) are the same, ignoring cv-qualification at
   all levels.  CB says how we should behave when comparing array bounds.  */

bool
comp_ptr_ttypes_const (tree to, tree from, compare_bounds_t cb)
{
  bool is_opaque_pointer = false;

  for (; ; to = TREE_TYPE (to), from = TREE_TYPE (from))
    {
      if (TREE_CODE (to) != TREE_CODE (from))
	return false;

      if (TREE_CODE (from) == OFFSET_TYPE
	  && same_type_p (TYPE_OFFSET_BASETYPE (from),
			  TYPE_OFFSET_BASETYPE (to)))
	  continue;

      if (VECTOR_TYPE_P (to))
	is_opaque_pointer = vector_targets_convertible_p (to, from);

      if (TREE_CODE (to) == ARRAY_TYPE
	  /* Ignore cv-qualification, but if we see e.g. int[3] and int[4],
	     we must fail.  */
	  && !comp_array_types (to, from, cb, /*strict=*/false))
	return false;

      /* CWG 330 says we need to look through arrays.  */
      if (!TYPE_PTR_P (to) && TREE_CODE (to) != ARRAY_TYPE)
	return (is_opaque_pointer
		|| same_type_ignoring_top_level_qualifiers_p (to, from));
    }
}

/* Returns the type qualifiers for this type, including the qualifiers on the
   elements for an array type.  */

int
cp_type_quals (const_tree type)
{
  int quals;
  /* This CONST_CAST is okay because strip_array_types returns its
     argument unmodified and we assign it to a const_tree.  */
  type = strip_array_types (CONST_CAST_TREE (type));
  if (type == error_mark_node
      /* Quals on a FUNCTION_TYPE are memfn quals.  */
      || TREE_CODE (type) == FUNCTION_TYPE)
    return TYPE_UNQUALIFIED;
  quals = TYPE_QUALS (type);
  /* METHOD and REFERENCE_TYPEs should never have quals.  */
  gcc_assert ((TREE_CODE (type) != METHOD_TYPE
	       && !TYPE_REF_P (type))
	      || ((quals & (TYPE_QUAL_CONST|TYPE_QUAL_VOLATILE))
		  == TYPE_UNQUALIFIED));
  return quals;
}

/* Returns the function-ref-qualifier for TYPE */

cp_ref_qualifier
type_memfn_rqual (const_tree type)
{
  gcc_assert (FUNC_OR_METHOD_TYPE_P (type));

  if (!FUNCTION_REF_QUALIFIED (type))
    return REF_QUAL_NONE;
  else if (FUNCTION_RVALUE_QUALIFIED (type))
    return REF_QUAL_RVALUE;
  else
    return REF_QUAL_LVALUE;
}

/* Returns the function-cv-quals for TYPE, which must be a FUNCTION_TYPE or
   METHOD_TYPE.  */

int
type_memfn_quals (const_tree type)
{
  if (TREE_CODE (type) == FUNCTION_TYPE)
    return TYPE_QUALS (type);
  else if (TREE_CODE (type) == METHOD_TYPE)
    return cp_type_quals (class_of_this_parm (type));
  else
    gcc_unreachable ();
}

/* Returns the FUNCTION_TYPE TYPE with its function-cv-quals changed to
   MEMFN_QUALS and its ref-qualifier to RQUAL. */

tree
apply_memfn_quals (tree type, cp_cv_quals memfn_quals, cp_ref_qualifier rqual)
{
  /* Could handle METHOD_TYPE here if necessary.  */
  gcc_assert (TREE_CODE (type) == FUNCTION_TYPE);
  if (TYPE_QUALS (type) == memfn_quals
      && type_memfn_rqual (type) == rqual)
    return type;

  /* This should really have a different TYPE_MAIN_VARIANT, but that gets
     complex.  */
  tree result = build_qualified_type (type, memfn_quals);
  return build_ref_qualified_type (result, rqual);
}

/* Returns nonzero if TYPE is const or volatile.  */

bool
cv_qualified_p (const_tree type)
{
  int quals = cp_type_quals (type);
  return (quals & (TYPE_QUAL_CONST|TYPE_QUAL_VOLATILE)) != 0;
}

/* Returns nonzero if the TYPE contains a mutable member.  */

bool
cp_has_mutable_p (const_tree type)
{
  /* This CONST_CAST is okay because strip_array_types returns its
     argument unmodified and we assign it to a const_tree.  */
  type = strip_array_types (CONST_CAST_TREE(type));

  return CLASS_TYPE_P (type) && CLASSTYPE_HAS_MUTABLE (type);
}

/* Set TREE_READONLY and TREE_VOLATILE on DECL as indicated by the
   TYPE_QUALS.  For a VAR_DECL, this may be an optimistic
   approximation.  In particular, consider:

     int f();
     struct S { int i; };
     const S s = { f(); }

   Here, we will make "s" as TREE_READONLY (because it is declared
   "const") -- only to reverse ourselves upon seeing that the
   initializer is non-constant.  */

void
cp_apply_type_quals_to_decl (int type_quals, tree decl)
{
  tree type = TREE_TYPE (decl);

  if (type == error_mark_node)
    return;

  if (TREE_CODE (decl) == TYPE_DECL)
    return;

  gcc_assert (!(TREE_CODE (type) == FUNCTION_TYPE
		&& type_quals != TYPE_UNQUALIFIED));

  /* Avoid setting TREE_READONLY incorrectly.  */
  /* We used to check TYPE_NEEDS_CONSTRUCTING here, but now a constexpr
     constructor can produce constant init, so rely on cp_finish_decl to
     clear TREE_READONLY if the variable has non-constant init.  */

  /* If the type has (or might have) a mutable component, that component
     might be modified.  */
  if (TYPE_HAS_MUTABLE_P (type) || !COMPLETE_TYPE_P (type))
    type_quals &= ~TYPE_QUAL_CONST;

  c_apply_type_quals_to_decl (type_quals, decl);
}

/* Subroutine of casts_away_constness.  Make T1 and T2 point at
   exemplar types such that casting T1 to T2 is casting away constness
   if and only if there is no implicit conversion from T1 to T2.  */

static void
casts_away_constness_r (tree *t1, tree *t2, tsubst_flags_t complain)
{
  int quals1;
  int quals2;

  /* [expr.const.cast]

     For multi-level pointer to members and multi-level mixed pointers
     and pointers to members (conv.qual), the "member" aspect of a
     pointer to member level is ignored when determining if a const
     cv-qualifier has been cast away.  */
  /* [expr.const.cast]

     For  two  pointer types:

	    X1 is T1cv1,1 * ... cv1,N *   where T1 is not a pointer type
	    X2 is T2cv2,1 * ... cv2,M *   where T2 is not a pointer type
	    K is min(N,M)

     casting from X1 to X2 casts away constness if, for a non-pointer
     type T there does not exist an implicit conversion (clause
     _conv_) from:

	    Tcv1,(N-K+1) * cv1,(N-K+2) * ... cv1,N *

     to

	    Tcv2,(M-K+1) * cv2,(M-K+2) * ... cv2,M *.  */
  if ((!TYPE_PTR_P (*t1) && !TYPE_PTRDATAMEM_P (*t1))
      || (!TYPE_PTR_P (*t2) && !TYPE_PTRDATAMEM_P (*t2)))
    {
      *t1 = cp_build_qualified_type (void_type_node,
				     cp_type_quals (*t1));
      *t2 = cp_build_qualified_type (void_type_node,
				     cp_type_quals (*t2));
      return;
    }

  quals1 = cp_type_quals (*t1);
  quals2 = cp_type_quals (*t2);

  if (TYPE_PTRDATAMEM_P (*t1))
    *t1 = TYPE_PTRMEM_POINTED_TO_TYPE (*t1);
  else
    *t1 = TREE_TYPE (*t1);
  if (TYPE_PTRDATAMEM_P (*t2))
    *t2 = TYPE_PTRMEM_POINTED_TO_TYPE (*t2);
  else
    *t2 = TREE_TYPE (*t2);

  casts_away_constness_r (t1, t2, complain);
  *t1 = build_pointer_type (*t1);
  *t2 = build_pointer_type (*t2);
  *t1 = cp_build_qualified_type (*t1, quals1);
  *t2 = cp_build_qualified_type (*t2, quals2);
}

/* Returns nonzero if casting from TYPE1 to TYPE2 casts away
   constness.

   ??? This function returns non-zero if casting away qualifiers not
   just const.  We would like to return to the caller exactly which
   qualifiers are casted away to give more accurate diagnostics.
*/

static bool
casts_away_constness (tree t1, tree t2, tsubst_flags_t complain)
{
  if (TYPE_REF_P (t2))
    {
      /* [expr.const.cast]

	 Casting from an lvalue of type T1 to an lvalue of type T2
	 using a reference cast casts away constness if a cast from an
	 rvalue of type "pointer to T1" to the type "pointer to T2"
	 casts away constness.  */
      t1 = (TYPE_REF_P (t1) ? TREE_TYPE (t1) : t1);
      return casts_away_constness (build_pointer_type (t1),
				   build_pointer_type (TREE_TYPE (t2)),
				   complain);
    }

  if (TYPE_PTRDATAMEM_P (t1) && TYPE_PTRDATAMEM_P (t2))
    /* [expr.const.cast]

       Casting from an rvalue of type "pointer to data member of X
       of type T1" to the type "pointer to data member of Y of type
       T2" casts away constness if a cast from an rvalue of type
       "pointer to T1" to the type "pointer to T2" casts away
       constness.  */
    return casts_away_constness
      (build_pointer_type (TYPE_PTRMEM_POINTED_TO_TYPE (t1)),
       build_pointer_type (TYPE_PTRMEM_POINTED_TO_TYPE (t2)),
       complain);

  /* Casting away constness is only something that makes sense for
     pointer or reference types.  */
  if (!TYPE_PTR_P (t1) || !TYPE_PTR_P (t2))
    return false;

  /* Top-level qualifiers don't matter.  */
  t1 = TYPE_MAIN_VARIANT (t1);
  t2 = TYPE_MAIN_VARIANT (t2);
  casts_away_constness_r (&t1, &t2, complain);
  if (!can_convert (t2, t1, complain))
    return true;

  return false;
}

/* If T is a REFERENCE_TYPE return the type to which T refers.
   Otherwise, return T itself.  */

tree
non_reference (tree t)
{
  if (t && TYPE_REF_P (t))
    t = TREE_TYPE (t);
  return t;
}


/* Return nonzero if REF is an lvalue valid for this language;
   otherwise, print an error message and return zero.  USE says
   how the lvalue is being used and so selects the error message.  */

int
lvalue_or_else (tree ref, enum lvalue_use use, tsubst_flags_t complain)
{
  cp_lvalue_kind kind = lvalue_kind (ref);

  if (kind == clk_none)
    {
      if (complain & tf_error)
	lvalue_error (cp_expr_loc_or_input_loc (ref), use);
      return 0;
    }
  else if (kind & (clk_rvalueref|clk_class))
    {
      if (!(complain & tf_error))
	return 0;
      /* Make this a permerror because we used to accept it.  */
      permerror (cp_expr_loc_or_input_loc (ref),
		 "using rvalue as lvalue");
    }
  return 1;
}

/* Return true if a user-defined literal operator is a raw operator.  */

bool
check_raw_literal_operator (const_tree decl)
{
  tree argtypes = TYPE_ARG_TYPES (TREE_TYPE (decl));
  tree argtype;
  int arity;
  bool maybe_raw_p = false;

  /* Count the number and type of arguments and check for ellipsis.  */
  for (argtype = argtypes, arity = 0;
       argtype && argtype != void_list_node;
       ++arity, argtype = TREE_CHAIN (argtype))
    {
      tree t = TREE_VALUE (argtype);

      if (same_type_p (t, const_string_type_node))
	maybe_raw_p = true;
    }
  if (!argtype)
    return false; /* Found ellipsis.  */

  if (!maybe_raw_p || arity != 1)
    return false;

  return true;
}


/* Return true if a user-defined literal operator has one of the allowed
   argument types.  */

bool
check_literal_operator_args (const_tree decl,
			     bool *long_long_unsigned_p, bool *long_double_p)
{
  tree argtypes = TYPE_ARG_TYPES (TREE_TYPE (decl));

  *long_long_unsigned_p = false;
  *long_double_p = false;
  if (processing_template_decl || processing_specialization)
    return argtypes == void_list_node;
  else
    {
      tree argtype;
      int arity;
      int max_arity = 2;

      /* Count the number and type of arguments and check for ellipsis.  */
      for (argtype = argtypes, arity = 0;
	   argtype && argtype != void_list_node;
	   argtype = TREE_CHAIN (argtype))
	{
	  tree t = TREE_VALUE (argtype);
	  ++arity;

	  if (TYPE_PTR_P (t))
	    {
	      bool maybe_raw_p = false;
	      t = TREE_TYPE (t);
	      if (cp_type_quals (t) != TYPE_QUAL_CONST)
		return false;
	      t = TYPE_MAIN_VARIANT (t);
	      if ((maybe_raw_p = same_type_p (t, char_type_node))
		  || same_type_p (t, wchar_type_node)
		  || same_type_p (t, char8_type_node)
		  || same_type_p (t, char16_type_node)
		  || same_type_p (t, char32_type_node))
		{
		  argtype = TREE_CHAIN (argtype);
		  if (!argtype)
		    return false;
		  t = TREE_VALUE (argtype);
		  if (maybe_raw_p && argtype == void_list_node)
		    return true;
		  else if (same_type_p (t, size_type_node))
		    {
		      ++arity;
		      continue;
		    }
		  else
		    return false;
		}
	    }
	  else if (same_type_p (t, long_long_unsigned_type_node))
	    {
	      max_arity = 1;
	      *long_long_unsigned_p = true;
	    }
	  else if (same_type_p (t, long_double_type_node))
	    {
	      max_arity = 1;
	      *long_double_p = true;
	    }
	  else if (same_type_p (t, char_type_node))
	    max_arity = 1;
	  else if (same_type_p (t, wchar_type_node))
	    max_arity = 1;
	  else if (same_type_p (t, char8_type_node))
	    max_arity = 1;
	  else if (same_type_p (t, char16_type_node))
	    max_arity = 1;
	  else if (same_type_p (t, char32_type_node))
	    max_arity = 1;
	  else
	    return false;
	}
      if (!argtype)
	return false; /* Found ellipsis.  */

      if (arity != max_arity)
	return false;

      return true;
    }
}

/* Always returns false since unlike C90, C++ has no concept of implicit
   function declarations.  */

bool
c_decl_implicit (const_tree)
{
  return false;
}
