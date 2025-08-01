/* Subroutines for insn-output.cc for Tensilica's Xtensa architecture.
   Copyright (C) 2001-2025 Free Software Foundation, Inc.
   Contributed by Bob Wilson (bwilson@tensilica.com) at Tensilica.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#define IN_TARGET_CODE 1

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "gimple.h"
#include "cfghooks.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "stringpool.h"
#include "attribs.h"
#include "optabs.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "diagnostic-core.h"
#include "cfgrtl.h"
#include "output.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "calls.h"
#include "varasm.h"
#include "alias.h"
#include "explow.h"
#include "expr.h"
#include "langhooks.h"
#include "gimplify.h"
#include "builtins.h"
#include "dumpfile.h"
#include "hw-doloop.h"
#include "rtl-iter.h"
#include "insn-attr.h"
#include "tree-pass.h"
#include "print-rtl.h"
#include "context.h"
#include "pass_manager.h"
#include <math.h>
#include "opts.h"

/* This file should be included last.  */
#include "target-def.h"

/* Enumeration for all of the relational tests, so that we can build
   arrays indexed by the test type, and not worry about the order
   of EQ, NE, etc.  */

enum internal_test
{
  ITEST_EQ,
  ITEST_NE,
  ITEST_GT,
  ITEST_GE,
  ITEST_LT,
  ITEST_LE,
  ITEST_GTU,
  ITEST_GEU,
  ITEST_LTU,
  ITEST_LEU,
  ITEST_MAX
};

/* Array giving truth value on whether or not a given hard register
   can support a given mode.  */
static char xtensa_hard_regno_mode_ok_p
  [(int) MAX_MACHINE_MODE][FIRST_PSEUDO_REGISTER];

/* Largest block move to handle in-line.  */
#define LARGEST_MOVE_RATIO 15

/* Define the structure for the machine field in struct function.  */
struct GTY(()) machine_function
{
  int accesses_prev_frame;
  bool need_a7_copy;
  bool vararg_a7;
  rtx vararg_a7_copy;
  rtx_insn *set_frame_ptr_insn;
  /* Current frame size calculated by compute_frame_size.  */
  unsigned current_frame_size;
  /* Callee-save area size in the current frame calculated by
     compute_frame_size.  */
  int callee_save_size;
  bool frame_laid_out;
  bool inhibit_logues_a1_adjusts;
  rtx last_logues_a9_content;
  HARD_REG_SET eliminated_callee_saved;
  hash_map<rtx, int> *litpool_usage;
};

static void xtensa_option_override (void);
static void xtensa_option_override_after_change (void);
static enum internal_test map_test_to_internal_test (enum rtx_code);
static rtx gen_int_relational (enum rtx_code, rtx, rtx);
static rtx gen_float_relational (enum rtx_code, rtx, rtx);
static rtx gen_conditional_move (enum rtx_code, machine_mode, rtx, rtx);
static struct machine_function * xtensa_init_machine_status (void);
static rtx xtensa_legitimize_tls_address (rtx);
static rtx xtensa_legitimize_address (rtx, rtx, machine_mode);
static bool xtensa_mode_dependent_address_p (const_rtx, addr_space_t);
static bool xtensa_return_in_msb (const_tree);
static void printx (FILE *, signed int);
static rtx xtensa_builtin_saveregs (void);
static bool xtensa_legitimate_address_p (machine_mode, rtx, bool,
					 code_helper = ERROR_MARK);
static unsigned int xtensa_multibss_section_type_flags (tree, const char *,
							int) ATTRIBUTE_UNUSED;
static section *xtensa_select_rtx_section (machine_mode, rtx,
					   unsigned HOST_WIDE_INT);
static bool xtensa_rtx_costs (rtx, machine_mode, int, int, int *, bool);
static int xtensa_insn_cost (rtx_insn *, bool);
static int xtensa_register_move_cost (machine_mode, reg_class_t,
				      reg_class_t);
static tree xtensa_build_builtin_va_list (void);
static bool xtensa_return_in_memory (const_tree, const_tree);
static tree xtensa_gimplify_va_arg_expr (tree, tree, gimple_seq *,
					 gimple_seq *);
static void xtensa_function_arg_advance (cumulative_args_t,
					 const function_arg_info &);
static rtx xtensa_function_arg (cumulative_args_t, const function_arg_info &);
static rtx xtensa_function_incoming_arg (cumulative_args_t,
					 const function_arg_info &);
static rtx xtensa_function_value (const_tree, const_tree, bool);
static rtx xtensa_libcall_value (machine_mode, const_rtx);
static bool xtensa_function_value_regno_p (const unsigned int);
static unsigned int xtensa_function_arg_boundary (machine_mode,
						  const_tree);
static void xtensa_init_builtins (void);
static tree xtensa_fold_builtin (tree, int, tree *, bool);
static rtx xtensa_expand_builtin (tree, rtx, rtx, machine_mode, int);
static void xtensa_va_start (tree, rtx);
static bool xtensa_frame_pointer_required (void);
static rtx xtensa_static_chain (const_tree, bool);
static void xtensa_asm_trampoline_template (FILE *);
static void xtensa_trampoline_init (rtx, tree, rtx);
static bool xtensa_output_addr_const_extra (FILE *, rtx);
static bool xtensa_cannot_force_const_mem (machine_mode, rtx);
static machine_mode xtensa_promote_function_mode (const_tree,
						  machine_mode,
						  int *, const_tree,
						  int);

static reg_class_t xtensa_preferred_reload_class (rtx, reg_class_t);
static reg_class_t xtensa_preferred_output_reload_class (rtx, reg_class_t);
static reg_class_t xtensa_secondary_reload (bool, rtx, reg_class_t,
					    machine_mode,
					    struct secondary_reload_info *);

static bool constantpool_address_p (const_rtx addr);
static bool xtensa_legitimate_constant_p (machine_mode, rtx);
static void xtensa_reorg (void);
static bool xtensa_can_use_doloop_p (const widest_int &, const widest_int &,
				     unsigned int, bool);
static const char *xtensa_invalid_within_doloop (const rtx_insn *);

static bool xtensa_member_type_forces_blk (const_tree,
					   machine_mode mode);

static void xtensa_conditional_register_usage (void);
static unsigned int xtensa_hard_regno_nregs (unsigned int, machine_mode);
static bool xtensa_hard_regno_mode_ok (unsigned int, machine_mode);
static bool xtensa_modes_tieable_p (machine_mode, machine_mode);
static HOST_WIDE_INT xtensa_constant_alignment (const_tree, HOST_WIDE_INT);
static bool xtensa_can_eliminate (const int from ATTRIBUTE_UNUSED,
				  const int to);
static HOST_WIDE_INT xtensa_starting_frame_offset (void);
static unsigned HOST_WIDE_INT xtensa_asan_shadow_offset (void);
static bool xtensa_function_ok_for_sibcall (tree, tree);
static bool xtensa_can_output_mi_thunk (const_tree thunk_fndecl ATTRIBUTE_UNUSED,
					HOST_WIDE_INT delta ATTRIBUTE_UNUSED,
					HOST_WIDE_INT vcall_offset ATTRIBUTE_UNUSED,
					const_tree function ATTRIBUTE_UNUSED);
static void xtensa_output_mi_thunk (FILE *file, tree thunk ATTRIBUTE_UNUSED,
				    HOST_WIDE_INT delta,
				    HOST_WIDE_INT vcall_offset,
				    tree function);

static rtx xtensa_delegitimize_address (rtx);
static reg_class_t xtensa_ira_change_pseudo_allocno_class (int, reg_class_t,
							   reg_class_t);
static HARD_REG_SET xtensa_zero_call_used_regs (HARD_REG_SET);



/* These hooks specify assembly directives for creating certain kinds
   of integer object.  */

#undef TARGET_ASM_ALIGNED_SI_OP
#define TARGET_ASM_ALIGNED_SI_OP "\t.word\t"

#undef TARGET_ASM_SELECT_RTX_SECTION
#define TARGET_ASM_SELECT_RTX_SECTION  xtensa_select_rtx_section

#undef TARGET_LEGITIMIZE_ADDRESS
#define TARGET_LEGITIMIZE_ADDRESS xtensa_legitimize_address
#undef TARGET_MODE_DEPENDENT_ADDRESS_P
#define TARGET_MODE_DEPENDENT_ADDRESS_P xtensa_mode_dependent_address_p

#undef TARGET_REGISTER_MOVE_COST
#define TARGET_REGISTER_MOVE_COST xtensa_register_move_cost
#undef TARGET_RTX_COSTS
#define TARGET_RTX_COSTS xtensa_rtx_costs
#undef TARGET_INSN_COST
#define TARGET_INSN_COST xtensa_insn_cost
#undef TARGET_ADDRESS_COST
#define TARGET_ADDRESS_COST hook_int_rtx_mode_as_bool_0

#undef TARGET_MEMBER_TYPE_FORCES_BLK
#define TARGET_MEMBER_TYPE_FORCES_BLK xtensa_member_type_forces_blk

#undef TARGET_BUILD_BUILTIN_VA_LIST
#define TARGET_BUILD_BUILTIN_VA_LIST xtensa_build_builtin_va_list

#undef TARGET_EXPAND_BUILTIN_VA_START
#define TARGET_EXPAND_BUILTIN_VA_START xtensa_va_start

#undef TARGET_PROMOTE_FUNCTION_MODE
#define TARGET_PROMOTE_FUNCTION_MODE xtensa_promote_function_mode

#undef TARGET_RETURN_IN_MEMORY
#define TARGET_RETURN_IN_MEMORY xtensa_return_in_memory
#undef TARGET_FUNCTION_VALUE
#define TARGET_FUNCTION_VALUE xtensa_function_value
#undef TARGET_LIBCALL_VALUE
#define TARGET_LIBCALL_VALUE xtensa_libcall_value
#undef TARGET_FUNCTION_VALUE_REGNO_P
#define TARGET_FUNCTION_VALUE_REGNO_P xtensa_function_value_regno_p

#undef TARGET_SPLIT_COMPLEX_ARG
#define TARGET_SPLIT_COMPLEX_ARG hook_bool_const_tree_true
#undef TARGET_MUST_PASS_IN_STACK
#define TARGET_MUST_PASS_IN_STACK must_pass_in_stack_var_size
#undef TARGET_FUNCTION_ARG_ADVANCE
#define TARGET_FUNCTION_ARG_ADVANCE xtensa_function_arg_advance
#undef TARGET_FUNCTION_ARG
#define TARGET_FUNCTION_ARG xtensa_function_arg
#undef TARGET_FUNCTION_INCOMING_ARG
#define TARGET_FUNCTION_INCOMING_ARG xtensa_function_incoming_arg
#undef TARGET_FUNCTION_ARG_BOUNDARY
#define TARGET_FUNCTION_ARG_BOUNDARY xtensa_function_arg_boundary

#undef TARGET_EXPAND_BUILTIN_SAVEREGS
#define TARGET_EXPAND_BUILTIN_SAVEREGS xtensa_builtin_saveregs
#undef TARGET_GIMPLIFY_VA_ARG_EXPR
#define TARGET_GIMPLIFY_VA_ARG_EXPR xtensa_gimplify_va_arg_expr

#undef TARGET_RETURN_IN_MSB
#define TARGET_RETURN_IN_MSB xtensa_return_in_msb

#undef  TARGET_INIT_BUILTINS
#define TARGET_INIT_BUILTINS xtensa_init_builtins
#undef  TARGET_FOLD_BUILTIN
#define TARGET_FOLD_BUILTIN xtensa_fold_builtin
#undef  TARGET_EXPAND_BUILTIN
#define TARGET_EXPAND_BUILTIN xtensa_expand_builtin

#undef  TARGET_PREFERRED_RELOAD_CLASS
#define TARGET_PREFERRED_RELOAD_CLASS xtensa_preferred_reload_class
#undef  TARGET_PREFERRED_OUTPUT_RELOAD_CLASS
#define TARGET_PREFERRED_OUTPUT_RELOAD_CLASS xtensa_preferred_output_reload_class

#undef TARGET_SECONDARY_RELOAD
#define TARGET_SECONDARY_RELOAD xtensa_secondary_reload

#undef TARGET_HAVE_TLS
#define TARGET_HAVE_TLS HAVE_AS_TLS

#undef TARGET_CANNOT_FORCE_CONST_MEM
#define TARGET_CANNOT_FORCE_CONST_MEM xtensa_cannot_force_const_mem

#undef TARGET_LEGITIMATE_ADDRESS_P
#define TARGET_LEGITIMATE_ADDRESS_P	xtensa_legitimate_address_p

#undef TARGET_FRAME_POINTER_REQUIRED
#define TARGET_FRAME_POINTER_REQUIRED xtensa_frame_pointer_required

#undef TARGET_STATIC_CHAIN
#define TARGET_STATIC_CHAIN xtensa_static_chain
#undef TARGET_ASM_TRAMPOLINE_TEMPLATE
#define TARGET_ASM_TRAMPOLINE_TEMPLATE xtensa_asm_trampoline_template
#undef TARGET_TRAMPOLINE_INIT
#define TARGET_TRAMPOLINE_INIT xtensa_trampoline_init

#undef TARGET_OPTION_OVERRIDE
#define TARGET_OPTION_OVERRIDE xtensa_option_override

#undef TARGET_OVERRIDE_OPTIONS_AFTER_CHANGE
#define TARGET_OVERRIDE_OPTIONS_AFTER_CHANGE xtensa_option_override_after_change

#undef TARGET_ASM_OUTPUT_ADDR_CONST_EXTRA
#define TARGET_ASM_OUTPUT_ADDR_CONST_EXTRA xtensa_output_addr_const_extra

#undef TARGET_LEGITIMATE_CONSTANT_P
#define TARGET_LEGITIMATE_CONSTANT_P xtensa_legitimate_constant_p

#undef TARGET_MACHINE_DEPENDENT_REORG
#define TARGET_MACHINE_DEPENDENT_REORG xtensa_reorg

#undef TARGET_CAN_USE_DOLOOP_P
#define TARGET_CAN_USE_DOLOOP_P xtensa_can_use_doloop_p

#undef TARGET_INVALID_WITHIN_DOLOOP
#define TARGET_INVALID_WITHIN_DOLOOP xtensa_invalid_within_doloop

#undef TARGET_CONDITIONAL_REGISTER_USAGE
#define TARGET_CONDITIONAL_REGISTER_USAGE xtensa_conditional_register_usage

#undef TARGET_HARD_REGNO_NREGS
#define TARGET_HARD_REGNO_NREGS xtensa_hard_regno_nregs
#undef TARGET_HARD_REGNO_MODE_OK
#define TARGET_HARD_REGNO_MODE_OK xtensa_hard_regno_mode_ok

#undef TARGET_MODES_TIEABLE_P
#define TARGET_MODES_TIEABLE_P xtensa_modes_tieable_p

#undef TARGET_CONSTANT_ALIGNMENT
#define TARGET_CONSTANT_ALIGNMENT xtensa_constant_alignment

#undef TARGET_CAN_ELIMINATE
#define TARGET_CAN_ELIMINATE xtensa_can_eliminate

#undef TARGET_STARTING_FRAME_OFFSET
#define TARGET_STARTING_FRAME_OFFSET xtensa_starting_frame_offset

#undef TARGET_ASAN_SHADOW_OFFSET
#define TARGET_ASAN_SHADOW_OFFSET xtensa_asan_shadow_offset

#undef TARGET_HAVE_SPECULATION_SAFE_VALUE
#define TARGET_HAVE_SPECULATION_SAFE_VALUE speculation_safe_value_not_needed

#undef TARGET_DELEGITIMIZE_ADDRESS
#define TARGET_DELEGITIMIZE_ADDRESS xtensa_delegitimize_address

#undef TARGET_FUNCTION_OK_FOR_SIBCALL
#define TARGET_FUNCTION_OK_FOR_SIBCALL xtensa_function_ok_for_sibcall

#undef TARGET_ASM_CAN_OUTPUT_MI_THUNK
#define TARGET_ASM_CAN_OUTPUT_MI_THUNK xtensa_can_output_mi_thunk

#undef TARGET_ASM_OUTPUT_MI_THUNK
#define TARGET_ASM_OUTPUT_MI_THUNK xtensa_output_mi_thunk

#undef TARGET_MAX_ANCHOR_OFFSET
#define TARGET_MAX_ANCHOR_OFFSET 1020

#undef TARGET_DIFFERENT_ADDR_DISPLACEMENT_P
#define TARGET_DIFFERENT_ADDR_DISPLACEMENT_P hook_bool_void_true

#undef TARGET_IRA_CHANGE_PSEUDO_ALLOCNO_CLASS
#define TARGET_IRA_CHANGE_PSEUDO_ALLOCNO_CLASS xtensa_ira_change_pseudo_allocno_class

#undef TARGET_ZERO_CALL_USED_REGS
#define TARGET_ZERO_CALL_USED_REGS xtensa_zero_call_used_regs

struct gcc_target targetm = TARGET_INITIALIZER;


/* Functions to test Xtensa immediate operand validity.  */

bool
xtensa_simm8 (HOST_WIDE_INT v)
{
  return IN_RANGE (v, -128, 127);
}


bool
xtensa_simm8x256 (HOST_WIDE_INT v)
{
  return (v & 255) == 0 && IN_RANGE (v, -32768, 32512);
}


bool
xtensa_simm12b (HOST_WIDE_INT v)
{
  return IN_RANGE (v, -2048, 2047);
}


static bool
xtensa_uimm8 (HOST_WIDE_INT v)
{
  return IN_RANGE (v, 0, 255);
}


static bool
xtensa_uimm8x2 (HOST_WIDE_INT v)
{
  return (v & 1) == 0 && IN_RANGE (v, 0, 510);
}


static bool
xtensa_uimm8x4 (HOST_WIDE_INT v)
{
  return (v & 3) == 0 && IN_RANGE (v, 0, 1020);
}


bool
xtensa_b4const_or_zero (HOST_WIDE_INT v)
{
  switch (v)
    {
    case -1:
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 10:
    case 12:
    case 16:
    case 32:
    case 64:
    case 128:
    case 256:
      return true;
    }
  return false;
}


bool
xtensa_b4constu (HOST_WIDE_INT v)
{
  switch (v)
    {
    case 32768:
    case 65536:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 10:
    case 12:
    case 16:
    case 32:
    case 64:
    case 128:
    case 256:
      return true;
    }
  return false;
}


bool
xtensa_m1_or_1_thru_15 (HOST_WIDE_INT v)
{
  return v == -1 || IN_RANGE (v, 1, 15);
}


bool
xtensa_mask_immediate (HOST_WIDE_INT v)
{
  return IN_RANGE (exact_log2 (v + 1), 1, 16);
}


/* This is just like the standard true_regnum() function except that it
   works even when reg_renumber is not initialized.  */

int
xt_true_regnum (rtx x)
{
  if (REG_P (x))
    {
      if (! HARD_REGISTER_P (x)
	  && reg_renumber
	  && (lra_in_progress || reg_renumber[REGNO (x)] >= 0))
	return reg_renumber[REGNO (x)];
      return REGNO (x);
    }
  if (SUBREG_P (x))
    {
      int base = xt_true_regnum (SUBREG_REG (x));

      if (base >= 0
	  && HARD_REGISTER_NUM_P (base))
	{
	  struct subreg_info info;

	  subreg_get_info (lra_in_progress
			   ? (unsigned) base : REGNO (SUBREG_REG (x)),
			   GET_MODE (SUBREG_REG (x)),
			   SUBREG_BYTE (x), GET_MODE (x), &info);
	  if (info.representable_p)
	    return base + info.offset;
	}
    }
  return -1;
}


int
xtensa_valid_move (machine_mode mode, rtx *operands)
{
  /* Either the destination or source must be a register, and the
     MAC16 accumulator doesn't count.  */

  if (register_operand (operands[0], mode))
    {
      int dst_regnum = xt_true_regnum (operands[0]);

      if (xtensa_tls_referenced_p (operands[1]))
	return FALSE;

      /* The stack pointer can only be assigned with a MOVSP opcode.  */
      if (dst_regnum == STACK_POINTER_REGNUM)
	return !TARGET_WINDOWED_ABI
	  || (mode == SImode
	      && register_operand (operands[1], mode)
	      && !ACC_REG_P (xt_true_regnum (operands[1])));

      if (!ACC_REG_P (dst_regnum))
	return true;
    }
  if (register_operand (operands[1], mode))
    {
      int src_regnum = xt_true_regnum (operands[1]);
      if (!ACC_REG_P (src_regnum))
	return true;
    }
  return FALSE;
}


int
smalloffset_mem_p (rtx op)
{
  if (MEM_P (op))
    {
      rtx addr = XEXP (op, 0);
      if (REG_P (addr))
	return BASE_REG_P (addr, 0);
      if (GET_CODE (addr) == PLUS)
	{
	  rtx offset = XEXP (addr, 0);
	  HOST_WIDE_INT val;
	  if (! CONST_INT_P (offset))
	    offset = XEXP (addr, 1);
	  if (! CONST_INT_P (offset))
	    return FALSE;

	  val = INTVAL (offset);
	  return (val & 3) == 0 && IN_RANGE (val, 0, 60);
	}
    }
  return FALSE;
}


static bool
constantpool_address_p (const_rtx addr)
{
  const_rtx sym = addr;

  if (GET_CODE (addr) == CONST)
    {
      rtx offset;

      /* Only handle (PLUS (SYM, OFFSET)) form.  */
      addr = XEXP (addr, 0);
      if (GET_CODE (addr) != PLUS)
	return false;

      /* Make sure the address is word aligned.  */
      offset = XEXP (addr, 1);
      if (! CONST_INT_P (offset)
	  || (INTVAL (offset) & 3) != 0)
	return false;

      sym = XEXP (addr, 0);
    }

  if (SYMBOL_REF_P (sym)
      && CONSTANT_POOL_ADDRESS_P (sym))
    return true;

  return false;
}


int
constantpool_mem_p (rtx op)
{
  if (SUBREG_P (op))
    op = SUBREG_REG (op);
  if (MEM_P (op))
    return constantpool_address_p (XEXP (op, 0));
  return FALSE;
}


/* Return TRUE if X is a thread-local symbol.  */

static bool
xtensa_tls_symbol_p (rtx x)
{
  if (! targetm.have_tls)
    return false;

  return SYMBOL_REF_P (x) && SYMBOL_REF_TLS_MODEL (x) != 0;
}


void
xtensa_extend_reg (rtx dst, rtx src)
{
  rtx temp = gen_reg_rtx (SImode);
  rtx shift = GEN_INT (BITS_PER_WORD - GET_MODE_BITSIZE (GET_MODE (src)));

  /* Generate paradoxical subregs as needed so that the modes match.  */
  src = simplify_gen_subreg (SImode, src, GET_MODE (src), 0);
  dst = simplify_gen_subreg (SImode, dst, GET_MODE (dst), 0);

  emit_insn (gen_ashlsi3 (temp, src, shift));
  emit_insn (gen_ashrsi3 (dst, temp, shift));
}


bool
xtensa_mem_offset (unsigned v, machine_mode mode)
{
  switch (mode)
    {
    case E_BLKmode:
      /* Handle the worst case for block moves.  See xtensa_expand_block_move
	 where we emit an optimized block move operation if the block can be
	 moved in < "move_ratio" pieces.  The worst case is when the block is
	 aligned but has a size of (3 mod 4) (does this happen?) so that the
	 last piece requires a byte load/store.  */
      return (xtensa_uimm8 (v)
	      && xtensa_uimm8 (v + MOVE_MAX * LARGEST_MOVE_RATIO));

    case E_QImode:
      return xtensa_uimm8 (v);

    case E_HImode:
      return xtensa_uimm8x2 (v);

    case E_DImode:
    case E_DFmode:
      return (xtensa_uimm8x4 (v) && xtensa_uimm8x4 (v + 4));

    default:
      break;
    }

  return xtensa_uimm8x4 (v);
}


/* Make normal rtx_code into something we can index from an array.  */

static enum internal_test
map_test_to_internal_test (enum rtx_code test_code)
{
  enum internal_test test = ITEST_MAX;

  switch (test_code)
    {
    default:			break;
    case EQ:  test = ITEST_EQ;  break;
    case NE:  test = ITEST_NE;  break;
    case GT:  test = ITEST_GT;  break;
    case GE:  test = ITEST_GE;  break;
    case LT:  test = ITEST_LT;  break;
    case LE:  test = ITEST_LE;  break;
    case GTU: test = ITEST_GTU; break;
    case GEU: test = ITEST_GEU; break;
    case LTU: test = ITEST_LTU; break;
    case LEU: test = ITEST_LEU; break;
    }

  return test;
}


/* Generate the code to compare two integer values.  The return value is
   the comparison expression.  */

static rtx
gen_int_relational (enum rtx_code test_code, /* relational test (EQ, etc) */
		    rtx cmp0, /* first operand to compare */
		    rtx cmp1 /* second operand to compare */)
{
  struct cmp_info
  {
    enum rtx_code test_code;	/* test code to use in insn */
    bool (*const_range_p) (HOST_WIDE_INT); /* range check function */
    int const_add;		/* constant to add (convert LE -> LT) */
    int reverse_regs;		/* reverse registers in test */
    int invert_const;		/* != 0 if invert value if cmp1 is constant */
    int invert_reg;		/* != 0 if invert value if cmp1 is register */
    int unsignedp;		/* != 0 for unsigned comparisons.  */
  };

  static struct cmp_info info[ (int)ITEST_MAX ] = {

    { EQ,	xtensa_b4const_or_zero,	0, 0, 0, 0, 0 },	/* EQ  */
    { NE,	xtensa_b4const_or_zero,	0, 0, 0, 0, 0 },	/* NE  */

    { LT,	xtensa_b4const_or_zero,	1, 1, 1, 0, 0 },	/* GT  */
    { GE,	xtensa_b4const_or_zero,	0, 0, 0, 0, 0 },	/* GE  */
    { LT,	xtensa_b4const_or_zero,	0, 0, 0, 0, 0 },	/* LT  */
    { GE,	xtensa_b4const_or_zero,	1, 1, 1, 0, 0 },	/* LE  */

    { LTU,	xtensa_b4constu,	1, 1, 1, 0, 1 },	/* GTU */
    { GEU,	xtensa_b4constu,	0, 0, 0, 0, 1 },	/* GEU */
    { LTU,	xtensa_b4constu,	0, 0, 0, 0, 1 },	/* LTU */
    { GEU,	xtensa_b4constu,	1, 1, 1, 0, 1 },	/* LEU */
  };

  enum internal_test test;
  machine_mode mode;
  struct cmp_info *p_info;
  int invert;

  test = map_test_to_internal_test (test_code);
  gcc_assert (test != ITEST_MAX);

  p_info = &info[ (int)test ];

  mode = GET_MODE (cmp0);
  if (mode == VOIDmode)
    mode = GET_MODE (cmp1);

  /* Make sure we can handle any constants given to us.  */
  if (CONST_INT_P (cmp1))
    {
      HOST_WIDE_INT value = INTVAL (cmp1);
      unsigned HOST_WIDE_INT uvalue = (unsigned HOST_WIDE_INT)value;

      /* if the immediate overflows or does not fit in the immediate field,
	 spill it to a register */

      if ((p_info->unsignedp ?
	   (uvalue + p_info->const_add > uvalue) :
	   (value + p_info->const_add > value)) != (p_info->const_add > 0))
	{
	  cmp1 = force_reg (mode, cmp1);
	}
      else if (!(p_info->const_range_p) (value + p_info->const_add))
	{
	  cmp1 = force_reg (mode, cmp1);
	}
    }
  else if (! REG_P (cmp1) && ! SUBREG_P (cmp1))
    {
      cmp1 = force_reg (mode, cmp1);
    }

  /* See if we need to invert the result.  */
  invert = (CONST_INT_P (cmp1)
	    ? p_info->invert_const
	    : p_info->invert_reg);

  /* Comparison to constants, may involve adding 1 to change a LT into LE.
     Comparison between two registers, may involve switching operands.  */
  if (CONST_INT_P (cmp1))
    {
      if (p_info->const_add != 0)
	cmp1 = GEN_INT (INTVAL (cmp1) + p_info->const_add);

    }
  else if (p_info->reverse_regs)
    std::swap (cmp0, cmp1);

  return gen_rtx_fmt_ee (invert ? reverse_condition (p_info->test_code)
				: p_info->test_code,
			 VOIDmode, cmp0, cmp1);
}


/* Generate the code to compare two float values.  The return value is
   the comparison expression.  */

static rtx
gen_float_relational (enum rtx_code test_code, /* relational test (EQ, etc) */
		      rtx cmp0, /* first operand to compare */
		      rtx cmp1 /* second operand to compare */)
{
  rtx (*gen_fn) (rtx, rtx, rtx);
  rtx brtmp;
  int reverse_regs, invert;

  switch (test_code)
    {
    case EQ: reverse_regs = 0; invert = 0; gen_fn = gen_seq_sf; break;
    case NE: reverse_regs = 0; invert = 1; gen_fn = gen_seq_sf; break;
    case LE: reverse_regs = 0; invert = 0; gen_fn = gen_sle_sf; break;
    case GT: reverse_regs = 1; invert = 0; gen_fn = gen_slt_sf; break;
    case LT: reverse_regs = 0; invert = 0; gen_fn = gen_slt_sf; break;
    case GE: reverse_regs = 1; invert = 0; gen_fn = gen_sle_sf; break;
    case UNEQ: reverse_regs = 0; invert = 0; gen_fn = gen_suneq_sf; break;
    case LTGT: reverse_regs = 0; invert = 1; gen_fn = gen_suneq_sf; break;
    case UNLE: reverse_regs = 0; invert = 0; gen_fn = gen_sunle_sf; break;
    case UNGT: reverse_regs = 1; invert = 0; gen_fn = gen_sunlt_sf; break;
    case UNLT: reverse_regs = 0; invert = 0; gen_fn = gen_sunlt_sf; break;
    case UNGE: reverse_regs = 1; invert = 0; gen_fn = gen_sunle_sf; break;
    case UNORDERED:
      reverse_regs = 0; invert = 0; gen_fn = gen_sunordered_sf; break;
    case ORDERED:
      reverse_regs = 0; invert = 1; gen_fn = gen_sunordered_sf; break;
    default:
      fatal_insn ("bad test", gen_rtx_fmt_ee (test_code, VOIDmode, cmp0, cmp1));
      reverse_regs = 0; invert = 0; gen_fn = 0; /* avoid compiler warnings */
    }

  if (reverse_regs)
    std::swap (cmp0, cmp1);

  brtmp = gen_rtx_REG (CCmode, FPCC_REGNUM);
  emit_insn (gen_fn (brtmp, cmp0, cmp1));

  return gen_rtx_fmt_ee (invert ? EQ : NE, VOIDmode, brtmp, const0_rtx);
}


void
xtensa_expand_conditional_branch (rtx *operands, machine_mode mode)
{
  enum rtx_code test_code = GET_CODE (operands[0]);
  rtx cmp0 = operands[1];
  rtx cmp1 = operands[2];
  rtx cmp, label;

  switch (mode)
    {
    case E_SFmode:
      if (TARGET_HARD_FLOAT)
	{
	  cmp = gen_float_relational (test_code, cmp0, cmp1);
	  break;
	}
      /* FALLTHRU */

    case E_DFmode:
    default:
      fatal_insn ("bad test", gen_rtx_fmt_ee (test_code, VOIDmode, cmp0, cmp1));

    case E_SImode:
      cmp = gen_int_relational (test_code, cmp0, cmp1);
      break;
    }

  /* Generate the branch.  */
  label = gen_rtx_LABEL_REF (VOIDmode, operands[3]);
  emit_jump_insn (gen_rtx_SET (pc_rtx,
			       gen_rtx_IF_THEN_ELSE (VOIDmode, cmp,
						     label,
						     pc_rtx)));
}


static rtx
gen_conditional_move (enum rtx_code code, machine_mode mode,
		      rtx op0, rtx op1)
{
  if (mode == SImode)
    {
      rtx cmp;

      /* Jump optimization calls get_condition() which canonicalizes
	 comparisons like (GE x <const>) to (GT x <const-1>).
	 Transform those comparisons back to GE, since that is the
	 comparison supported in Xtensa.  We shouldn't have to
	 transform <LE x const> comparisons, because neither
	 xtensa_expand_conditional_branch() nor get_condition() will
	 produce them.  */

      if ((code == GT) && (op1 == constm1_rtx))
	{
	  code = GE;
	  op1 = const0_rtx;
	}
      cmp = gen_rtx_fmt_ee (code, VOIDmode, pc_rtx, const0_rtx);

      if (boolean_operator (cmp, VOIDmode))
	{
	  /* Swap the operands to make const0 second.  */
	  if (op0 == const0_rtx)
	    {
	      op0 = op1;
	      op1 = const0_rtx;
	    }

	  /* If not comparing against zero, emit a comparison (subtract).  */
	  if (op1 != const0_rtx)
	    {
	      op0 = expand_binop (SImode, sub_optab, op0, op1,
				  0, 0, OPTAB_LIB_WIDEN);
	      op1 = const0_rtx;
	    }
	}
      else if (branch_operator (cmp, VOIDmode))
	{
	  /* Swap the operands to make const0 second.  */
	  if (op0 == const0_rtx)
	    {
	      op0 = op1;
	      op1 = const0_rtx;

	      switch (code)
		{
		case LT: code = GE; break;
		case GE: code = LT; break;
		default: gcc_unreachable ();
		}
	    }

	  if (op1 != const0_rtx)
	    return 0;
	}
      else
	return 0;

      return gen_rtx_fmt_ee (code, VOIDmode, op0, op1);
    }

  if (TARGET_HARD_FLOAT && mode == SFmode)
    return gen_float_relational (code, op0, op1);

  return 0;
}


int
xtensa_expand_conditional_move (rtx *operands, int isflt)
{
  rtx dest = operands[0];
  rtx cmp = operands[1];
  machine_mode cmp_mode = GET_MODE (XEXP (cmp, 0));
  rtx (*gen_fn) (rtx, rtx, rtx, rtx, rtx);

  if (!(cmp = gen_conditional_move (GET_CODE (cmp), cmp_mode,
				    XEXP (cmp, 0), XEXP (cmp, 1))))
    return 0;

  if (isflt)
    gen_fn = (cmp_mode == SImode
	      ? gen_movsfcc_internal0
	      : gen_movsfcc_internal1);
  else
    gen_fn = (cmp_mode == SImode
	      ? gen_movsicc_internal0
	      : gen_movsicc_internal1);

  emit_insn (gen_fn (dest, XEXP (cmp, 0), operands[2], operands[3], cmp));
  return 1;
}


int
xtensa_expand_scc (rtx operands[4], machine_mode cmp_mode)
{
  rtx dest = operands[0];
  rtx cmp;
  rtx one_tmp, zero_tmp;
  rtx (*gen_fn) (rtx, rtx, rtx, rtx, rtx);

  if (cmp_mode == SImode && TARGET_SALT)
    {
      rtx a = operands[2], b = force_reg (SImode, operands[3]);
      enum rtx_code code = GET_CODE (operands[1]);
      bool invert_res = false;

      switch (code)
	{
	case GE:
	case GEU:
	  invert_res = true;
	  break;
	case GT:
	case GTU:
	  std::swap (a, b);
	  break;
	case LE:
	case LEU:
	  invert_res = true;
	  std::swap (a, b);
	  break;
	default:
	  break;
	}

      switch (code)
	{
	case GE:
	case GT:
	case LE:
	case LT:
	  emit_insn (gen_salt (dest, a, b));
	  if (!invert_res)
	    return 1;
	  break;
	case GEU:
	case GTU:
	case LEU:
	case LTU:
	  emit_insn (gen_saltu (dest, a, b));
	  if (!invert_res)
	    return 1;
	  break;
	default:
	  break;
	}

      if (invert_res)
	{
	  emit_insn (gen_negsi2 (dest, dest));
	  emit_insn (gen_addsi3 (dest, dest, const1_rtx));
	  return 1;
	}
    }

  if (! (cmp = gen_conditional_move (GET_CODE (operands[1]), cmp_mode,
				     operands[2], operands[3])))
    return 0;

  one_tmp = force_reg (SImode, const1_rtx);
  zero_tmp = force_reg (SImode, const0_rtx);

  gen_fn = (cmp_mode == SImode
	    ? gen_movsicc_internal0
	    : gen_movsicc_internal1);
  emit_insn (gen_fn (dest, XEXP (cmp, 0), one_tmp, zero_tmp, cmp));
  return 1;
}


/* Split OP[1] into OP[2,3] and likewise for OP[0] into OP[0,1].  MODE is
   for the output, i.e., the input operands are twice as big as MODE.  */

void
xtensa_split_operand_pair (rtx operands[4], machine_mode mode)
{
  switch (GET_CODE (operands[1]))
    {
    case REG:
      operands[3] = gen_rtx_REG (mode, REGNO (operands[1]) + 1);
      operands[2] = gen_rtx_REG (mode, REGNO (operands[1]));
      break;

    case MEM:
      operands[3] = adjust_address (operands[1], mode, GET_MODE_SIZE (mode));
      operands[2] = adjust_address (operands[1], mode, 0);
      break;

    case CONST_INT:
    case CONST_DOUBLE:
      split_double (operands[1], &operands[2], &operands[3]);
      break;

    default:
      gcc_unreachable ();
    }

  switch (GET_CODE (operands[0]))
    {
    case REG:
      operands[1] = gen_rtx_REG (mode, REGNO (operands[0]) + 1);
      operands[0] = gen_rtx_REG (mode, REGNO (operands[0]));
      break;

    case MEM:
      operands[1] = adjust_address (operands[0], mode, GET_MODE_SIZE (mode));
      operands[0] = adjust_address (operands[0], mode, 0);
      break;

    default:
      gcc_unreachable ();
    }
}


/* Try to emit insns to load src (either naked or pooled SI/SF constant)
   into dst with synthesizing a such constant value from a sequence of
   load-immediate / arithmetic ones, instead of a L32R instruction
   (plus a constant in litpool).  */

static int
xtensa_constantsynth_2insn (rtx dst, HOST_WIDE_INT srcval,
			    rtx (*gen_op)(rtx, HOST_WIDE_INT),
			    HOST_WIDE_INT op_imm)
{
  HOST_WIDE_INT imm = INT_MAX;
  rtx x = NULL_RTX;
  int shift, sqr;

  gcc_assert (REG_P (dst));

  shift = exact_log2 (srcval + 1);
  if (IN_RANGE (shift, 1, 31))
    {
      imm = -1;
      x = gen_lshrsi3 (dst, dst, GEN_INT (32 - shift));
    }

  shift = ctz_hwi (srcval);
  if ((!x || (TARGET_DENSITY && ! IN_RANGE (imm, -32, 95)))
      && xtensa_simm12b (srcval >> shift))
    {
      imm = srcval >> shift;
      x = gen_ashlsi3 (dst, dst, GEN_INT (shift));
    }

  if ((!x || (TARGET_DENSITY && ! IN_RANGE (imm, -32, 95)))
      && IN_RANGE (srcval, (-2048 - 32768), (2047 + 32512)))
    {
      HOST_WIDE_INT imm0, imm1;

      if (srcval < -32768)
	imm1 = -32768;
      else if (srcval > 32512)
	imm1 = 32512;
      else
	imm1 = srcval & ~255;
      imm0 = srcval - imm1;
      if (TARGET_DENSITY && imm1 < 32512 && IN_RANGE (imm0, 224, 255))
	imm0 -= 256, imm1 += 256;
      imm = imm0;
      x = gen_addsi3 (dst, dst, GEN_INT (imm1));
    }

  sqr = (int) floorf (sqrtf (srcval));
  if (TARGET_MUL32 && optimize_size
      && !x && IN_RANGE (srcval, 0, (2047 * 2047)) && sqr * sqr == srcval)
    {
      imm = sqr;
      x = gen_mulsi3 (dst, dst, dst);
    }

  if (!x)
    return 0;

  emit_move_insn (dst, GEN_INT (imm));
  emit_insn (x);
  if (gen_op)
    emit_move_insn (dst, gen_op (dst, op_imm));

  return 1;
}

static rtx
xtensa_constantsynth_rtx_SLLI (rtx reg, HOST_WIDE_INT imm)
{
  return gen_rtx_ASHIFT (SImode, reg, GEN_INT (imm));
}

static rtx
xtensa_constantsynth_rtx_ADDSUBX (rtx reg, HOST_WIDE_INT imm)
{
  return imm == 7
	 ? gen_rtx_MINUS (SImode, gen_rtx_ASHIFT (SImode, reg, GEN_INT (3)),
			  reg)
	 : gen_rtx_PLUS (SImode, gen_rtx_ASHIFT (SImode, reg,
						 GEN_INT (floor_log2 (imm - 1))),
			 reg);
}

int
xtensa_constantsynth (rtx dst, rtx src)
{
  HOST_WIDE_INT srcval;
  static opt_pass *pass_rtl_split2;
  int *pv;

  /* Derefer if src is litpool entry, and get integer constant value.  */
  src = avoid_constant_pool_reference (src);
  if (CONST_INT_P (src))
    srcval = INTVAL (src);
  else if (CONST_DOUBLE_P (src) && GET_MODE (src) == SFmode)
    {
      long l;

      REAL_VALUE_TO_TARGET_SINGLE (*CONST_DOUBLE_REAL_VALUE (src), l);
      srcval = (int32_t)l, src = GEN_INT (srcval);
    }
  else
    return 0;

  /* Force dst as SImode.  */
  gcc_assert (REG_P (dst));
  if (GET_MODE (dst) != SImode)
    dst = gen_rtx_REG (SImode, REGNO (dst));

  if (optimize_size)
    {
      /* During the first split pass after register allocation (rtl-split2),
	 record the occurrence of integer src value and do nothing.  */
      if (!pass_rtl_split2)
	pass_rtl_split2 = g->get_passes ()->get_pass_by_name ("rtl-split2");
      if (current_pass == pass_rtl_split2)
	{
	  if (!cfun->machine->litpool_usage)
	    cfun->machine->litpool_usage = hash_map<rtx, int>::create_ggc ();
	  if ((pv = cfun->machine->litpool_usage->get (src)))
	    ++*pv;
	  else
	    cfun->machine->litpool_usage->put (src, 1);
	  return 0;
	}

      /* If two or more identical integer constants appear in the function,
	 the code size can be reduced by re-emitting a "move" (load from an
	 either litpool entry or relaxed immediate) instruction in SImode
	 to increase the chances that the litpool entry will be shared.  */
      if (cfun->machine->litpool_usage
	  && (pv = cfun->machine->litpool_usage->get (src))
	  && *pv > 1)
	{
	  emit_move_insn (dst, src);
	  return 1;
	}
    }

  /* No need for synthesizing for what fits into MOVI instruction.  */
  if (xtensa_simm12b (srcval))
    {
      emit_move_insn (dst, src);
      return 1;
    }

  /* 2-insns substitution.  */
  if ((optimize_size || (optimize && xtensa_extra_l32r_costs >= 1))
      && xtensa_constantsynth_2insn (dst, srcval, NULL, 0))
    return 1;

  /* 3-insns substitution.  */
  if (optimize > 1 && !optimize_size && xtensa_extra_l32r_costs >= 2)
    {
      int shift, divisor;

      /* 2-insns substitution followed by SLLI.  */
      shift = ctz_hwi (srcval);
      if (IN_RANGE (shift, 1, 31) &&
	  xtensa_constantsynth_2insn (dst, srcval >> shift,
				      xtensa_constantsynth_rtx_SLLI,
				      shift))
	return 1;

      /* 2-insns substitution followed by ADDX[248] or SUBX8.  */
      if (TARGET_ADDX)
	for (divisor = 3; divisor <= 9; divisor += 2)
	  if (srcval % divisor == 0 &&
	      xtensa_constantsynth_2insn (dst, srcval / divisor,
					  xtensa_constantsynth_rtx_ADDSUBX,
					  divisor))
	    return 1;

      /* loading simm12 followed by left/right bitwise rotation:
	 MOVI + SSAI + SRC.  */
      if ((srcval & 0x001FF800) == 0
	  || (srcval & 0x001FF800) == 0x001FF800)
	{
	  int32_t v;

	  for (shift = 1; shift < 12; ++shift)
	    {
	      v = (int32_t)(((uint32_t)srcval >> shift)
			    | ((uint32_t)srcval << (32 - shift)));
	      if (xtensa_simm12b(v))
		{
		  emit_move_insn (dst, GEN_INT (v));
		  emit_insn (gen_rotlsi3 (dst, dst, GEN_INT (shift)));
		  return 1;
		}
	      v = (int32_t)(((uint32_t)srcval << shift)
			    | ((uint32_t)srcval >> (32 - shift)));
	      if (xtensa_simm12b(v))
		{
		  emit_move_insn (dst, GEN_INT (v));
		  emit_insn (gen_rotrsi3 (dst, dst, GEN_INT (shift)));
		  return 1;
		}
	    }
	}
    }

  /* If cannot synthesize the value and also cannot fit into MOVI instruc-
     tion, re-emit a "move" (load from an either litpool entry or relaxed
     immediate) instruction in SImode in order to increase the chances that
     the litpool entry will be shared.  */
  emit_move_insn (dst, src);
  return 1;
}


/* Emit insns to move operands[1] into operands[0].
   Return 1 if we have written out everything that needs to be done to
   do the move.  Otherwise, return 0 and the caller will emit the move
   normally.  */

int
xtensa_emit_move_sequence (rtx *operands, machine_mode mode)
{
  rtx src = operands[1];

  if (CONSTANT_P (src)
      && (! CONST_INT_P (src) || ! xtensa_simm12b (INTVAL (src))))
    {
      rtx dst = operands[0];

      if (xtensa_tls_referenced_p (src))
	{
	  rtx addend = NULL;

	  if (GET_CODE (src) == CONST && GET_CODE (XEXP (src, 0)) == PLUS)
	    {
	      addend = XEXP (XEXP (src, 0), 1);
	      src = XEXP (XEXP (src, 0), 0);
	    }

	  src = xtensa_legitimize_tls_address (src);
	  if (addend)
	    {
	      src = gen_rtx_PLUS (mode, src, addend);
	      src = force_operand (src, dst);
	    }
	  emit_move_insn (dst, src);
	  return 1;
	}

      if (! TARGET_AUTO_LITPOOLS && ! TARGET_CONST16
	  && ! (CONST_INT_P (src) && can_create_pseudo_p ()))
	{
	  src = force_const_mem (SImode, src);
	  operands[1] = src;
	}

      /* PC-relative loads are always SImode, and CONST16 is only
	 supported in the movsi pattern, so add a SUBREG for any other
	 (smaller) mode.  */

      if (mode != SImode)
	{
	  if (register_operand (dst, mode))
	    {
	      emit_move_insn (simplify_gen_subreg (SImode, dst, mode, 0), src);
	      return 1;
	    }
	  else
	    {
	      src = force_reg (SImode, src);
	      src = gen_lowpart_SUBREG (mode, src);
	      operands[1] = src;
	    }
	}
    }

  if (can_create_pseudo_p ()
      && !xtensa_valid_move (mode, operands))
    operands[1] = force_reg (mode, operands[1]);

  operands[1] = xtensa_copy_incoming_a7 (operands[1]);

  return 0;
}


/* Check if an incoming argument in a7 is expected to be used soon and
   if OPND is a register or register pair that includes a7.  If so,
   create a new pseudo and copy a7 into that pseudo at the very
   beginning of the function, followed by the special "set_frame_ptr"
   unspec_volatile insn.  The return value is either the original
   operand, if it is not a7, or the new pseudo containing a copy of
   the incoming argument.  This is necessary because the register
   allocator will ignore conflicts with a7 and may either assign some
   other pseudo to a7 or use a7 as the hard_frame_pointer, clobbering
   the incoming argument in a7.  By copying the argument out of a7 as
   the very first thing, and then immediately following that with an
   unspec_volatile to keep the scheduler away, we should avoid any
   problems.  Putting the set_frame_ptr insn at the beginning, with
   only the a7 copy before it, also makes it easier for the prologue
   expander to initialize the frame pointer after the a7 copy and to
   fix up the a7 copy to use the stack pointer instead of the frame
   pointer.  */

rtx
xtensa_copy_incoming_a7 (rtx opnd)
{
  rtx entry_insns = 0;
  rtx reg, tmp;
  machine_mode mode;

  if (!cfun->machine->need_a7_copy)
    return opnd;

  /* This function should never be called again once a7 has been copied.  */
  gcc_assert (!cfun->machine->set_frame_ptr_insn);

  mode = GET_MODE (opnd);

  /* The operand using a7 may come in a later instruction, so just return
     the original operand if it doesn't use a7.  */
  reg = opnd;
  if (SUBREG_P (reg))
    {
      gcc_assert (SUBREG_BYTE (reg) == 0);
      reg = SUBREG_REG (reg);
    }
  if (! REG_P (reg)
      || REGNO (reg) > A7_REG
      || REGNO (reg) + hard_regno_nregs (A7_REG, mode) <= A7_REG)
    return opnd;

  /* 1-word args will always be in a7; 2-word args in a6/a7.  */
  gcc_assert (REGNO (reg) + hard_regno_nregs (A7_REG, mode) - 1 == A7_REG);

  cfun->machine->need_a7_copy = false;

  /* Copy a7 to a new pseudo at the function entry.  Use gen_raw_REG to
     create the REG for a7 so that hard_frame_pointer_rtx is not used.  */

  start_sequence ();
  tmp = gen_reg_rtx (mode);

  switch (mode)
    {
    case E_DFmode:
    case E_DImode:
      /* Copy the value out of A7 here but keep the first word in A6 until
	 after the set_frame_ptr insn.  Otherwise, the register allocator
	 may decide to put "subreg (tmp, 0)" in A7 and clobber the incoming
	 value.  */
      emit_insn (gen_movsi_internal (gen_rtx_SUBREG (SImode, tmp, 4),
				     gen_raw_REG (SImode, A7_REG)));
      break;
    case E_SFmode:
      emit_insn (gen_movsf_internal (tmp, gen_raw_REG (mode, A7_REG)));
      break;
    case E_SImode:
      emit_insn (gen_movsi_internal (tmp, gen_raw_REG (mode, A7_REG)));
      break;
    case E_HImode:
      emit_insn (gen_movhi_internal (tmp, gen_raw_REG (mode, A7_REG)));
      break;
    case E_QImode:
      emit_insn (gen_movqi_internal (tmp, gen_raw_REG (mode, A7_REG)));
      break;
    default:
      gcc_unreachable ();
    }

  cfun->machine->set_frame_ptr_insn = emit_insn (gen_set_frame_ptr ());

  /* For DF and DI mode arguments, copy the incoming value in A6 now.  */
  if (mode == DFmode || mode == DImode)
    emit_insn (gen_movsi_internal (gen_rtx_SUBREG (SImode, tmp, 0),
				   gen_rtx_REG (SImode, A7_REG - 1)));
  entry_insns = end_sequence ();

  if (cfun->machine->vararg_a7)
    {
      /* This is called from within builtin_saveregs, which will insert the
	 saveregs code at the function entry, ahead of anything placed at
	 the function entry now.  Instead, save the sequence to be inserted
	 at the beginning of the saveregs code.  */
      cfun->machine->vararg_a7_copy = entry_insns;
    }
  else
    {
      /* Put entry_insns after the NOTE that starts the function.  If
	 this is inside a start_sequence, make the outer-level insn
	 chain current, so the code is placed at the start of the
	 function.  */
      push_topmost_sequence ();
      /* Do not use entry_of_function() here.  This is called from within
	 expand_function_start, when the CFG still holds GIMPLE.  */
      emit_insn_after (entry_insns, get_insns ());
      pop_topmost_sequence ();
    }

  return tmp;
}


/* Try to expand a block move operation to a sequence of RTL move
   instructions.  If not optimizing, or if the block size is not a
   constant, or if the block is too large, the expansion fails and GCC
   falls back to calling memcpy().

   operands[0] is the destination
   operands[1] is the source
   operands[2] is the length
   operands[3] is the alignment */

int
xtensa_expand_block_move (rtx *operands)
{
  static const machine_mode mode_from_align[] =
  {
    VOIDmode, QImode, HImode, VOIDmode, SImode,
  };

  rtx dst_mem = operands[0];
  rtx src_mem = operands[1];
  HOST_WIDE_INT bytes, align;
  int num_pieces, move_ratio;
  rtx temp[2];
  machine_mode mode[2];
  int amount[2];
  bool active[2];
  int phase = 0;
  int next;
  int offset_ld = 0;
  int offset_st = 0;
  rtx x;

  /* If this is not a fixed size move, just call memcpy.  */
  if (!optimize || ! CONST_INT_P (operands[2]))
    return 0;

  bytes = INTVAL (operands[2]);
  align = INTVAL (operands[3]);

  /* Anything to move?  */
  if (bytes <= 0)
    return 0;

  if (align > MOVE_MAX)
    align = MOVE_MAX;

  /* Decide whether to expand inline based on the optimization level.  */
  move_ratio = 4;
  if (optimize > 2)
    move_ratio = LARGEST_MOVE_RATIO;
  num_pieces = (bytes / align) + ((bytes % align + 1) / 2);
  if (num_pieces > move_ratio)
    return 0;

  x = XEXP (dst_mem, 0);
  if (!REG_P (x))
    {
      x = force_reg (Pmode, x);
      dst_mem = replace_equiv_address (dst_mem, x);
    }

  x = XEXP (src_mem, 0);
  if (!REG_P (x))
    {
      x = force_reg (Pmode, x);
      src_mem = replace_equiv_address (src_mem, x);
    }

  active[0] = active[1] = false;

  do
    {
      next = phase;
      phase ^= 1;

      if (bytes > 0)
	{
	  int next_amount;

	  next_amount = (bytes >= 4 ? 4 : (bytes >= 2 ? 2 : 1));
	  next_amount = MIN (next_amount, align);

	  amount[next] = next_amount;
	  mode[next] = mode_from_align[next_amount];
	  temp[next] = gen_reg_rtx (mode[next]);

	  x = adjust_address (src_mem, mode[next], offset_ld);
	  emit_move_insn (temp[next], x);

	  offset_ld += next_amount;
	  bytes -= next_amount;
	  active[next] = true;
	}

      if (active[phase])
	{
	  active[phase] = false;

	  x = adjust_address (dst_mem, mode[phase], offset_st);
	  emit_move_insn (x, temp[phase]);

	  offset_st += amount[phase];
	}
    }
  while (active[next]);

  return 1;
}


/* Worker function for xtensa_expand_block_set().

   Expand into an insn sequence that calls the "memset" function.  */

static rtx_insn *
xtensa_expand_block_set_libcall (rtx dst_mem,
				 HOST_WIDE_INT value,
				 HOST_WIDE_INT bytes)
{
  rtx reg;
  rtx_insn *seq;

  start_sequence ();

  reg = XEXP (dst_mem, 0);
  if (! REG_P (reg))
    reg = XEXP (replace_equiv_address (dst_mem,
				       force_reg (Pmode, reg)), 0);
  emit_library_call (gen_rtx_SYMBOL_REF (Pmode, "memset"),
		     LCT_NORMAL, VOIDmode,
		     reg, SImode,
		     GEN_INT (value), SImode,
		     GEN_INT (bytes), SImode);

  seq = end_sequence ();

  return seq;
}

/* Worker function for xtensa_expand_block_set().

   Expand into an insn sequence of one constant load followed by multiple
   memory stores.  Returns NULL if the conditions for expansion are not
   met.  */

static rtx_insn *
xtensa_expand_block_set_unrolled_loop (rtx dst_mem,
				       HOST_WIDE_INT value,
				       HOST_WIDE_INT bytes,
				       HOST_WIDE_INT align)
{
  rtx reg;
  int offset;
  rtx_insn *seq;

  if (bytes > 64)
    return NULL;

  start_sequence ();

  reg = XEXP (dst_mem, 0);
  if (! REG_P (reg))
    dst_mem = replace_equiv_address (dst_mem,
				     force_reg (Pmode, reg));
  switch (align)
    {
    case 1:
      break;
    case 2:
      value = (int16_t)((uint8_t)value * 0x0101U);
      break;
    case 4:
      value = (int32_t)((uint8_t)value * 0x01010101U);
      break;
    default:
      gcc_unreachable ();
    }
  reg = force_reg (SImode, GEN_INT (value));

  offset = 0;
  do
    {
      int unit_size = MIN (bytes, align);
      machine_mode unit_mode = (unit_size >= 4 ? SImode :
			       (unit_size >= 2 ? HImode : QImode));

      unit_size = GET_MODE_SIZE (unit_mode);
      emit_move_insn (adjust_address (dst_mem, unit_mode, offset),
		      (unit_mode == SImode) ? reg
		       : convert_to_mode (unit_mode, reg, true));
      offset += unit_size;
      bytes -= unit_size;
    }
  while (bytes > 0);

  seq = end_sequence ();

  return seq;
}

/* Worker function for xtensa_expand_block_set(),

   Expand into an insn sequence of a small loop that fill the memory
   range.  Returns NULL if the conditions for expansion are not met.  */

static rtx_insn *
xtensa_expand_block_set_small_loop (rtx dst_mem,
				    HOST_WIDE_INT value,
				    HOST_WIDE_INT bytes,
				    HOST_WIDE_INT align)
{
  HOST_WIDE_INT count;
  rtx reg, dst, end;
  machine_mode unit_mode;
  rtx_code_label *label;
  rtx_insn *seq;

  /* Totally-aligned block only.  */
  if (bytes % align != 0)
    return NULL;
  count = bytes / align;

  /* If the Loop Option (zero-overhead looping) is configured and active,
     almost no restrictions about the length of the block.  */
  if (! (TARGET_LOOPS && optimize))
    {
      /* If 4-byte aligned, small loop substitution is almost optimal,
	 thus limited to only offset to the end address for ADDI/ADDMI
	 instruction.  */
      if (align == 4
	  && ! (bytes <= 127 || xtensa_simm8x256 (bytes)))
	return NULL;

      /* If no 4-byte aligned, loop count should be treated as the
	 constraint.  */
      if (align != 4
	  && count > ((optimize > 1 && !optimize_size) ? 8 : 15))
	return NULL;
    }

  start_sequence ();

  reg = XEXP (dst_mem, 0);
  if (REG_P (reg))
    emit_move_insn (dst = gen_reg_rtx (SImode), reg);
  else
    dst = XEXP (replace_equiv_address (dst_mem,
				       force_reg (Pmode, reg)), 0);
  emit_insn (gen_addsi3 (end = gen_reg_rtx (SImode),
			 dst,
			 (TARGET_LOOPS && optimize)
			  ? force_reg (SImode, GEN_INT (bytes))
			  : GEN_INT (bytes)));
  switch (align)
    {
    case 1:
      unit_mode = QImode;
      break;
    case 2:
      value = (int16_t)((uint8_t)value * 0x0101U);
      unit_mode = HImode;
      break;
    case 4:
      value = (int32_t)((uint8_t)value * 0x01010101U);
      unit_mode = SImode;
      break;
    default:
      gcc_unreachable ();
    }
  reg = force_reg (unit_mode, GEN_INT (value));

  emit_label (label = gen_label_rtx ());
  emit_move_insn (gen_rtx_MEM (unit_mode, dst), reg);
  emit_insn (gen_addsi3 (dst, dst, GEN_INT (align)));
  emit_cmp_and_jump_insns (dst, end, NE, const0_rtx, SImode, true, label);

  seq = end_sequence ();

  return seq;
}


/* Try to expand a block set operation to a sequence of RTL move
   instructions.  If not optimizing, or if the block size is not a
   constant, or if the block is too large, or if the value to
   initialize the block with is not a constant, the expansion
   fails and GCC falls back to calling memset().

   operands[0] is the destination
   operands[1] is the length
   operands[2] is the initialization value
   operands[3] is the alignment */

int
xtensa_expand_block_set (rtx *operands)
{
  rtx dst_mem = operands[0];
  HOST_WIDE_INT bytes, value, align;
  rtx_insn *seq[3];
  int min_cost, min_index, i, n, cost;
  rtx_insn *insn;

  if (! CONST_INT_P (operands[1])
      || ! CONST_INT_P (operands[2])
      || (bytes = INTVAL (operands[1])) <= 0)
    return 0;

  value = (int8_t)INTVAL (operands[2]);
  align = INTVAL (operands[3]);
  if (align > MOVE_MAX)
    align = MOVE_MAX;

  /* Try to generate three equivalent insn sequences but method and
     size.  */
  seq[0] = xtensa_expand_block_set_libcall (dst_mem, value, bytes);
  seq[1] = xtensa_expand_block_set_unrolled_loop (dst_mem, value,
						  bytes, align);
  seq[2] = xtensa_expand_block_set_small_loop (dst_mem, value,
					       bytes, align);

  /* Find the sequence that has minimum size-basis insn costs.  */
  if (dump_file)
    fprintf (dump_file, "xtensa_expand_block_set:\n");
  min_cost = INT_MAX, min_index = 0;
  for (i = 0; i < 3; ++i)
    if ((insn = seq[i]))
      {
	if (dump_file)
	  fprintf (dump_file, " method %d...\n", i);

	for (n = 0, cost = 0; insn; insn = NEXT_INSN (insn))
	  {
	    if (active_insn_p (insn))
	      ++n, cost += xtensa_insn_cost (insn, false);
	    if (dump_file)
	      dump_insn_slim (dump_file, insn);
	  }

	/* Apply expansion bonus if -O2 or -O3 by discounting the cost
	   other than libcall.  */
	if (i > 0)
	  {
	    if (optimize == 2 && !optimize_size)
	      cost = (cost + 1) / 2;
	    else if (optimize >= 3)
	      cost = (cost + 2) / 4;
	  }

	if (dump_file)
	  fprintf (dump_file, "\t%d active insns, %d cost.\n", n, cost);

	if (cost < min_cost)
	  min_cost = cost, min_index = i;
      }
  if (dump_file)
    fprintf (dump_file, " choose method %d.\n", min_index);

  /* Fall back if libcall is minimum.  */
  if (min_index == 0)
    return 0;

  emit_insn (seq[min_index]);

  return 1;
}


void
xtensa_expand_nonlocal_goto (rtx *operands)
{
  rtx goto_handler = operands[1];
  rtx containing_fp = operands[3];

  /* Generate a call to "__xtensa_nonlocal_goto" (in libgcc); the code
     is too big to generate in-line.  */

  if (! REG_P (containing_fp))
    containing_fp = force_reg (Pmode, containing_fp);

  emit_library_call (gen_rtx_SYMBOL_REF (Pmode, "__xtensa_nonlocal_goto"),
		     LCT_NORMAL, VOIDmode,
		     containing_fp, Pmode,
		     goto_handler, Pmode);
}


static struct machine_function *
xtensa_init_machine_status (void)
{
  return ggc_cleared_alloc<machine_function> ();
}


/* Shift VAL of mode MODE left by COUNT bits.  */

static inline rtx
xtensa_expand_mask_and_shift (rtx val, machine_mode mode, rtx count)
{
  val = expand_simple_binop (SImode, AND, val, GEN_INT (GET_MODE_MASK (mode)),
			     NULL_RTX, 1, OPTAB_DIRECT);
  return expand_simple_binop (SImode, ASHIFT, val, count,
			      NULL_RTX, 1, OPTAB_DIRECT);
}


/* Structure to hold the initial parameters for a compare_and_swap operation
   in HImode and QImode.  */

struct alignment_context
{
  rtx memsi;	  /* SI aligned memory location.  */
  rtx shift;	  /* Bit offset with regard to lsb.  */
  rtx modemask;	  /* Mask of the HQImode shifted by SHIFT bits.  */
  rtx modemaski;  /* ~modemask */
};


/* Initialize structure AC for word access to HI and QI mode memory.  */

static void
init_alignment_context (struct alignment_context *ac, rtx mem)
{
  machine_mode mode = GET_MODE (mem);
  rtx byteoffset = NULL_RTX;
  bool aligned = (MEM_ALIGN (mem) >= GET_MODE_BITSIZE (SImode));

  if (aligned)
    ac->memsi = adjust_address (mem, SImode, 0); /* Memory is aligned.  */
  else
    {
      /* Alignment is unknown.  */
      rtx addr, align;

      /* Force the address into a register.  */
      addr = force_reg (Pmode, XEXP (mem, 0));

      /* Align it to SImode.  */
      align = expand_simple_binop (Pmode, AND, addr,
				   GEN_INT (-GET_MODE_SIZE (SImode)),
				   NULL_RTX, 1, OPTAB_DIRECT);
      /* Generate MEM.  */
      ac->memsi = gen_rtx_MEM (SImode, align);
      MEM_VOLATILE_P (ac->memsi) = MEM_VOLATILE_P (mem);
      set_mem_alias_set (ac->memsi, ALIAS_SET_MEMORY_BARRIER);
      set_mem_align (ac->memsi, GET_MODE_BITSIZE (SImode));

      byteoffset = expand_simple_binop (Pmode, AND, addr,
					GEN_INT (GET_MODE_SIZE (SImode) - 1),
					NULL_RTX, 1, OPTAB_DIRECT);
    }

  /* Calculate shiftcount.  */
  if (TARGET_BIG_ENDIAN)
    {
      ac->shift = GEN_INT (GET_MODE_SIZE (SImode) - GET_MODE_SIZE (mode));
      if (!aligned)
	ac->shift = expand_simple_binop (SImode, MINUS, ac->shift, byteoffset,
					 NULL_RTX, 1, OPTAB_DIRECT);
    }
  else
    {
      if (aligned)
	ac->shift = NULL_RTX;
      else
	ac->shift = byteoffset;
    }

  if (ac->shift != NULL_RTX)
    {
      /* Shift is the byte count, but we need the bitcount.  */
      gcc_assert (exact_log2 (BITS_PER_UNIT) >= 0);
      ac->shift = expand_simple_binop (SImode, ASHIFT, ac->shift,
				       GEN_INT (exact_log2 (BITS_PER_UNIT)),
				       NULL_RTX, 1, OPTAB_DIRECT);
      ac->modemask = expand_simple_binop (SImode, ASHIFT,
					  GEN_INT (GET_MODE_MASK (mode)),
					  ac->shift,
					  NULL_RTX, 1, OPTAB_DIRECT);
    }
  else
    ac->modemask = GEN_INT (GET_MODE_MASK (mode));

  ac->modemaski = expand_simple_unop (SImode, NOT, ac->modemask, NULL_RTX, 1);
}


/* Expand an atomic compare and swap operation for HImode and QImode.
   MEM is the memory location, CMP the old value to compare MEM with
   and NEW_RTX the value to set if CMP == MEM.  */

void
xtensa_expand_compare_and_swap (rtx target, rtx mem, rtx cmp, rtx new_rtx)
{
  machine_mode mode = GET_MODE (mem);
  struct alignment_context ac;
  rtx tmp, cmpv, newv, val;
  rtx oldval = gen_reg_rtx (SImode);
  rtx res = gen_reg_rtx (SImode);
  rtx_code_label *csloop = gen_label_rtx ();
  rtx_code_label *csend = gen_label_rtx ();

  init_alignment_context (&ac, mem);

  if (ac.shift != NULL_RTX)
    {
      cmp = xtensa_expand_mask_and_shift (cmp, mode, ac.shift);
      new_rtx = xtensa_expand_mask_and_shift (new_rtx, mode, ac.shift);
    }

  /* Load the surrounding word into VAL with the MEM value masked out.  */
  val = force_reg (SImode, expand_simple_binop (SImode, AND, ac.memsi,
						ac.modemaski, NULL_RTX, 1,
						OPTAB_DIRECT));
  emit_label (csloop);

  /* Patch CMP and NEW_RTX into VAL at correct position.  */
  cmpv = force_reg (SImode, expand_simple_binop (SImode, IOR, cmp, val,
						 NULL_RTX, 1, OPTAB_DIRECT));
  newv = force_reg (SImode, expand_simple_binop (SImode, IOR, new_rtx, val,
						 NULL_RTX, 1, OPTAB_DIRECT));

  /* Jump to end if we're done.  */
  emit_insn (gen_sync_compare_and_swapsi (res, ac.memsi, cmpv, newv));
  emit_cmp_and_jump_insns (res, cmpv, EQ, const0_rtx, SImode, true, csend);

  /* Check for changes outside mode.  */
  emit_move_insn (oldval, val);
  tmp = expand_simple_binop (SImode, AND, res, ac.modemaski,
			     val, 1, OPTAB_DIRECT);
  if (tmp != val)
    emit_move_insn (val, tmp);

  /* Loop internal if so.  */
  emit_cmp_and_jump_insns (oldval, val, NE, const0_rtx, SImode, true, csloop);

  emit_label (csend);

  /* Return the correct part of the bitfield.  */
  convert_move (target,
		(ac.shift == NULL_RTX ? res
		 : expand_simple_binop (SImode, LSHIFTRT, res, ac.shift,
					NULL_RTX, 1, OPTAB_DIRECT)),
		1);
}


/* Expand an atomic operation CODE of mode MODE (either HImode or QImode --
   the default expansion works fine for SImode).  MEM is the memory location
   and VAL the value to play with.  If AFTER is true then store the value
   MEM holds after the operation, if AFTER is false then store the value MEM
   holds before the operation.  If TARGET is zero then discard that value, else
   store it to TARGET.  */

void
xtensa_expand_atomic (enum rtx_code code, rtx target, rtx mem, rtx val,
		      bool after)
{
  machine_mode mode = GET_MODE (mem);
  struct alignment_context ac;
  rtx_code_label *csloop = gen_label_rtx ();
  rtx cmp, tmp;
  rtx old = gen_reg_rtx (SImode);
  rtx new_rtx = gen_reg_rtx (SImode);
  rtx orig = NULL_RTX;

  init_alignment_context (&ac, mem);

  /* Prepare values before the compare-and-swap loop.  */
  if (ac.shift != NULL_RTX)
    val = xtensa_expand_mask_and_shift (val, mode, ac.shift);
  switch (code)
    {
    case PLUS:
    case MINUS:
      orig = gen_reg_rtx (SImode);
      convert_move (orig, val, 1);
      break;

    case SET:
    case IOR:
    case XOR:
      break;

    case MULT: /* NAND */
    case AND:
      /* val = "11..1<val>11..1" */
      val = expand_simple_binop (SImode, XOR, val, ac.modemaski,
				 NULL_RTX, 1, OPTAB_DIRECT);
      break;

    default:
      gcc_unreachable ();
    }

  /* Load full word.  Subsequent loads are performed by S32C1I.  */
  cmp = force_reg (SImode, ac.memsi);

  emit_label (csloop);
  emit_move_insn (old, cmp);

  switch (code)
    {
    case PLUS:
    case MINUS:
      val = expand_simple_binop (SImode, code, old, orig,
				 NULL_RTX, 1, OPTAB_DIRECT);
      val = expand_simple_binop (SImode, AND, val, ac.modemask,
				 NULL_RTX, 1, OPTAB_DIRECT);
      /* FALLTHRU */
    case SET:
      tmp = expand_simple_binop (SImode, AND, old, ac.modemaski,
				 NULL_RTX, 1, OPTAB_DIRECT);
      tmp = expand_simple_binop (SImode, IOR, tmp, val,
				 new_rtx, 1, OPTAB_DIRECT);
      break;

    case AND:
    case IOR:
    case XOR:
      tmp = expand_simple_binop (SImode, code, old, val,
				 new_rtx, 1, OPTAB_DIRECT);
      break;

    case MULT: /* NAND */
      tmp = expand_simple_binop (SImode, AND, old, val,
				 NULL_RTX, 1, OPTAB_DIRECT);
      tmp = expand_simple_binop (SImode, XOR, tmp, ac.modemask,
				 new_rtx, 1, OPTAB_DIRECT);
      break;

    default:
      gcc_unreachable ();
    }

  if (tmp != new_rtx)
    emit_move_insn (new_rtx, tmp);
  emit_insn (gen_sync_compare_and_swapsi (cmp, ac.memsi, old, new_rtx));
  emit_cmp_and_jump_insns (cmp, old, NE, const0_rtx, SImode, true, csloop);

  if (target)
    {
      tmp = (after ? new_rtx : cmp);
      convert_move (target,
		    (ac.shift == NULL_RTX ? tmp
		     : expand_simple_binop (SImode, LSHIFTRT, tmp, ac.shift,
					    NULL_RTX, 1, OPTAB_DIRECT)),
		    1);
    }
}


void
xtensa_setup_frame_addresses (void)
{
  /* Set flag to cause TARGET_FRAME_POINTER_REQUIRED to return true.  */
  cfun->machine->accesses_prev_frame = 1;

  if (TARGET_WINDOWED_ABI)
    emit_library_call
      (gen_rtx_SYMBOL_REF (Pmode, "__xtensa_libgcc_window_spill"),
       LCT_NORMAL, VOIDmode);
}


/* Emit the assembly for the end of a zero-cost loop.  Normally we just emit
   a comment showing where the end of the loop is.  However, if there is a
   label or a branch at the end of the loop then we need to place a nop
   there.  If the loop ends with a label we need the nop so that branches
   targeting that label will target the nop (and thus remain in the loop),
   instead of targeting the instruction after the loop (and thus exiting
   the loop).  If the loop ends with a branch, we need the nop in case the
   branch is targeting a location inside the loop.  When the branch
   executes it will cause the loop count to be decremented even if it is
   taken (because it is the last instruction in the loop), so we need to
   nop after the branch to prevent the loop count from being decremented
   when the branch is taken.  */

void
xtensa_emit_loop_end (rtx_insn *insn, rtx *operands)
{
  char done = 0;

  for (insn = PREV_INSN (insn); insn && !done; insn = PREV_INSN (insn))
    {
      switch (GET_CODE (insn))
	{
	case NOTE:
	case BARRIER:
	  break;

	case CODE_LABEL:
	  output_asm_insn (TARGET_DENSITY ? "nop.n" : "nop", operands);
	  done = 1;
	  break;

	default:
	  {
	    rtx body = PATTERN (insn);

	    if (JUMP_P (body))
	      {
		output_asm_insn (TARGET_DENSITY ? "nop.n" : "nop", operands);
		done = 1;
	      }
	    else if ((GET_CODE (body) != USE)
		     && (GET_CODE (body) != CLOBBER))
	      done = 1;
	  }
	  break;
	}
    }

  output_asm_insn ("%1_LEND:", operands);
}


char *
xtensa_emit_branch (bool immed, rtx *operands)
{
  static char result[64];
  enum rtx_code code = GET_CODE (operands[3]);
  const char *op;

  switch (code)
    {
    case EQ:	op = "eq"; break;
    case NE:	op = "ne"; break;
    case LT:	op = "lt"; break;
    case GE:	op = "ge"; break;
    case LTU:	op = "ltu"; break;
    case GEU:	op = "geu"; break;
    default:	gcc_unreachable ();
    }

  if (immed)
    {
      if (INTVAL (operands[1]) == 0)
	sprintf (result, "b%sz%s\t%%0, %%2", op,
		 (TARGET_DENSITY && (code == EQ || code == NE)) ? ".n" : "");
      else
	sprintf (result, "b%si\t%%0, %%d1, %%2", op);
    }
  else
    sprintf (result, "b%s\t%%0, %%1, %%2", op);

  return result;
}


char *
xtensa_emit_movcc (bool inverted, bool isfp, bool isbool, rtx *operands)
{
  static char result[64];
  enum rtx_code code;
  const char *op;

  code = GET_CODE (operands[4]);
  if (inverted)
    code = reverse_condition (code);
  if (isbool)
    {
      switch (code)
	{
	case EQ:	op = "f"; break;
	case NE:	op = "t"; break;
	default:	gcc_unreachable ();
	}
    }
  else
    {
      switch (code)
	{
	case EQ:	op = "eqz"; break;
	case NE:	op = "nez"; break;
	case LT:	op = "ltz"; break;
	case GE:	op = "gez"; break;
	default:	gcc_unreachable ();
	}
    }

  sprintf (result, "mov%s%s\t%%0, %%%d, %%1",
	   op, isfp ? ".s" : "", inverted ? 3 : 2);
  return result;
}


void
xtensa_expand_call (int callop, rtx *operands)
{
  rtx call;
  rtx_insn *call_insn;
  rtx addr = XEXP (operands[callop], 0);

  if (flag_pic && SYMBOL_REF_P (addr)
      && (!SYMBOL_REF_LOCAL_P (addr) || SYMBOL_REF_EXTERNAL_P (addr)))
    addr = gen_sym_PLT (addr);

  if (!call_insn_operand (addr, VOIDmode))
    {
      /* This may be called while generating MI thunk when we pretend
	 that reload is over.  Use a8 as a temporary register in that case.  */
      rtx reg = can_create_pseudo_p ()
	? copy_to_mode_reg (Pmode, addr)
	: copy_to_suggested_reg (addr,
				 gen_rtx_REG (Pmode, A8_REG),
				 Pmode);
      XEXP (operands[callop], 0) = reg;
    }

  call = gen_rtx_CALL (VOIDmode, operands[callop], operands[callop + 1]);

  if (callop)
    call = gen_rtx_SET (operands[0], call);

  call_insn = emit_call_insn (call);

  if (TARGET_WINDOWED_ABI)
    {
      /*
       * Windowed xtensa ABI specifies that static chain pointer is passed
       * in memory below the caller's stack pointer, which means that the
       * callee may clobber it if it's a non-leaf function.
       * Add the clobber expression for the static chain to the function call
       * expression list so that it is not assumed to be live across the call.
       */
      rtx clob = gen_rtx_CLOBBER (Pmode, xtensa_static_chain (NULL, false));
      CALL_INSN_FUNCTION_USAGE (call_insn) =
	gen_rtx_EXPR_LIST (Pmode, clob, CALL_INSN_FUNCTION_USAGE (call_insn));
    }
}


char *
xtensa_emit_call (int callop, rtx *operands)
{
  static char result[64];
  rtx tgt = operands[callop];

  if (CONST_INT_P (tgt))
    sprintf (result, "call%d\t" HOST_WIDE_INT_PRINT_HEX,
	     WINDOW_SIZE, INTVAL (tgt));
  else if (register_operand (tgt, VOIDmode))
    sprintf (result, "callx%d\t%%%d", WINDOW_SIZE, callop);
  else
    sprintf (result, "call%d\t%%%d", WINDOW_SIZE, callop);

  return result;
}


char *
xtensa_emit_sibcall (int callop, rtx *operands)
{
  static char result[64];
  rtx tgt = operands[callop];

  if (CONST_INT_P (tgt))
    sprintf (result, "j.l\t" HOST_WIDE_INT_PRINT_HEX ", a9",
	     INTVAL (tgt));
  else if (register_operand (tgt, VOIDmode))
    sprintf (result, "jx\t%%%d", callop);
  else
    sprintf (result, "j.l\t%%%d, a9", callop);

  return result;
}

bool
xtensa_legitimate_address_p (machine_mode mode, rtx addr, bool strict,
			     code_helper)
{
  /* Allow constant pool addresses.  */
  if (mode != BLKmode
      && ! TARGET_CONST16 && constantpool_address_p (addr)
      && ! xtensa_tls_referenced_p (addr))
    return true;

  while (SUBREG_P (addr))
    addr = SUBREG_REG (addr);

  /* Allow base registers.  */
  if (REG_P (addr) && BASE_REG_P (addr, strict))
    return true;

  /* Check for "register + offset" addressing.  */
  if (GET_CODE (addr) == PLUS)
    {
      rtx xplus0 = XEXP (addr, 0);
      rtx xplus1 = XEXP (addr, 1);
      enum rtx_code code0;
      enum rtx_code code1;

      while (SUBREG_P (xplus0))
	xplus0 = SUBREG_REG (xplus0);
      code0 = GET_CODE (xplus0);

      while (SUBREG_P (xplus1))
	xplus1 = SUBREG_REG (xplus1);
      code1 = GET_CODE (xplus1);

      /* Swap operands if necessary so the register is first.  */
      if (code0 != REG && code1 == REG)
	{
	  xplus0 = XEXP (addr, 1);
	  xplus1 = XEXP (addr, 0);
	  code0 = GET_CODE (xplus0);
	  code1 = GET_CODE (xplus1);
	}

      if (code0 == REG && BASE_REG_P (xplus0, strict)
	  && code1 == CONST_INT
	  && xtensa_mem_offset (INTVAL (xplus1), mode))
	return true;
    }

  return false;
}


/* Construct the SYMBOL_REF for the _TLS_MODULE_BASE_ symbol.  */

static GTY(()) rtx xtensa_tls_module_base_symbol;

static rtx
xtensa_tls_module_base (void)
{
  if (! xtensa_tls_module_base_symbol)
    {
      xtensa_tls_module_base_symbol =
	gen_rtx_SYMBOL_REF (Pmode, "_TLS_MODULE_BASE_");
      SYMBOL_REF_FLAGS (xtensa_tls_module_base_symbol)
	|= TLS_MODEL_GLOBAL_DYNAMIC << SYMBOL_FLAG_TLS_SHIFT;
    }

  return xtensa_tls_module_base_symbol;
}


static rtx_insn *
xtensa_call_tls_desc (rtx sym, rtx *retp)
{
  rtx fn, arg, a_io;
  rtx_insn *call_insn, *insns;

  start_sequence ();
  fn = gen_reg_rtx (Pmode);
  arg = gen_reg_rtx (Pmode);
  a_io = gen_rtx_REG (Pmode, WINDOW_SIZE + 2);

  emit_insn (gen_tls_func (fn, sym));
  emit_insn (gen_tls_arg (arg, sym));
  emit_move_insn (a_io, arg);
  call_insn = emit_call_insn (gen_tls_call (a_io, fn, sym, const1_rtx));
  use_reg (&CALL_INSN_FUNCTION_USAGE (call_insn), a_io);
  insns = end_sequence ();

  *retp = a_io;
  return insns;
}


static rtx
xtensa_legitimize_tls_address (rtx x)
{
  unsigned int model = SYMBOL_REF_TLS_MODEL (x);
  rtx dest, tp, ret, modbase, base, addend;
  rtx_insn *insns;

  dest = gen_reg_rtx (Pmode);
  switch (model)
    {
    case TLS_MODEL_GLOBAL_DYNAMIC:
      insns = xtensa_call_tls_desc (x, &ret);
      emit_libcall_block (insns, dest, ret, x);
      break;

    case TLS_MODEL_LOCAL_DYNAMIC:
      base = gen_reg_rtx (Pmode);
      modbase = xtensa_tls_module_base ();
      insns = xtensa_call_tls_desc (modbase, &ret);
      emit_libcall_block (insns, base, ret, modbase);
      addend = force_reg (SImode, gen_sym_DTPOFF (x));
      emit_insn (gen_addsi3 (dest, base, addend));
      break;

    case TLS_MODEL_INITIAL_EXEC:
    case TLS_MODEL_LOCAL_EXEC:
      tp = gen_reg_rtx (SImode);
      emit_insn (gen_get_thread_pointersi (tp));
      addend = force_reg (SImode, gen_sym_TPOFF (x));
      emit_insn (gen_addsi3 (dest, tp, addend));
      break;

    default:
      gcc_unreachable ();
    }

  return dest;
}


rtx
xtensa_legitimize_address (rtx x,
			   rtx oldx ATTRIBUTE_UNUSED,
			   machine_mode mode)
{
  if (xtensa_tls_symbol_p (x))
    return xtensa_legitimize_tls_address (x);

  if (GET_CODE (x) == PLUS)
    {
      rtx plus0 = XEXP (x, 0);
      rtx plus1 = XEXP (x, 1);

      if (! REG_P (plus0) && REG_P (plus1))
	{
	  plus0 = XEXP (x, 1);
	  plus1 = XEXP (x, 0);
	}

      /* Try to split up the offset to use an ADDMI instruction.  */
      if (REG_P (plus0) && CONST_INT_P (plus1)
	  && !xtensa_mem_offset (INTVAL (plus1), mode)
	  && !xtensa_simm8 (INTVAL (plus1))
	  && xtensa_mem_offset (INTVAL (plus1) & 0xff, mode)
	  && xtensa_simm8x256 (INTVAL (plus1) & ~0xff))
	{
	  rtx temp = gen_reg_rtx (Pmode);
	  rtx addmi_offset = GEN_INT (INTVAL (plus1) & ~0xff);
	  emit_insn (gen_rtx_SET (temp, gen_rtx_PLUS (Pmode, plus0,
						      addmi_offset)));
	  return gen_rtx_PLUS (Pmode, temp, GEN_INT (INTVAL (plus1) & 0xff));
	}
    }

  return x;
}

/* Worker function for TARGET_MODE_DEPENDENT_ADDRESS_P.

   Treat constant-pool references as "mode dependent" since they can
   only be accessed with SImode loads.  This works around a bug in the
   combiner where a constant pool reference is temporarily converted
   to an HImode load, which is then assumed to zero-extend based on
   our definition of LOAD_EXTEND_OP.  This is wrong because the high
   bits of a 16-bit value in the constant pool are now sign-extended
   by default.  */

static bool
xtensa_mode_dependent_address_p (const_rtx addr,
				 addr_space_t as ATTRIBUTE_UNUSED)
{
  return constantpool_address_p (addr);
}

/* Return TRUE if X contains any TLS symbol references.  */

bool
xtensa_tls_referenced_p (rtx x)
{
  if (! targetm.have_tls)
    return false;

  subrtx_iterator::array_type array;
  FOR_EACH_SUBRTX (iter, array, x, ALL)
    {
      const_rtx x = *iter;
      if (SYMBOL_REF_P (x) && SYMBOL_REF_TLS_MODEL (x) != 0)
	return true;

      /* Ignore TLS references that have already been legitimized.  */
      if (GET_CODE (x) == UNSPEC)
	switch (XINT (x, 1))
	  {
	  case UNSPEC_TPOFF:
	  case UNSPEC_DTPOFF:
	  case UNSPEC_TLS_FUNC:
	  case UNSPEC_TLS_ARG:
	  case UNSPEC_TLS_CALL:
	    iter.skip_subrtxes ();
	    break;
	  default:
	    break;
	  }
    }
  return false;
}


/* Helper function for "*shlrd_..." patterns.  */

enum rtx_code
xtensa_shlrd_which_direction (rtx op0, rtx op1)
{
  if (GET_CODE (op0) == ASHIFT && GET_CODE (op1) == LSHIFTRT)
    return ASHIFT;	/* shld  */
  if (GET_CODE (op0) == LSHIFTRT && GET_CODE (op1) == ASHIFT)
    return LSHIFTRT;	/* shrd  */

  return UNKNOWN;
}


/* Return true after "split1" pass has been finished.  */

bool
xtensa_split1_finished_p (void)
{
  return cfun && (cfun->curr_properties & PROP_rtl_split_insns);
}


/* Split a DImode pair of reg (operand[0]) and const_int (operand[1]) into
   two SImode pairs, the low-part (operands[0] and [1]) and the high-part
   (operands[2] and [3]).  */

void
xtensa_split_DI_reg_imm (rtx *operands)
{
  rtx lowpart, highpart;

  if (WORDS_BIG_ENDIAN)
    split_double (operands[1], &highpart, &lowpart);
  else
    split_double (operands[1], &lowpart, &highpart);

  operands[3] = highpart;
  operands[2] = gen_highpart (SImode, operands[0]);
  operands[1] = lowpart;
  operands[0] = gen_lowpart (SImode, operands[0]);
}


/* Try to split an integer value into what are suitable for two consecutive
   immediate addition instructions, ADDI or ADDMI.  */

static bool
xtensa_split_imm_two_addends (HOST_WIDE_INT imm, HOST_WIDE_INT v[2])
{
  HOST_WIDE_INT v0, v1;

  if (imm < -32768)
    v0 = -32768, v1 = imm + 32768;
  else if (imm > 32512)
    v0 = 32512, v1 = imm - 32512;
  else if (TARGET_DENSITY && xtensa_simm12b (imm))
    /* A pair of MOVI(.N) and ADD.N is one or two bytes less than two
       immediate additions if TARGET_DENSITY.  */
    return false;
  else
    v0 = (imm + 128) & ~255L, v1 = imm - v0;

  if (xtensa_simm8 (v1) || xtensa_simm8x256 (v1))
    {
      v[0] = v0, v[1] = v1;
      return true;
    }

  return false;
}


/* Helper function for integer immediate addition with scratch register
   as needed, that splits and emits either up to two ADDI/ADDMI machine
   instructions or an addition by register following an integer immediate
   load (which may later be transformed by constantsynth).

   If 'scratch' is NULL_RTX but still needed, a new pseudo-register will
   be allocated.  Thus, after the reload/LRA pass, the specified scratch
   register must be a hard one.  */

static bool
xtensa_emit_add_imm (rtx dst, rtx src, HOST_WIDE_INT imm, rtx scratch,
		     bool need_note)
{
  bool retval = false;
  HOST_WIDE_INT v[2];
  rtx_insn *insn;

  if (imm == 0)
    return false;

  if (xtensa_simm8 (imm) || xtensa_simm8x256 (imm))
    insn = emit_insn (gen_addsi3 (dst, src, GEN_INT (imm)));
  else if (xtensa_split_imm_two_addends (imm, v))
    {
      if (!scratch)
	scratch = gen_reg_rtx (SImode);
      emit_insn (gen_addsi3 (scratch, src, GEN_INT (v[0])));
      insn = emit_insn (gen_addsi3 (dst, scratch, GEN_INT (v[1])));
    }
  else
    {
      if (scratch)
	emit_move_insn (scratch, GEN_INT (imm));
      else
	scratch = force_reg (SImode, GEN_INT (imm));
      retval = true;
      insn = emit_insn (gen_addsi3 (dst, src, scratch));
    }

  if (need_note)
    {
      rtx note_rtx = gen_rtx_SET (dst, plus_constant (Pmode, src, imm));

      RTX_FRAME_RELATED_P (insn) = 1;
      add_reg_note (insn, REG_FRAME_RELATED_EXPR, note_rtx);
    }

  return retval;
}


/* Implement TARGET_CANNOT_FORCE_CONST_MEM.  */

static bool
xtensa_cannot_force_const_mem (machine_mode mode ATTRIBUTE_UNUSED, rtx x)
{
  return xtensa_tls_referenced_p (x);
}


/* Return the debugger register number to use for 'regno'.  */

int
xtensa_debugger_regno (int regno)
{
  int first = -1;

  if (GP_REG_P (regno))
    {
      regno -= GP_REG_FIRST;
      first = 0;
    }
  else if (BR_REG_P (regno))
    {
      regno -= BR_REG_FIRST;
      first = 16;
    }
  else if (FP_REG_P (regno))
    {
      regno -= FP_REG_FIRST;
      first = 48;
    }
  else if (ACC_REG_P (regno))
    {
      first = 0x200;	/* Start of Xtensa special registers.  */
      regno = 16;	/* ACCLO is special register 16.  */
    }

  /* When optimizing, we sometimes get asked about pseudo-registers
     that don't represent hard registers.  Return 0 for these.  */
  if (first == -1)
    return 0;

  return first + regno;
}


/* Argument support functions.  */

/* Initialize CUMULATIVE_ARGS for a function.  */

void
init_cumulative_args (CUMULATIVE_ARGS *cum, int incoming)
{
  cum->arg_words = 0;
  cum->incoming = incoming;
}


/* Advance the argument to the next argument position.  */

static void
xtensa_function_arg_advance (cumulative_args_t cum,
			     const function_arg_info &arg)
{
  int words, max;
  int *arg_words;

  arg_words = &get_cumulative_args (cum)->arg_words;
  max = MAX_ARGS_IN_REGISTERS;

  words = ((arg.promoted_size_in_bytes () + UNITS_PER_WORD - 1)
	   / UNITS_PER_WORD);

  if (*arg_words < max
      && (targetm.calls.must_pass_in_stack (arg)
	  || *arg_words + words > max))
    *arg_words = max;

  *arg_words += words;
}


/* Return an RTL expression containing the register for the given argument,
   or 0 if the argument is to be passed on the stack.  INCOMING_P is nonzero
   if this is an incoming argument to the current function.  */

static rtx
xtensa_function_arg_1 (cumulative_args_t cum_v, const function_arg_info &arg,
		       bool incoming_p)
{
  CUMULATIVE_ARGS *cum = get_cumulative_args (cum_v);
  int regbase, words, max;
  int *arg_words;
  int regno;

  arg_words = &cum->arg_words;
  regbase = (incoming_p ? GP_ARG_FIRST : GP_OUTGOING_ARG_FIRST);
  max = MAX_ARGS_IN_REGISTERS;

  words = ((arg.promoted_size_in_bytes () + UNITS_PER_WORD - 1)
	   / UNITS_PER_WORD);

  if (arg.type && (TYPE_ALIGN (arg.type) > BITS_PER_WORD))
    {
      int align = MIN (TYPE_ALIGN (arg.type), STACK_BOUNDARY) / BITS_PER_WORD;
      *arg_words = (*arg_words + align - 1) & -align;
    }

  if (*arg_words + words > max)
    return (rtx)0;

  regno = regbase + *arg_words;

  if (cum->incoming && regno <= A7_REG && regno + words > A7_REG)
    cfun->machine->need_a7_copy = TARGET_WINDOWED_ABI;

  return gen_rtx_REG (arg.mode, regno);
}

/* Implement TARGET_FUNCTION_ARG.  */

static rtx
xtensa_function_arg (cumulative_args_t cum, const function_arg_info &arg)
{
  return xtensa_function_arg_1 (cum, arg, false);
}

/* Implement TARGET_FUNCTION_INCOMING_ARG.  */

static rtx
xtensa_function_incoming_arg (cumulative_args_t cum,
			      const function_arg_info &arg)
{
  return xtensa_function_arg_1 (cum, arg, true);
}

static unsigned int
xtensa_function_arg_boundary (machine_mode mode, const_tree type)
{
  unsigned int alignment;

  alignment = type ? TYPE_ALIGN (type) : GET_MODE_ALIGNMENT (mode);
  if (alignment < PARM_BOUNDARY)
    alignment = PARM_BOUNDARY;
  if (alignment > STACK_BOUNDARY)
    alignment = STACK_BOUNDARY;
  return alignment;
}


static bool
xtensa_return_in_msb (const_tree valtype)
{
  return (TARGET_BIG_ENDIAN
	  && AGGREGATE_TYPE_P (valtype)
	  && int_size_in_bytes (valtype) >= UNITS_PER_WORD);
}


static void
xtensa_option_override (void)
{
  int regno;
  machine_mode mode;

  if (xtensa_windowed_abi == -1)
    xtensa_windowed_abi = TARGET_WINDOWED_ABI_DEFAULT;

  if (xtensa_strict_alignment == XTENSA_STRICT_ALIGNMENT_UNDEFINED)
    xtensa_strict_alignment = !XCHAL_UNALIGNED_LOAD_HW
      || !XCHAL_UNALIGNED_STORE_HW;

  if (! TARGET_THREADPTR)
    targetm.have_tls = false;

  /* Use CONST16 in the absence of L32R.
     Set it in the TARGET_OPTION_OVERRIDE to avoid dependency on xtensa
     configuration in the xtensa-common.cc  */

  if (!TARGET_L32R)
    target_flags |= MASK_CONST16;

  if (!TARGET_BOOLEANS && TARGET_HARD_FLOAT)
    error ("boolean registers required for the floating-point option");

  /* Set up array giving whether a given register can hold a given mode.  */
  for (mode = VOIDmode;
       mode != MAX_MACHINE_MODE;
       mode = (machine_mode) ((int) mode + 1))
    {
      int size = GET_MODE_SIZE (mode);
      enum mode_class mclass = GET_MODE_CLASS (mode);

      for (regno = 0; regno < FIRST_PSEUDO_REGISTER; regno++)
	{
	  int temp;

	  if (ACC_REG_P (regno))
	    temp = (TARGET_MAC16
		    && (mclass == MODE_INT) && (size <= UNITS_PER_WORD));
	  else if (GP_REG_P (regno))
	    temp = ((regno & 1) == 0 || (size <= UNITS_PER_WORD));
	  else if (FP_REG_P (regno))
	    temp = (TARGET_HARD_FLOAT && (mode == SFmode));
	  else if (BR_REG_P (regno))
	    temp = (TARGET_BOOLEANS && (mode == CCmode));
	  else
	    temp = FALSE;

	  xtensa_hard_regno_mode_ok_p[(int) mode][regno] = temp;
	}
    }

  init_machine_status = xtensa_init_machine_status;

  /* Check PIC settings.  PIC is only supported when using L32R
     instructions, and some targets need to always use PIC.  */
  if (flag_pic && TARGET_CONST16)
    error ("%<-f%s%> is not supported with CONST16 instructions",
	   (flag_pic > 1 ? "PIC" : "pic"));
  else if (TARGET_FORCE_NO_PIC)
    flag_pic = 0;
  else if (XTENSA_ALWAYS_PIC)
    {
      if (TARGET_CONST16)
	error ("PIC is required but not supported with CONST16 instructions");
      flag_pic = 1;
    }
  /* There's no need for -fPIC (as opposed to -fpic) on Xtensa.  */
  if (flag_pic > 1)
    flag_pic = 1;
  if (flag_pic && !flag_pie)
    flag_shlib = 1;

  /* Hot/cold partitioning does not work on this architecture, because of
     constant pools (the load instruction cannot necessarily reach that far).
     Therefore disable it on this architecture.  */
  if (flag_reorder_blocks_and_partition)
    {
      flag_reorder_blocks_and_partition = 0;
      flag_reorder_blocks = 1;
    }

  /* One of the late-combine passes runs after register allocation
     and can match define_insn_and_splits that were previously used
     only before register allocation.  Some of those define_insn_and_splits
     require the split to take place, but have a split condition of
     can_create_pseudo_p, and so matching after RA will give an
     unsplittable instruction.  Disable late-combine by default until
     the define_insn_and_splits are fixed.  */
  if (!OPTION_SET_P (flag_late_combine_instructions))
    flag_late_combine_instructions = 0;

  xtensa_option_override_after_change ();
}

static void
xtensa_option_override_after_change (void)
{
  if (!OPTION_SET_P (flag_late_combine_instructions))
    flag_late_combine_instructions = 0;
}

/* Implement TARGET_HARD_REGNO_NREGS.  */

static unsigned int
xtensa_hard_regno_nregs (unsigned int regno, machine_mode mode)
{
  if (FP_REG_P (regno))
    return CEIL (GET_MODE_SIZE (mode), UNITS_PER_FPREG);
  return CEIL (GET_MODE_SIZE (mode), UNITS_PER_WORD);
}

/* Implement TARGET_HARD_REGNO_MODE_OK.  */

static bool
xtensa_hard_regno_mode_ok (unsigned int regno, machine_mode mode)
{
  return xtensa_hard_regno_mode_ok_p[mode][regno];
}

/* Implement TARGET_MODES_TIEABLE_P.  */

static bool
xtensa_modes_tieable_p (machine_mode mode1, machine_mode mode2)
{
  return ((GET_MODE_CLASS (mode1) == MODE_FLOAT
	   || GET_MODE_CLASS (mode1) == MODE_COMPLEX_FLOAT)
	  == (GET_MODE_CLASS (mode2) == MODE_FLOAT
	      || GET_MODE_CLASS (mode2) == MODE_COMPLEX_FLOAT));
}

/* A C compound statement to output to stdio stream STREAM the
   assembler syntax for an instruction operand X.  X is an RTL
   expression.

   CODE is a value that can be used to specify one of several ways
   of printing the operand.  It is used when identical operands
   must be printed differently depending on the context.  CODE
   comes from the '%' specification that was used to request
   printing of the operand.  If the specification was just '%DIGIT'
   then CODE is 0; if the specification was '%LTR DIGIT' then CODE
   is the ASCII code for LTR.

   If X is a register, this macro should print the register's name.
   The names can be found in an array 'reg_names' whose type is
   'char *[]'.  'reg_names' is initialized from 'REGISTER_NAMES'.

   When the machine description has a specification '%PUNCT' (a '%'
   followed by a punctuation character), this macro is called with
   a null pointer for X and the punctuation character for CODE.

   'a', 'c', 'l', and 'n' are reserved.

   The Xtensa specific codes are:

   'd'  CONST_INT, print as signed decimal
   'x'  CONST_INT, print as signed hexadecimal
   'K'  CONST_INT, print number of bits in mask for EXTUI
   'R'  CONST_INT, print (X & 0x1f)
   'L'  CONST_INT, print ((32 - X) & 0x1f)
   'U', CONST_DOUBLE:SF, print (REAL_EXP (rval) - 1)
   'V', CONST_DOUBLE:SF, print (1 - REAL_EXP (rval))
   'D'  REG, print second register of double-word register operand
   'N'  MEM, print address of next word following a memory operand
   'v'  MEM, if memory reference is volatile, output a MEMW before it
   't'  any constant, add "@h" suffix for top 16 bits
   'b'  any constant, add "@l" suffix for bottom 16 bits
*/

static void
printx (FILE *file, signed int val)
{
  /* Print a hexadecimal value in a nice way.  */
  if (IN_RANGE (val, -9, 9))
    fprintf (file, "%d", val);
  else if (val < 0)
    fprintf (file, "-0x%x", -val);
  else
    fprintf (file, "0x%x", val);
}


void
print_operand (FILE *file, rtx x, int letter)
{
  if (!x)
    error ("%<PRINT_OPERAND%> null pointer");

  switch (letter)
    {
    case 'D':
      if (REG_P (x) || SUBREG_P (x))
	fprintf (file, "%s", reg_names[xt_true_regnum (x) + 1]);
      else
	output_operand_lossage ("invalid %%D value");
      break;

    case 'v':
      if (MEM_P (x))
	{
	  /* For a volatile memory reference, emit a MEMW before the
	     load or store.  */
	  if (MEM_VOLATILE_P (x) && TARGET_SERIALIZE_VOLATILE)
	    {
	      rtx_insn *prev_insn
			= prev_nonnote_nondebug_insn (current_output_insn);
	      rtx pat, src;

	      if (! (prev_insn && NONJUMP_INSN_P (prev_insn)
		     && GET_CODE (pat = PATTERN (prev_insn)) == SET
		     && GET_CODE (src = SET_SRC (pat)) == UNSPEC
		     && XINT (src, 1) == UNSPEC_MEMW))
		fprintf (file, "memw\n\t");
	    }
	}
      else
	output_operand_lossage ("invalid %%v value");
      break;

    case 'N':
      if (MEM_P (x)
	  && (GET_MODE (x) == DFmode || GET_MODE (x) == DImode))
	{
	  x = adjust_address (x, GET_MODE (x) == DFmode ? E_SFmode : E_SImode,
			      4);
	  output_address (GET_MODE (x), XEXP (x, 0));
	}
      else
	output_operand_lossage ("invalid %%N value");
      break;

    case 'K':
      if (CONST_INT_P (x))
	{
	  unsigned val = INTVAL (x);
	  if (!xtensa_mask_immediate (val))
	    fatal_insn ("invalid mask", x);

	  fprintf (file, "%d", floor_log2 (val + 1));
	}
      else
	output_operand_lossage ("invalid %%K value");
      break;

    case 'L':
      if (CONST_INT_P (x))
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, (32 - INTVAL (x)) & 0x1f);
      else
	output_operand_lossage ("invalid %%L value");
      break;

    case 'R':
      if (CONST_INT_P (x))
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, INTVAL (x) & 0x1f);
      else
	output_operand_lossage ("invalid %%R value");
      break;

    case 'U':
      if (CONST_DOUBLE_P (x) && GET_MODE (x) == SFmode)
	fprintf (file, "%d", REAL_EXP (CONST_DOUBLE_REAL_VALUE (x)) - 1);
      else
	output_operand_lossage ("invalid %%U value");
      break;

    case 'V':
      if (CONST_DOUBLE_P (x) && GET_MODE (x) == SFmode)
	fprintf (file, "%d", 1 - REAL_EXP (CONST_DOUBLE_REAL_VALUE (x)));
      else
	output_operand_lossage ("invalid %%V value");
      break;

    case 'x':
      if (CONST_INT_P (x))
	printx (file, INTVAL (x));
      else
	output_operand_lossage ("invalid %%x value");
      break;

    case 'd':
      if (CONST_INT_P (x))
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, INTVAL (x));
      else
	output_operand_lossage ("invalid %%d value");
      break;

    case 't':
    case 'b':
      if (CONST_INT_P (x))
	{
	  printx (file, INTVAL (x));
	  fputs (letter == 't' ? "@h" : "@l", file);
	}
      else if (CONST_DOUBLE_P (x))
	{
	  if (GET_MODE (x) == SFmode)
	    {
	      long l;
	      REAL_VALUE_TO_TARGET_SINGLE (*CONST_DOUBLE_REAL_VALUE (x), l);
	      fprintf (file, "0x%08lx@%c", l, letter == 't' ? 'h' : 'l');
	    }
	  else
	    output_operand_lossage ("invalid %%t/%%b value");
	}
      else if (GET_CODE (x) == CONST)
	{
	  /* X must be a symbolic constant on ELF.  Write an expression
	     suitable for 'const16' that sets the high or low 16 bits.  */
	  if (GET_CODE (XEXP (x, 0)) != PLUS
	      || (! SYMBOL_REF_P (XEXP (XEXP (x, 0), 0))
		  && ! LABEL_REF_P (XEXP (XEXP (x, 0), 0)))
	      || ! CONST_INT_P (XEXP (XEXP (x, 0), 1)))
	    output_operand_lossage ("invalid %%t/%%b value");
	  print_operand (file, XEXP (XEXP (x, 0), 0), 0);
	  fputs (letter == 't' ? "@h" : "@l", file);
	  /* There must be a non-alphanumeric character between 'h' or 'l'
	     and the number.  The '-' is added by print_operand() already.  */
	  if (INTVAL (XEXP (XEXP (x, 0), 1)) >= 0)
	    fputs ("+", file);
	  print_operand (file, XEXP (XEXP (x, 0), 1), 0);
	}
      else
	{
	  output_addr_const (file, x);
	  fputs (letter == 't' ? "@h" : "@l", file);
	}
      break;

    case 'y':
      if (CONST_DOUBLE_P (x) && GET_MODE (x) == SFmode)
	{
	  long l;
	  REAL_VALUE_TO_TARGET_SINGLE (*CONST_DOUBLE_REAL_VALUE (x), l);
	  fprintf (file, "0x%08lx", l);
	  break;
	}

      /* fall through */

    default:
      if (REG_P (x) || SUBREG_P (x))
	fprintf (file, "%s", reg_names[xt_true_regnum (x)]);
      else if (MEM_P (x))
	output_address (GET_MODE (x), XEXP (x, 0));
      else if (CONST_INT_P (x))
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, INTVAL (x));
      else
	output_addr_const (file, x);
    }
}


/* A C compound statement to output to stdio stream STREAM the
   assembler syntax for an instruction operand that is a memory
   reference whose address is ADDR.  ADDR is an RTL expression.  */

void
print_operand_address (FILE *file, rtx addr)
{
  if (!addr)
    error ("%<PRINT_OPERAND_ADDRESS%>, null pointer");

  switch (GET_CODE (addr))
    {
    default:
      fatal_insn ("invalid address", addr);
      break;

    case REG:
      fprintf (file, "%s, 0", reg_names [REGNO (addr)]);
      break;

    case PLUS:
      {
	rtx reg = (rtx)0;
	rtx offset = (rtx)0;
	rtx arg0 = XEXP (addr, 0);
	rtx arg1 = XEXP (addr, 1);

	if (REG_P (arg0))
	  {
	    reg = arg0;
	    offset = arg1;
	  }
	else if (REG_P (arg1))
	  {
	    reg = arg1;
	    offset = arg0;
	  }
	else
	  fatal_insn ("no register in address", addr);

	if (CONSTANT_P (offset))
	  {
	    fprintf (file, "%s, ", reg_names [REGNO (reg)]);
	    output_addr_const (file, offset);
	  }
	else
	  fatal_insn ("address offset not a constant", addr);
      }
      break;

    case LABEL_REF:
    case SYMBOL_REF:
    case CONST_INT:
    case CONST:
      output_addr_const (file, addr);
      break;
    }
}

/* Implement TARGET_ASM_OUTPUT_ADDR_CONST_EXTRA.  */

static bool
xtensa_output_addr_const_extra (FILE *fp, rtx x)
{
  if (GET_CODE (x) == UNSPEC && XVECLEN (x, 0) == 1)
    {
      switch (XINT (x, 1))
	{
	case UNSPEC_TPOFF:
	  output_addr_const (fp, XVECEXP (x, 0, 0));
	  fputs ("@TPOFF", fp);
	  return true;
	case UNSPEC_DTPOFF:
	  output_addr_const (fp, XVECEXP (x, 0, 0));
	  fputs ("@DTPOFF", fp);
	  return true;
	case UNSPEC_PLT:
	  if (flag_pic)
	    {
	      output_addr_const (fp, XVECEXP (x, 0, 0));
	      fputs ("@PLT", fp);
	      return true;
	    }
	  break;
	default:
	  break;
	}
    }
  return false;
}

static void
xtensa_output_integer_literal_parts (FILE *file, rtx x, int size)
{
  if (size > 4 && !(size & (size - 1)))
    {
      rtx first, second;

      split_double (x, &first, &second);
      xtensa_output_integer_literal_parts (file, first, size / 2);
      fputs (", ", file);
      xtensa_output_integer_literal_parts (file, second, size / 2);
    }
  else if (size == 4 || size == 2)
    {
      output_addr_const (file, x);
    }
  else
    {
      gcc_unreachable();
    }
}

void
xtensa_output_literal (FILE *file, rtx x, machine_mode mode, int labelno)
{
  long value_long[2];

  fprintf (file, "\t.literal .LC%u, ", (unsigned) labelno);

  switch (GET_MODE_CLASS (mode))
    {
    case MODE_FLOAT:
      gcc_assert (CONST_DOUBLE_P (x));

      switch (mode)
	{
	case E_SFmode:
	  REAL_VALUE_TO_TARGET_SINGLE (*CONST_DOUBLE_REAL_VALUE (x),
				       value_long[0]);
	  if (HOST_BITS_PER_LONG > 32)
	    value_long[0] &= 0xffffffff;
	  fprintf (file, "0x%08lx\n", value_long[0]);
	  break;

	case E_DFmode:
	  REAL_VALUE_TO_TARGET_DOUBLE (*CONST_DOUBLE_REAL_VALUE (x),
				       value_long);
	  if (HOST_BITS_PER_LONG > 32)
	    {
	      value_long[0] &= 0xffffffff;
	      value_long[1] &= 0xffffffff;
	    }
	  fprintf (file, "0x%08lx, 0x%08lx\n",
		   value_long[0], value_long[1]);
	  break;

	default:
	  gcc_unreachable ();
	}

      break;

    case MODE_INT:
    case MODE_PARTIAL_INT:
      xtensa_output_integer_literal_parts (file, x, GET_MODE_SIZE (mode));
      fputs ("\n", file);
      break;

    default:
      gcc_unreachable ();
    }
}

static bool
xtensa_call_save_reg (int regno)
{
  if (TARGET_WINDOWED_ABI)
    return false;

  if (regno == A0_REG)
    /* Ensure the return address to be saved to the stack slot in order
       to assist stack dump analysis when -Og is specified.  */
    return optimize_debug
	   || crtl->profile || !crtl->is_leaf || crtl->calls_eh_return
	   || df_regs_ever_live_p (regno);

  if (crtl->calls_eh_return && IN_RANGE (regno, 2, 3))
    return true;

  return !call_used_or_fixed_reg_p (regno) && df_regs_ever_live_p (regno);
}

/* Return the bytes needed to compute the frame pointer from the current
   stack pointer.  */

#define STACK_BYTES (STACK_BOUNDARY / BITS_PER_UNIT)
#define XTENSA_STACK_ALIGN(LOC) (((LOC) + STACK_BYTES-1) & ~(STACK_BYTES-1))

long
compute_frame_size (poly_int64 size)
{
  int regno;

  if (reload_completed && cfun->machine->frame_laid_out)
    return cfun->machine->current_frame_size;

  /* Add space for the incoming static chain value.  */
  if (cfun->static_chain_decl != NULL)
    size += (1 * UNITS_PER_WORD);

  cfun->machine->callee_save_size = 0;
  for (regno = 0; regno < FIRST_PSEUDO_REGISTER; ++regno)
    {
      if (xtensa_call_save_reg (regno))
	cfun->machine->callee_save_size += UNITS_PER_WORD;
    }

  cfun->machine->current_frame_size =
    XTENSA_STACK_ALIGN (size
			+ cfun->machine->callee_save_size
			+ crtl->outgoing_args_size
			+ (WINDOW_SIZE * UNITS_PER_WORD));
  cfun->machine->callee_save_size =
    XTENSA_STACK_ALIGN (cfun->machine->callee_save_size);
  cfun->machine->frame_laid_out = true;
  return cfun->machine->current_frame_size;
}


bool
xtensa_frame_pointer_required (void)
{
  /* The code to expand builtin_frame_addr and builtin_return_addr
     currently uses the hard_frame_pointer instead of frame_pointer.
     This seems wrong but maybe it's necessary for other architectures.
     This function is derived from the i386 code.  */

  if (cfun->machine->accesses_prev_frame || cfun->has_nonlocal_label)
    return true;

  return false;
}

HOST_WIDE_INT
xtensa_initial_elimination_offset (int from, int to ATTRIBUTE_UNUSED)
{
  long frame_size = compute_frame_size (get_frame_size ());
  HOST_WIDE_INT offset;

  switch (from)
    {
    case FRAME_POINTER_REGNUM:
      if (FRAME_GROWS_DOWNWARD)
	offset = frame_size - (WINDOW_SIZE * UNITS_PER_WORD)
	  - cfun->machine->callee_save_size;
      else
	offset = 0;
      break;
    case ARG_POINTER_REGNUM:
      offset = frame_size;
      break;
    default:
      gcc_unreachable ();
    }

  return offset;
}

#define ADJUST_SP_NONE      0x0
#define ADJUST_SP_NEED_NOTE 0x1
#define ADJUST_SP_FRAME_PTR 0x2
static void
xtensa_emit_adjust_stack_ptr (HOST_WIDE_INT offset, int flags)
{
  rtx src, scratch;
  rtx_insn *insn;

  if (cfun->machine->inhibit_logues_a1_adjusts)
    return;

  src = (flags & ADJUST_SP_FRAME_PTR)
	 ? hard_frame_pointer_rtx : stack_pointer_rtx;
  scratch = gen_rtx_REG (Pmode, A9_REG);

  if (df && DF_REG_DEF_COUNT (A9_REG) == 0
      && cfun->machine->last_logues_a9_content
      && -INTVAL (cfun->machine->last_logues_a9_content) == offset)
    {
      insn = emit_insn (gen_subsi3 (stack_pointer_rtx, src, scratch));
      if (flags & ADJUST_SP_NEED_NOTE)
	{
	  rtx note_rtx = gen_rtx_SET (stack_pointer_rtx,
				      plus_constant (Pmode, src, offset));

	  RTX_FRAME_RELATED_P (insn) = 1;
	  add_reg_note (insn, REG_FRAME_RELATED_EXPR, note_rtx);
	}
    }
  else if (xtensa_emit_add_imm (stack_pointer_rtx, src, offset, scratch,
				(flags & ADJUST_SP_NEED_NOTE)))
    cfun->machine->last_logues_a9_content = GEN_INT (offset);
}

static bool
xtensa_can_eliminate_callee_saved_reg_p (unsigned int regno,
					 rtx_insn **p_insnS,
					 rtx_insn **p_insnR)
{
  df_ref ref;
  rtx_insn *insn, *insnS = NULL, *insnR = NULL;
  rtx pattern;

  if (!optimize || !df || call_used_or_fixed_reg_p (regno))
    return false;

  for (ref = DF_REG_DEF_CHAIN (regno);
       ref; ref = DF_REF_NEXT_REG (ref))
    if (DF_REF_CLASS (ref) != DF_REF_REGULAR
	|| DEBUG_INSN_P (insn = DF_REF_INSN (ref)))
      continue;
    else if (GET_CODE (pattern = PATTERN (insn)) == SET
	     && REG_P (SET_DEST (pattern))
	     && REGNO (SET_DEST (pattern)) == regno
	     && REG_NREGS (SET_DEST (pattern)) == 1
	     && REG_P (SET_SRC (pattern))
	     && REGNO (SET_SRC (pattern)) != A1_REG)
      {
	if (insnS)
	  return false;
	insnS = insn;
	continue;
      }
    else
      return false;

  for (ref = DF_REG_USE_CHAIN (regno);
       ref; ref = DF_REF_NEXT_REG (ref))
    if (DF_REF_CLASS (ref) != DF_REF_REGULAR
	|| DEBUG_INSN_P (insn = DF_REF_INSN (ref)))
      continue;
    else if (GET_CODE (pattern = PATTERN (insn)) == SET
	     && REG_P (SET_SRC (pattern))
	     && REGNO (SET_SRC (pattern)) == regno
	     && REG_NREGS (SET_SRC (pattern)) == 1
	     && REG_P (SET_DEST (pattern))
	     && REGNO (SET_DEST (pattern)) != A1_REG)
      {
	if (insnR)
	  return false;
	insnR = insn;
	continue;
      }
    else
      return false;

  if (!insnS || !insnR)
    return false;

  *p_insnS = insnS, *p_insnR = insnR;

  return true;
}

/* minimum frame = reg save area (4 words) plus static chain (1 word)
   and the total number of words must be a multiple of 128 bits.  */
#define MIN_FRAME_SIZE (8 * UNITS_PER_WORD)

void
xtensa_expand_prologue (void)
{
  HOST_WIDE_INT total_size;
  rtx_insn *insn = NULL;
  rtx note_rtx;

  total_size = compute_frame_size (get_frame_size ());

  if (flag_stack_usage_info)
    current_function_static_stack_size = total_size;

  if (TARGET_WINDOWED_ABI)
    {
      if (total_size < (1 << (12+3)))
	insn = emit_insn (gen_entry (GEN_INT (total_size)));
      else
	{
	  /* Use a8 as a temporary since a0-a7 may be live.  */
	  rtx tmp_reg = gen_rtx_REG (Pmode, A8_REG);
	  emit_insn (gen_entry (GEN_INT (MIN_FRAME_SIZE)));
	  xtensa_emit_add_imm (tmp_reg, stack_pointer_rtx,
			       MIN_FRAME_SIZE - total_size,
			       tmp_reg, false);
	  insn = emit_insn (gen_movsi (stack_pointer_rtx, tmp_reg));
	}
    }
  else
    {
      int regno;
      HOST_WIDE_INT offset = 0;
      int callee_save_size = cfun->machine->callee_save_size;
      df_ref ref;
      bool stack_pointer_needed = frame_pointer_needed
				  || crtl->calls_eh_return;
      bool large_stack_needed;

      /* Check if the function body really needs the stack pointer.  */
      if (!stack_pointer_needed && df)
	for (ref = DF_REG_USE_CHAIN (A1_REG);
	     ref; ref = DF_REF_NEXT_REG (ref))
	  if (DF_REF_CLASS (ref) == DF_REF_REGULAR
	      && NONJUMP_INSN_P (DF_REF_INSN (ref)))
	    {
	      stack_pointer_needed = true;
	      break;
	    }
      /* Check if callee-saved registers really need saving to the stack.  */
      if (!stack_pointer_needed)
	for (regno = 0; regno < FIRST_PSEUDO_REGISTER; ++regno)
	  if (xtensa_call_save_reg (regno))
	    {
	      stack_pointer_needed = true;
	      break;
	    }

      cfun->machine->inhibit_logues_a1_adjusts = !stack_pointer_needed;

      /* -128 is a limit of single addi instruction. */
      if (IN_RANGE (total_size, 1, 128))
	{
	  xtensa_emit_adjust_stack_ptr (-total_size,
					ADJUST_SP_NEED_NOTE);
	  offset = total_size - UNITS_PER_WORD;
	}
      else if (callee_save_size)
	{
	  /* 1020 is maximal s32i offset, if the frame is bigger than that
	   * we move sp to the end of callee-saved save area, save and then
	   * move it to its final location. */
	  if (total_size > 1024)
	    {
	      xtensa_emit_adjust_stack_ptr (-callee_save_size,
					    ADJUST_SP_NEED_NOTE);
	      offset = callee_save_size - UNITS_PER_WORD;
	    }
	  else
	    {
	      xtensa_emit_adjust_stack_ptr (-total_size,
					    ADJUST_SP_NEED_NOTE);
	      offset = total_size - UNITS_PER_WORD;
	    }
	}

      large_stack_needed = total_size > 1024
			   || (!callee_save_size && total_size > 128);
      for (regno = 0; regno < FIRST_PSEUDO_REGISTER; ++regno)
	if (xtensa_call_save_reg(regno))
	  {
	    rtx x = gen_rtx_PLUS (Pmode,
				  stack_pointer_rtx, GEN_INT (offset));
	    rtx mem = gen_frame_mem (SImode, x);
	    rtx_insn *insnS, *insnR;

	    if (!large_stack_needed
		&& xtensa_can_eliminate_callee_saved_reg_p (regno,
							    &insnS, &insnR))
	      {
		if (frame_pointer_needed)
		  mem = replace_rtx (mem, stack_pointer_rtx,
				     hard_frame_pointer_rtx);
		SET_DEST (PATTERN (insnS)) = mem;
		df_insn_rescan (insnS);
		SET_SRC (PATTERN (insnR)) = copy_rtx (mem);
		df_insn_rescan (insnR);
		SET_HARD_REG_BIT (cfun->machine->eliminated_callee_saved,
				  regno);
	      }
	    else
	      {
		rtx reg = gen_rtx_REG (SImode, regno);

		insn = emit_move_insn (mem, reg);
		RTX_FRAME_RELATED_P (insn) = 1;
		add_reg_note (insn, REG_FRAME_RELATED_EXPR,
			      gen_rtx_SET (mem, reg));
	      }
	    offset -= UNITS_PER_WORD;
	  }
      if (large_stack_needed)
	xtensa_emit_adjust_stack_ptr (callee_save_size - total_size,
				      ADJUST_SP_NEED_NOTE);
    }

  if (frame_pointer_needed)
    {
      if (cfun->machine->set_frame_ptr_insn)
	{
	  rtx_insn *first;

	  push_topmost_sequence ();
	  first = get_insns ();
	  pop_topmost_sequence ();

	  /* For all instructions prior to set_frame_ptr_insn, replace
	     hard_frame_pointer references with stack_pointer.  */
	  for (insn = first;
	       insn != cfun->machine->set_frame_ptr_insn;
	       insn = NEXT_INSN (insn))
	    {
	      if (INSN_P (insn))
		{
		  PATTERN (insn) = replace_rtx (copy_rtx (PATTERN (insn)),
						hard_frame_pointer_rtx,
						stack_pointer_rtx);
		  df_insn_rescan (insn);
		}
	    }
	}
      else
	{
	  insn = emit_insn (gen_movsi (hard_frame_pointer_rtx,
				       stack_pointer_rtx));
	  if (!TARGET_WINDOWED_ABI)
	    {
	      note_rtx = gen_rtx_SET (hard_frame_pointer_rtx,
				      stack_pointer_rtx);
	      RTX_FRAME_RELATED_P (insn) = 1;
	      add_reg_note (insn, REG_FRAME_RELATED_EXPR, note_rtx);
	    }
	}
    }

  if (TARGET_WINDOWED_ABI)
    {
      /* Create a note to describe the CFA.  Because this is only used to set
	 DW_AT_frame_base for debug info, don't bother tracking changes through
	 each instruction in the prologue.  It just takes up space.  */
      note_rtx = gen_rtx_SET ((frame_pointer_needed
			       ? hard_frame_pointer_rtx
			       : stack_pointer_rtx),
			      plus_constant (Pmode, stack_pointer_rtx,
					     -total_size));
      RTX_FRAME_RELATED_P (insn) = 1;
      add_reg_note (insn, REG_FRAME_RELATED_EXPR, note_rtx);
    }
}

void
xtensa_expand_epilogue (void)
{
  if (!TARGET_WINDOWED_ABI)
    {
      int regno;
      HOST_WIDE_INT offset;

      if (cfun->machine->current_frame_size > (frame_pointer_needed ? 127 : 1024))
	{
	  xtensa_emit_adjust_stack_ptr (cfun->machine->current_frame_size -
					cfun->machine->callee_save_size,
					frame_pointer_needed
					? ADJUST_SP_FRAME_PTR
					: ADJUST_SP_NONE);
	  offset = cfun->machine->callee_save_size - UNITS_PER_WORD;
	}
      else
	{
	  if (frame_pointer_needed)
	    emit_move_insn (stack_pointer_rtx, hard_frame_pointer_rtx);
	  offset = cfun->machine->current_frame_size - UNITS_PER_WORD;
	}

      /* Prevent reordering of saved a0 update and loading it back from
	 the save area.  */
      if (crtl->calls_eh_return)
	emit_insn (gen_blockage ());

      for (regno = 0; regno < FIRST_PSEUDO_REGISTER; ++regno)
	if (xtensa_call_save_reg(regno))
	  {
	    if (! TEST_HARD_REG_BIT (cfun->machine->eliminated_callee_saved,
				     regno))
	      {
		rtx x = gen_rtx_PLUS (Pmode,
				      stack_pointer_rtx, GEN_INT (offset));
		emit_move_insn (gen_rtx_REG (SImode, regno),
				gen_frame_mem (SImode, x));
	      }
	    offset -= UNITS_PER_WORD;
	  }

      if (cfun->machine->current_frame_size > 0)
	{
	  if (frame_pointer_needed || /* always reachable with addi */
	      cfun->machine->current_frame_size > 1024 ||
	      cfun->machine->current_frame_size <= 127)
	    {
	      if (cfun->machine->current_frame_size <= 127)
		offset = cfun->machine->current_frame_size;
	      else
		offset = cfun->machine->callee_save_size;
	      if (offset)
		xtensa_emit_adjust_stack_ptr (offset, ADJUST_SP_NONE);
	    }
	  else
	    xtensa_emit_adjust_stack_ptr (cfun->machine->current_frame_size,
					  ADJUST_SP_NONE);
	}

      if (crtl->calls_eh_return)
	emit_insn (gen_add3_insn (stack_pointer_rtx,
				  stack_pointer_rtx,
				  EH_RETURN_STACKADJ_RTX));
    }
}

void
xtensa_set_return_address (rtx address, rtx scratch)
{
  HOST_WIDE_INT total_size = compute_frame_size (get_frame_size ());
  rtx frame = frame_pointer_needed ?
    hard_frame_pointer_rtx : stack_pointer_rtx;
  rtx a0_addr = plus_constant (Pmode, frame,
			       total_size - UNITS_PER_WORD);
  rtx note = gen_rtx_SET (gen_frame_mem (SImode, a0_addr),
			  gen_rtx_REG (SImode, A0_REG));
  rtx insn;

  if (total_size > 1024)
    {
      xtensa_emit_add_imm (scratch, frame, total_size - UNITS_PER_WORD,
			   scratch, false);
      a0_addr = scratch;
    }

  insn = emit_move_insn (gen_frame_mem (SImode, a0_addr), address);
  RTX_FRAME_RELATED_P (insn) = 1;
  add_reg_note (insn, REG_FRAME_RELATED_EXPR, note);
}

rtx
xtensa_return_addr (int count, rtx frame)
{
  rtx result, retaddr, curaddr, label;

  if (!TARGET_WINDOWED_ABI)
    {
      if (count != 0)
	return const0_rtx;

      return get_hard_reg_initial_val (Pmode, A0_REG);
    }

  if (count == -1)
    retaddr = gen_rtx_REG (Pmode, A0_REG);
  else
    {
      rtx addr = plus_constant (Pmode, frame, -4 * UNITS_PER_WORD);
      addr = memory_address (Pmode, addr);
      retaddr = gen_reg_rtx (Pmode);
      emit_move_insn (retaddr, gen_rtx_MEM (Pmode, addr));
    }

  /* The 2 most-significant bits of the return address on Xtensa hold
     the register window size.  To get the real return address, these
     bits must be replaced with the high bits from some address in the
     code.  */

  /* Get the 2 high bits of a local label in the code.  */
  curaddr = gen_reg_rtx (Pmode);
  label = gen_label_rtx ();
  emit_label (label);
  LABEL_PRESERVE_P (label) = 1;
  emit_move_insn (curaddr, gen_rtx_LABEL_REF (Pmode, label));
  emit_insn (gen_lshrsi3 (curaddr, curaddr, GEN_INT (30)));
  emit_insn (gen_ashlsi3 (curaddr, curaddr, GEN_INT (30)));

  /* Clear the 2 high bits of the return address.  */
  result = gen_reg_rtx (Pmode);
  emit_insn (gen_ashlsi3 (result, retaddr, GEN_INT (2)));
  emit_insn (gen_lshrsi3 (result, result, GEN_INT (2)));

  /* Combine them to get the result.  */
  emit_insn (gen_iorsi3 (result, result, curaddr));
  return result;
}

/* Disable the use of word-sized or smaller complex modes for structures,
   and for function arguments in particular, where they cause problems with
   register a7.  The xtensa_copy_incoming_a7 function assumes that there is
   a single reference to an argument in a7, but with small complex modes the
   real and imaginary components may be extracted separately, leading to two
   uses of the register, only one of which would be replaced.  */

static bool
xtensa_member_type_forces_blk (const_tree, machine_mode mode)
{
  return mode == CQImode || mode == CHImode;
}

/* Create the va_list data type.

   This structure is set up by __builtin_saveregs.  The __va_reg field
   points to a stack-allocated region holding the contents of the
   incoming argument registers.  The __va_ndx field is an index
   initialized to the position of the first unnamed (variable)
   argument.  This same index is also used to address the arguments
   passed in memory.  Thus, the __va_stk field is initialized to point
   to the position of the first argument in memory offset to account
   for the arguments passed in registers and to account for the size
   of the argument registers not being 16-byte aligned.  E.G., there
   are 6 argument registers of 4 bytes each, but we want the __va_ndx
   for the first stack argument to have the maximal alignment of 16
   bytes, so we offset the __va_stk address by 32 bytes so that
   __va_stk[32] references the first argument on the stack.  */

static tree
xtensa_build_builtin_va_list (void)
{
  tree f_stk, f_reg, f_ndx, record, type_decl;

  record = (*lang_hooks.types.make_type) (RECORD_TYPE);
  type_decl = build_decl (BUILTINS_LOCATION,
			  TYPE_DECL, get_identifier ("__va_list_tag"), record);

  f_stk = build_decl (BUILTINS_LOCATION,
		      FIELD_DECL, get_identifier ("__va_stk"),
		      ptr_type_node);
  f_reg = build_decl (BUILTINS_LOCATION,
		      FIELD_DECL, get_identifier ("__va_reg"),
		      ptr_type_node);
  f_ndx = build_decl (BUILTINS_LOCATION,
		      FIELD_DECL, get_identifier ("__va_ndx"),
		      integer_type_node);

  DECL_FIELD_CONTEXT (f_stk) = record;
  DECL_FIELD_CONTEXT (f_reg) = record;
  DECL_FIELD_CONTEXT (f_ndx) = record;

  TYPE_STUB_DECL (record) = type_decl;
  TYPE_NAME (record) = type_decl;
  TYPE_FIELDS (record) = f_stk;
  DECL_CHAIN (f_stk) = f_reg;
  DECL_CHAIN (f_reg) = f_ndx;

  layout_type (record);
  return record;
}


/* Save the incoming argument registers on the stack.  Returns the
   address of the saved registers.  */

static rtx
xtensa_builtin_saveregs (void)
{
  rtx gp_regs;
  int arg_words = crtl->args.info.arg_words;
  int gp_left = MAX_ARGS_IN_REGISTERS - arg_words;

  if (gp_left <= 0)
    return const0_rtx;

  /* Allocate the general-purpose register space.  */
  gp_regs = assign_stack_local
    (BLKmode, MAX_ARGS_IN_REGISTERS * UNITS_PER_WORD, -1);
  set_mem_alias_set (gp_regs, get_varargs_alias_set ());

  /* Now store the incoming registers.  */
  cfun->machine->need_a7_copy = TARGET_WINDOWED_ABI;
  cfun->machine->vararg_a7 = true;
  move_block_from_reg (GP_ARG_FIRST + arg_words,
		       adjust_address (gp_regs, BLKmode,
				       arg_words * UNITS_PER_WORD),
		       gp_left);
  if (cfun->machine->vararg_a7_copy != 0)
    emit_insn_before (cfun->machine->vararg_a7_copy, get_insns ());

  return XEXP (gp_regs, 0);
}


/* Implement `va_start' for varargs and stdarg.  We look at the
   current function to fill in an initial va_list.  */

static void
xtensa_va_start (tree valist, rtx nextarg ATTRIBUTE_UNUSED)
{
  tree f_stk, stk;
  tree f_reg, reg;
  tree f_ndx, ndx;
  tree t, u;
  int arg_words;

  arg_words = crtl->args.info.arg_words;

  f_stk = TYPE_FIELDS (va_list_type_node);
  f_reg = DECL_CHAIN (f_stk);
  f_ndx = DECL_CHAIN (f_reg);

  stk = build3 (COMPONENT_REF, TREE_TYPE (f_stk), valist, f_stk, NULL_TREE);
  reg = build3 (COMPONENT_REF, TREE_TYPE (f_reg), unshare_expr (valist),
		f_reg, NULL_TREE);
  ndx = build3 (COMPONENT_REF, TREE_TYPE (f_ndx), unshare_expr (valist),
		f_ndx, NULL_TREE);

  /* Call __builtin_saveregs; save the result in __va_reg */
  u = make_tree (sizetype, expand_builtin_saveregs ());
  u = fold_convert (ptr_type_node, u);
  t = build2 (MODIFY_EXPR, ptr_type_node, reg, u);
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);

  /* Set the __va_stk member to ($arg_ptr - 32).  */
  u = make_tree (ptr_type_node, virtual_incoming_args_rtx);
  u = fold_build_pointer_plus_hwi (u, -32);
  t = build2 (MODIFY_EXPR, ptr_type_node, stk, u);
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);

  /* Set the __va_ndx member.  If the first variable argument is on
     the stack, adjust __va_ndx by 2 words to account for the extra
     alignment offset for __va_stk.  */
  if (arg_words >= MAX_ARGS_IN_REGISTERS)
    arg_words += 2;
  t = build2 (MODIFY_EXPR, integer_type_node, ndx,
	      build_int_cst (integer_type_node, arg_words * UNITS_PER_WORD));
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);
}


/* Implement `va_arg'.  */

static tree
xtensa_gimplify_va_arg_expr (tree valist, tree type, gimple_seq *pre_p,
			     gimple_seq *post_p ATTRIBUTE_UNUSED)
{
  tree f_stk, stk;
  tree f_reg, reg;
  tree f_ndx, ndx;
  tree type_size, array, orig_ndx, addr, size, va_size, t;
  tree lab_false, lab_over, lab_false2;
  bool indirect;

  indirect = pass_va_arg_by_reference (type);
  if (indirect)
    type = build_pointer_type (type);

  /* Handle complex values as separate real and imaginary parts.  */
  if (TREE_CODE (type) == COMPLEX_TYPE)
    {
      tree real_part, imag_part;

      real_part = xtensa_gimplify_va_arg_expr (valist, TREE_TYPE (type),
					       pre_p, NULL);
      real_part = get_initialized_tmp_var (real_part, pre_p, NULL);

      imag_part = xtensa_gimplify_va_arg_expr (unshare_expr (valist),
					       TREE_TYPE (type),
					       pre_p, NULL);
      imag_part = get_initialized_tmp_var (imag_part, pre_p, NULL);

      return build2 (COMPLEX_EXPR, type, real_part, imag_part);
    }

  f_stk = TYPE_FIELDS (va_list_type_node);
  f_reg = DECL_CHAIN (f_stk);
  f_ndx = DECL_CHAIN (f_reg);

  stk = build3 (COMPONENT_REF, TREE_TYPE (f_stk), valist,
		f_stk, NULL_TREE);
  reg = build3 (COMPONENT_REF, TREE_TYPE (f_reg), unshare_expr (valist),
		f_reg, NULL_TREE);
  ndx = build3 (COMPONENT_REF, TREE_TYPE (f_ndx), unshare_expr (valist),
		f_ndx, NULL_TREE);

  type_size = size_in_bytes (type);
  va_size = round_up (type_size, UNITS_PER_WORD);
  gimplify_expr (&va_size, pre_p, NULL, is_gimple_val, fb_rvalue);


  /* First align __va_ndx if necessary for this arg:

     orig_ndx = (AP).__va_ndx;
     if (__alignof__ (TYPE) > 4 )
       orig_ndx = ((orig_ndx + __alignof__ (TYPE) - 1)
			& -__alignof__ (TYPE)); */

  orig_ndx = get_initialized_tmp_var (ndx, pre_p, NULL);

  if (TYPE_ALIGN (type) > BITS_PER_WORD)
    {
      int align = MIN (TYPE_ALIGN (type), STACK_BOUNDARY) / BITS_PER_UNIT;

      t = build2 (PLUS_EXPR, integer_type_node, unshare_expr (orig_ndx),
		  build_int_cst (integer_type_node, align - 1));
      t = build2 (BIT_AND_EXPR, integer_type_node, t,
		  build_int_cst (integer_type_node, -align));
      gimplify_assign (unshare_expr (orig_ndx), t, pre_p);
    }


  /* Increment __va_ndx to point past the argument:

     (AP).__va_ndx = orig_ndx + __va_size (TYPE); */

  t = fold_convert (integer_type_node, va_size);
  t = build2 (PLUS_EXPR, integer_type_node, orig_ndx, t);
  gimplify_assign (unshare_expr (ndx), t, pre_p);


  /* Check if the argument is in registers:

     if ((AP).__va_ndx <= __MAX_ARGS_IN_REGISTERS * 4
	 && !must_pass_in_stack (type))
	__array = (AP).__va_reg; */

  array = create_tmp_var (ptr_type_node);

  lab_over = NULL;
  if (!must_pass_va_arg_in_stack (type))
    {
      lab_false = create_artificial_label (UNKNOWN_LOCATION);
      lab_over = create_artificial_label (UNKNOWN_LOCATION);

      t = build2 (GT_EXPR, boolean_type_node, unshare_expr (ndx),
		  build_int_cst (integer_type_node,
				 MAX_ARGS_IN_REGISTERS * UNITS_PER_WORD));
      t = build3 (COND_EXPR, void_type_node, t,
		  build1 (GOTO_EXPR, void_type_node, lab_false),
		  NULL_TREE);
      gimplify_and_add (t, pre_p);

      gimplify_assign (unshare_expr (array), reg, pre_p);

      t = build1 (GOTO_EXPR, void_type_node, lab_over);
      gimplify_and_add (t, pre_p);

      t = build1 (LABEL_EXPR, void_type_node, lab_false);
      gimplify_and_add (t, pre_p);
    }


  /* ...otherwise, the argument is on the stack (never split between
     registers and the stack -- change __va_ndx if necessary):

     else
       {
	 if (orig_ndx <= __MAX_ARGS_IN_REGISTERS * 4)
	     (AP).__va_ndx = 32 + __va_size (TYPE);
	 __array = (AP).__va_stk;
       } */

  lab_false2 = create_artificial_label (UNKNOWN_LOCATION);

  t = build2 (GT_EXPR, boolean_type_node, unshare_expr (orig_ndx),
	      build_int_cst (integer_type_node,
			     MAX_ARGS_IN_REGISTERS * UNITS_PER_WORD));
  t = build3 (COND_EXPR, void_type_node, t,
	      build1 (GOTO_EXPR, void_type_node, lab_false2),
	      NULL_TREE);
  gimplify_and_add (t, pre_p);

  t = size_binop (PLUS_EXPR, unshare_expr (va_size), size_int (32));
  t = fold_convert (integer_type_node, t);
  gimplify_assign (unshare_expr (ndx), t, pre_p);

  t = build1 (LABEL_EXPR, void_type_node, lab_false2);
  gimplify_and_add (t, pre_p);

  gimplify_assign (array, stk, pre_p);

  if (lab_over)
    {
      t = build1 (LABEL_EXPR, void_type_node, lab_over);
      gimplify_and_add (t, pre_p);
    }


  /* Given the base array pointer (__array) and index to the subsequent
     argument (__va_ndx), find the address:

     __array + (AP).__va_ndx - (BYTES_BIG_ENDIAN && sizeof (TYPE) < 4
				? sizeof (TYPE)
				: __va_size (TYPE))

     The results are endian-dependent because values smaller than one word
     are aligned differently.  */


  if (BYTES_BIG_ENDIAN && TREE_CODE (type_size) == INTEGER_CST)
    {
      t = fold_build2 (GE_EXPR, boolean_type_node, unshare_expr (type_size),
		       size_int (PARM_BOUNDARY / BITS_PER_UNIT));
      t = fold_build3 (COND_EXPR, sizetype, t, unshare_expr (va_size),
		       unshare_expr (type_size));
      size = t;
    }
  else
    size = unshare_expr (va_size);

  t = fold_convert (sizetype, unshare_expr (ndx));
  t = build2 (MINUS_EXPR, sizetype, t, size);
  addr = fold_build_pointer_plus (unshare_expr (array), t);

  addr = fold_convert (build_pointer_type (type), addr);
  if (indirect)
    addr = build_va_arg_indirect_ref (addr);
  return build_va_arg_indirect_ref (addr);
}


/* Builtins.  */

enum xtensa_builtin
{
  XTENSA_BUILTIN_UMULSIDI3,
  XTENSA_BUILTIN_max
};


static void
xtensa_init_builtins (void)
{
  tree ftype, decl;

  ftype = build_function_type_list (unsigned_intDI_type_node,
				    unsigned_intSI_type_node,
				    unsigned_intSI_type_node, NULL_TREE);

  decl = add_builtin_function ("__builtin_umulsidi3", ftype,
			       XTENSA_BUILTIN_UMULSIDI3, BUILT_IN_MD,
			       "__umulsidi3", NULL_TREE);
  TREE_NOTHROW (decl) = 1;
  TREE_READONLY (decl) = 1;
}


static tree
xtensa_fold_builtin (tree fndecl, int n_args ATTRIBUTE_UNUSED, tree *args,
		     bool ignore ATTRIBUTE_UNUSED)
{
  unsigned int fcode = DECL_MD_FUNCTION_CODE (fndecl);
  tree arg0, arg1;

  switch (fcode)
    {
    case XTENSA_BUILTIN_UMULSIDI3:
      arg0 = args[0];
      arg1 = args[1];
      if ((TREE_CODE (arg0) == INTEGER_CST && TREE_CODE (arg1) == INTEGER_CST)
	  || TARGET_MUL32_HIGH)
	return fold_build2 (MULT_EXPR, unsigned_intDI_type_node,
			    fold_convert (unsigned_intDI_type_node, arg0),
			    fold_convert (unsigned_intDI_type_node, arg1));
      break;

    default:
      internal_error ("bad builtin code");
      break;
    }

  return NULL;
}


static rtx
xtensa_expand_builtin (tree exp, rtx target,
		       rtx subtarget ATTRIBUTE_UNUSED,
		       machine_mode mode ATTRIBUTE_UNUSED,
		       int ignore)
{
  tree fndecl = TREE_OPERAND (CALL_EXPR_FN (exp), 0);
  unsigned int fcode = DECL_MD_FUNCTION_CODE (fndecl);

  switch (fcode)
    {
    case XTENSA_BUILTIN_UMULSIDI3:
      /* The umulsidi3 builtin is just a mechanism to avoid calling the real
	 __umulsidi3 function when the Xtensa configuration can directly
	 implement it.  If not, just call the function.  */
      return expand_call (exp, target, ignore);

    default:
      internal_error ("bad builtin code");
    }
  return NULL_RTX;
}

/* Worker function for TARGET_PREFERRED_RELOAD_CLASS.  */

static reg_class_t
xtensa_preferred_reload_class (rtx x, reg_class_t rclass)
{
  if (CONSTANT_P (x) && CONST_DOUBLE_P (x))
    return NO_REGS;

  /* Don't use the stack pointer or hard frame pointer for reloads!
     The hard frame pointer would normally be OK except that it may
     briefly hold an incoming argument in the prologue, and reload
     won't know that it is live because the hard frame pointer is
     treated specially.  */

  if (rclass == AR_REGS || rclass == GR_REGS)
    return RL_REGS;

  return rclass;
}

/* Worker function for TARGET_PREFERRED_OUTPUT_RELOAD_CLASS.  */

static reg_class_t
xtensa_preferred_output_reload_class (rtx x ATTRIBUTE_UNUSED,
				      reg_class_t rclass)
{
  /* Don't use the stack pointer or hard frame pointer for reloads!
     The hard frame pointer would normally be OK except that it may
     briefly hold an incoming argument in the prologue, and reload
     won't know that it is live because the hard frame pointer is
     treated specially.  */

  if (rclass == AR_REGS || rclass == GR_REGS)
    return RL_REGS;

  return rclass;
}

/* Worker function for TARGET_SECONDARY_RELOAD.  */

static reg_class_t
xtensa_secondary_reload (bool in_p, rtx x, reg_class_t rclass,
			 machine_mode mode, secondary_reload_info *sri)
{
  int regno;

  if (in_p && constantpool_mem_p (x))
    {
      if (rclass == FP_REGS)
	return RL_REGS;

      if (mode == QImode)
	sri->icode = CODE_FOR_reloadqi_literal;
      else if (mode == HImode)
	sri->icode = CODE_FOR_reloadhi_literal;
    }

  regno = xt_true_regnum (x);
  if (ACC_REG_P (regno))
    return ((rclass == GR_REGS || rclass == RL_REGS) ? NO_REGS : RL_REGS);
  if (rclass == ACC_REG)
    return (GP_REG_P (regno) ? NO_REGS : RL_REGS);

  return NO_REGS;
}

/* Called once at the start of IRA, by ADJUST_REG_ALLOC_ORDER.  */

void
xtensa_adjust_reg_alloc_order (void)
{
  static const int reg_windowed_alloc_order[FIRST_PSEUDO_REGISTER] =
	REG_ALLOC_ORDER;
  static const int reg_call0_alloc_order[FIRST_PSEUDO_REGISTER] =
  {
     9, 10, 11,  7,  6,  5,  4,  3,  2,  8,  0, 12, 13, 14, 15,
    18,
    19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
     1, 16, 17,
    35,
  };

  memcpy (reg_alloc_order, TARGET_WINDOWED_ABI ?
	  reg_windowed_alloc_order : reg_call0_alloc_order,
	  FIRST_PSEUDO_REGISTER * sizeof (int));
}


/* Some Xtensa targets support multiple bss sections.  If the section
   name ends with ".bss", add SECTION_BSS to the flags.  */

static unsigned int
xtensa_multibss_section_type_flags (tree decl, const char *name, int reloc)
{
  unsigned int flags = default_section_type_flags (decl, name, reloc);
  const char *suffix;

  suffix = strrchr (name, '.');
  if (suffix && strcmp (suffix, ".bss") == 0)
    {
      if (!decl || (VAR_P (decl)
		    && DECL_INITIAL (decl) == NULL_TREE))
	flags |= SECTION_BSS;  /* @nobits */
      else
	warning (0, "only uninitialized variables can be placed in a "
		 "%<.bss%> section");
    }

  return flags;
}


/* The literal pool stays with the function.  */

static section *
xtensa_select_rtx_section (machine_mode mode ATTRIBUTE_UNUSED,
			   rtx x ATTRIBUTE_UNUSED,
			   unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED)
{
  return function_section (current_function_decl);
}

/* Worker function for TARGET_REGISTER_MOVE_COST.  */

static int
xtensa_register_move_cost (machine_mode mode ATTRIBUTE_UNUSED,
			   reg_class_t from, reg_class_t to)
{
  /* If both are equal (except for BR_REGS) or belong to AR_REGS,
     the cost is 2 (the default value).  */
  if ((from == to && from != BR_REGS && to != BR_REGS)
      || (reg_class_subset_p (from, AR_REGS)
	  && reg_class_subset_p (to, AR_REGS)))
    return 2;

  /* The cost between AR_REGS and FR_REGS is 2 (the default value).  */
  if ((reg_class_subset_p (from, AR_REGS) && to == FP_REGS)
      || (from == FP_REGS && reg_class_subset_p (to, AR_REGS)))
    return 2;

  if ((reg_class_subset_p (from, AR_REGS) && to == ACC_REG)
      || (from == ACC_REG && reg_class_subset_p (to, AR_REGS)))
    return 3;

  /* Otherwise, spills to stack (because greater than 2x the default
     MEMORY_MOVE_COST).  */
  return 10;
}

/* Compute a (partial) cost for rtx X.  Return true if the complete
   cost has been computed, and false if subexpressions should be
   scanned.  In either case, *TOTAL contains the cost result.  */

static bool
xtensa_rtx_costs (rtx x, machine_mode mode, int outer_code,
		  int opno ATTRIBUTE_UNUSED,
		  int *total, bool speed)
{
  int code = GET_CODE (x);

  switch (code)
    {
    case CONST_INT:
      switch (outer_code)
	{
	case SET:
	  if (xtensa_simm12b (INTVAL (x))
	      || (current_pass && current_pass->tv_id == TV_IFCVT))
	    {
	      *total = speed ? COSTS_N_INSNS (1) : 0;
	      return true;
	    }
	  break;
	case PLUS:
	  if (xtensa_simm8 (INTVAL (x))
	      || xtensa_simm8x256 (INTVAL (x)))
	    {
	      *total = 0;
	      return true;
	    }
	  break;
	case AND:
	  if (xtensa_mask_immediate (INTVAL (x)))
	    {
	      *total = 0;
	      return true;
	    }
	  break;
	case COMPARE:
	  if (xtensa_b4const_or_zero (INTVAL (x))
	      || xtensa_b4constu (INTVAL (x)))
	    {
	      *total = 0;
	      return true;
	    }
	  break;
	case ASHIFT:
	case ASHIFTRT:
	case LSHIFTRT:
	case ROTATE:
	case ROTATERT:
	  /* No way to tell if X is the 2nd operand so be conservative.  */
	default: break;
	}
      if (xtensa_simm12b (INTVAL (x)))
	*total = 5;
      else if (TARGET_CONST16)
	*total = COSTS_N_INSNS (2);
      else
	*total = 6;
      return true;

    case CONST:
    case LABEL_REF:
    case SYMBOL_REF:
      if (TARGET_CONST16)
	*total = COSTS_N_INSNS (2);
      else
	*total = 5;
      return true;

    case CONST_DOUBLE:
      if (TARGET_CONST16)
	*total = COSTS_N_INSNS (4);
      else
	*total = 7;
      return true;

    case MEM:
      {
	int num_words =
	  (GET_MODE_SIZE (mode) > UNITS_PER_WORD) ?  2 : 1;

	if (memory_address_p (mode, XEXP ((x), 0)))
	  *total = COSTS_N_INSNS (num_words);
	else
	  *total = COSTS_N_INSNS (2*num_words);
	return true;
      }

    case FFS:
    case CTZ:
      *total = COSTS_N_INSNS (TARGET_NSA ? 5 : 50);
      return true;

    case CLZ:
    case CLRSB:
      *total = COSTS_N_INSNS (TARGET_NSA ? 1 : 50);
      return true;

    case BSWAP:
      *total = COSTS_N_INSNS (mode == HImode ? 3 : 5);
      return true;

    case NOT:
      *total = COSTS_N_INSNS (mode == DImode ? 3 : 2);
      return true;

    case AND:
    case IOR:
    case XOR:
      if (mode == DImode)
	*total = COSTS_N_INSNS (2);
      else
	*total = COSTS_N_INSNS (1);
      return true;

    case ASHIFT:
    case ASHIFTRT:
    case LSHIFTRT:
      if (mode == DImode)
	*total = COSTS_N_INSNS (50);
      else
	*total = COSTS_N_INSNS (1);
      return true;

    case ABS:
    case NEG:
      {
	if (mode == SFmode)
	  *total = COSTS_N_INSNS (TARGET_HARD_FLOAT ? 1 : 50);
	else if (mode == DFmode)
	  *total = COSTS_N_INSNS (50);
	else if (mode == DImode)
	  *total = COSTS_N_INSNS (4);
	else
	  *total = COSTS_N_INSNS (1);
	return true;
      }

    case PLUS:
    case MINUS:
      {
	if (mode == SFmode)
	  *total = COSTS_N_INSNS (TARGET_HARD_FLOAT ? 1 : 50);
	else if (mode == DFmode || mode == DImode)
	  *total = COSTS_N_INSNS (50);
	else
	  *total = COSTS_N_INSNS (1);
	return true;
      }

    case MULT:
      {
	if (mode == SFmode)
	  *total = COSTS_N_INSNS (TARGET_HARD_FLOAT ? 4 : 50);
	else if (mode == DFmode)
	  *total = COSTS_N_INSNS (50);
	else if (mode == DImode)
	  *total = COSTS_N_INSNS (TARGET_MUL32_HIGH ? 10 : 50);
	else if (TARGET_MUL32)
	  *total = COSTS_N_INSNS (4);
	else if (TARGET_MAC16)
	  *total = COSTS_N_INSNS (16);
	else if (TARGET_MUL16)
	  *total = COSTS_N_INSNS (12);
	else
	  *total = COSTS_N_INSNS (50);
	return true;
      }

    case DIV:
    case MOD:
      {
	if (mode == SFmode)
	  {
	    *total = COSTS_N_INSNS (TARGET_HARD_FLOAT_DIV ? 8 : 50);
	    return true;
	  }
	else if (mode == DFmode)
	  {
	    *total = COSTS_N_INSNS (50);
	    return true;
	  }
      }
      /* Fall through.  */

    case UDIV:
    case UMOD:
      {
	if (mode == DImode)
	  *total = COSTS_N_INSNS (speed ? 100 : 50);
	else if (TARGET_DIV32)
	  *total = COSTS_N_INSNS (32);
	else
	  *total = COSTS_N_INSNS (speed ? 100 : 50);
	return true;
      }

    case SQRT:
      if (mode == SFmode)
	*total = COSTS_N_INSNS (TARGET_HARD_FLOAT_SQRT ? 8 : 50);
      else
	*total = COSTS_N_INSNS (50);
      return true;

    case SMIN:
    case UMIN:
    case SMAX:
    case UMAX:
      *total = COSTS_N_INSNS (TARGET_MINMAX ? 1 : 50);
      return true;

    case SIGN_EXTRACT:
    case SIGN_EXTEND:
      *total = COSTS_N_INSNS (TARGET_SEXT ? 1 : 2);
      return true;

    case ZERO_EXTRACT:
    case ZERO_EXTEND:
    case IF_THEN_ELSE:
      *total = COSTS_N_INSNS (1);
      return true;

    default:
      return false;
    }
}

/* Return TRUE if the specified insn corresponds to one or more L32R machine
   instructions.  */

static bool
xtensa_is_insn_L32R_p (const rtx_insn *insn)
{
  rtx pat, dest, src;
  machine_mode mode;

  /* RTX insns that are not "(set (reg) ...)" cannot become L32R instructions:
     - it is permitted to apply PATTERN() to the insn without validation.
       See insn_cost() in gcc/rtlanal.cc.
     - it is used register_operand() instead of REG() to identify things that
       don't look like REGs but will eventually become so as well.  */
  if (GET_CODE (pat = PATTERN (insn)) != SET
      || ! register_operand (dest = SET_DEST (pat), VOIDmode))
    return false;

  /* If the source is a reference to a literal pool entry, then the insn
     obviously corresponds to an L32R instruction.  */
  if (constantpool_mem_p (src = SET_SRC (pat)))
    return true;

  /* Similarly, an insn whose source is not a constant obviously does not
     correspond to L32R.  */
  if (! CONSTANT_P (src))
    return false;

  /* If the source is a CONST_INT whose value fits into signed 12 bits, then
     the insn corresponds to a MOVI instruction (rather than an L32R one),
     regardless of the configuration of TARGET_CONST16 or
     TARGET_AUTOLITPOOLS.  Note that the destination register can be non-
     SImode.  */
  if (((mode = GET_MODE (dest)) == SImode
       || mode == HImode || mode == SFmode)
      && CONST_INT_P (src) && xtensa_simm12b (INTVAL (src)))
    return false;

  /* If TARGET_CONST16 is configured, constants of the remaining forms
     correspond to pairs of CONST16 instructions, not L32R.  */
  if (TARGET_CONST16)
    return false;

  /* The last remaining form of constant is one of the following:
     - CONST_INTs with large values
     - floating-point constants
     - symbolic constants
     and is all handled by a relaxed MOVI instruction, which is later
     converted to an L32R instruction by the assembler.  */
  return true;
}

/* Compute a relative costs of RTL insns.  This is necessary in order to
   achieve better RTL insn splitting/combination result.  */

static int
xtensa_insn_cost (rtx_insn *insn, bool speed)
{
  if (! (recog_memoized (insn) < 0))
    {
      int len = get_attr_length (insn);

      if (len == 0)
	return COSTS_N_INSNS (0);

      if (speed)  /* For speed cost.  */
	{
	  int n = (len + 2) / 3;

	  /* "L32R" may be particular slow (implementation-dependent).  */
	  if (xtensa_is_insn_L32R_p (insn))
	    return COSTS_N_INSNS ((1 + xtensa_extra_l32r_costs) * n);

	  /* Cost based on the pipeline model.  */
	  switch (get_attr_type (insn))
	    {
	    case TYPE_STORE:
	    case TYPE_MOVE:
	    case TYPE_ARITH:
	    case TYPE_MULTI:
	    case TYPE_NOP:
	    case TYPE_FARITH:
	    case TYPE_FSTORE:
	      return COSTS_N_INSNS (n);

	    case TYPE_LOAD:
	      return COSTS_N_INSNS (n - 1 + 2);

	    case TYPE_JUMP:
	    case TYPE_CALL:
	      return COSTS_N_INSNS (n - 1 + 3);

	    case TYPE_FCONV:
	    case TYPE_FLOAD:
	    case TYPE_MUL16:
	    case TYPE_MUL32:
	    case TYPE_RSR:
	      return COSTS_N_INSNS (n * 2);

	    case TYPE_FMADD:
	      return COSTS_N_INSNS (n * 4);

	    case TYPE_DIV32:
	      return COSTS_N_INSNS (n * 16);

	    default:
	      break;
	    }
	}
      else  /* For size cost.  */
	{
	  /* Cost based on the instruction length.  */
	  if (get_attr_type (insn) != TYPE_UNKNOWN)
	    {
	      /* "L32R" itself plus constant in litpool.  */
	      if (xtensa_is_insn_L32R_p (insn))
		len += (len / 3) * 4;

	      /* Consider fractional instruction length (for example, ".n"
		 short instructions or "L32R" litpool constants.  */
	      return (COSTS_N_INSNS (len) + 1) / 3;
	    }
	}
    }

  /* Fall back.  */
  return pattern_cost (PATTERN (insn), speed);
}

/* Worker function for TARGET_PROMOTE_FUNCTION_MODE.  */

static machine_mode
xtensa_promote_function_mode (const_tree type, machine_mode mode,
			      int *punsignedp, const_tree, int)
{
  if (GET_MODE_CLASS (mode) == MODE_INT
      && GET_MODE_SIZE (mode) < GET_MODE_SIZE (SImode))
    return SImode;

  return promote_mode (type, mode, punsignedp);
}

/* Worker function for TARGET_RETURN_IN_MEMORY.  */

static bool
xtensa_return_in_memory (const_tree type, const_tree fntype ATTRIBUTE_UNUSED)
{
  return ((unsigned HOST_WIDE_INT) int_size_in_bytes (type)
	  > (unsigned) (GP_RETURN_LAST - GP_RETURN_FIRST + 1) * UNITS_PER_WORD);
}

/* Worker function for TARGET_FUNCTION_VALUE.  */

rtx
xtensa_function_value (const_tree valtype, const_tree func ATTRIBUTE_UNUSED,
		       bool outgoing)
{
  return gen_rtx_REG ((INTEGRAL_TYPE_P (valtype)
		       && TYPE_PRECISION (valtype) < BITS_PER_WORD)
		      ? SImode : TYPE_MODE (valtype),
		      outgoing ? GP_OUTGOING_RETURN : GP_RETURN_FIRST);
}

/* Worker function for TARGET_LIBCALL_VALUE.  */

static rtx
xtensa_libcall_value (machine_mode mode, const_rtx fun ATTRIBUTE_UNUSED)
{
  return gen_rtx_REG ((GET_MODE_CLASS (mode) == MODE_INT
		       && GET_MODE_SIZE (mode) < UNITS_PER_WORD)
		      ? SImode : mode, GP_RETURN_FIRST);
}

/* Worker function TARGET_FUNCTION_VALUE_REGNO_P.  */

static bool
xtensa_function_value_regno_p (const unsigned int regno)
{
  return IN_RANGE (regno, GP_RETURN_FIRST, GP_RETURN_LAST);
}

/* The static chain is passed in memory.  Provide rtx giving 'mem'
   expressions that denote where they are stored.  */

static rtx
xtensa_static_chain (const_tree ARG_UNUSED (fndecl_or_type), bool incoming_p)
{
  if (TARGET_WINDOWED_ABI)
    {
      rtx base = incoming_p ? arg_pointer_rtx : stack_pointer_rtx;
      return gen_frame_mem (Pmode, plus_constant (Pmode, base,
						  -5 * UNITS_PER_WORD));
    }
  else
    return gen_rtx_REG (Pmode, A8_REG);
}


/* TRAMPOLINE_TEMPLATE: For Xtensa, the trampoline must perform an ENTRY
   instruction with a minimal stack frame in order to get some free
   registers.  Once the actual call target is known, the proper stack frame
   size is extracted from the ENTRY instruction at the target and the
   current frame is adjusted to match.  The trampoline then transfers
   control to the instruction following the ENTRY at the target.  Note:
   this assumes that the target begins with an ENTRY instruction.  */

static void
xtensa_asm_trampoline_template (FILE *stream)
{
  bool use_call0 = (TARGET_CONST16 || TARGET_ABSOLUTE_LITERALS);

  fprintf (stream, "\t.begin no-transform\n");

  if (TARGET_WINDOWED_ABI)
    {
      fprintf (stream, "\tentry\tsp, %d\n", MIN_FRAME_SIZE);

      if (use_call0)
	{
	  /* Save the return address.  */
	  fprintf (stream, "\tmov\ta10, a0\n");

	  /* Use a CALL0 instruction to skip past the constants and in the
	     process get the PC into A0.  This allows PC-relative access to
	     the constants without relying on L32R.  */
	  fprintf (stream, "\tcall0\t.Lskipconsts\n");
	}
      else
	fprintf (stream, "\tj\t.Lskipconsts\n");

      fprintf (stream, "\t.align\t4\n");
      fprintf (stream, ".Lchainval:%s0\n", integer_asm_op (4, TRUE));
      fprintf (stream, ".Lfnaddr:%s0\n", integer_asm_op (4, TRUE));
      fprintf (stream, ".Lskipconsts:\n");

      /* Load the static chain and function address from the trampoline.  */
      if (use_call0)
	{
	  fprintf (stream, "\taddi\ta0, a0, 3\n");
	  fprintf (stream, "\tl32i\ta9, a0, 0\n");
	  fprintf (stream, "\tl32i\ta8, a0, 4\n");
	}
      else
	{
	  fprintf (stream, "\tl32r\ta9, .Lchainval\n");
	  fprintf (stream, "\tl32r\ta8, .Lfnaddr\n");
	}

      /* Store the static chain.  */
      fprintf (stream, "\ts32i\ta9, sp, %d\n", MIN_FRAME_SIZE - 20);

      /* Set the proper stack pointer value.  */
      fprintf (stream, "\tl32i\ta9, a8, 0\n");
      fprintf (stream, "\textui\ta9, a9, %d, 12\n",
	       TARGET_BIG_ENDIAN ? 8 : 12);
      fprintf (stream, "\tslli\ta9, a9, 3\n");
      fprintf (stream, "\taddi\ta9, a9, %d\n", -MIN_FRAME_SIZE);
      fprintf (stream, "\tsub\ta9, sp, a9\n");
      fprintf (stream, "\tmovsp\tsp, a9\n");

      if (use_call0)
	/* Restore the return address.  */
	fprintf (stream, "\tmov\ta0, a10\n");

      /* Jump to the instruction following the ENTRY.  */
      fprintf (stream, "\taddi\ta8, a8, 3\n");
      fprintf (stream, "\tjx\ta8\n");

      /* Pad size to a multiple of TRAMPOLINE_ALIGNMENT.  */
      if (use_call0)
	fprintf (stream, "\t.byte\t0\n");
      else
	fprintf (stream, "\tnop\n");
    }
  else
    {
      if (use_call0)
	{
	  /* Save the return address.  */
	  fprintf (stream, "\tmov\ta10, a0\n");

	  /* Use a CALL0 instruction to skip past the constants and in the
	     process get the PC into A0.  This allows PC-relative access to
	     the constants without relying on L32R.  */
	  fprintf (stream, "\tcall0\t.Lskipconsts\n");
	}
      else
	fprintf (stream, "\tj\t.Lskipconsts\n");

      fprintf (stream, "\t.align\t4\n");
      fprintf (stream, ".Lchainval:%s0\n", integer_asm_op (4, TRUE));
      fprintf (stream, ".Lfnaddr:%s0\n", integer_asm_op (4, TRUE));
      fprintf (stream, ".Lskipconsts:\n");

      /* Load the static chain and function address from the trampoline.  */
      if (use_call0)
	{
	  fprintf (stream, "\taddi\ta0, a0, 3\n");
	  fprintf (stream, "\tl32i\ta8, a0, 0\n");
	  fprintf (stream, "\tl32i\ta9, a0, 4\n");
	  fprintf (stream, "\tmov\ta0, a10\n");
	}
      else
	{
	  fprintf (stream, "\tl32r\ta8, .Lchainval\n");
	  fprintf (stream, "\tl32r\ta9, .Lfnaddr\n");
	}
      fprintf (stream, "\tjx\ta9\n");

      /* Pad size to a multiple of TRAMPOLINE_ALIGNMENT.  */
      if (use_call0)
	fprintf (stream, "\t.byte\t0\n");
      else
	fprintf (stream, "\tnop\n");
    }
  fprintf (stream, "\t.end no-transform\n");
}

static void
xtensa_trampoline_init (rtx m_tramp, tree fndecl, rtx chain)
{
  rtx func = XEXP (DECL_RTL (fndecl), 0);
  bool use_call0 = (TARGET_CONST16 || TARGET_ABSOLUTE_LITERALS);
  int chain_off;
  int func_off;

  if (TARGET_WINDOWED_ABI)
    {
      chain_off = use_call0 ? 12 : 8;
      func_off = use_call0 ? 16 : 12;
    }
  else
    {
      chain_off = use_call0 ? 8 : 4;
      func_off = use_call0 ? 12 : 8;
    }

  emit_block_move (m_tramp, assemble_trampoline_template (),
		   GEN_INT (TRAMPOLINE_SIZE), BLOCK_OP_NORMAL);

  emit_move_insn (adjust_address (m_tramp, SImode, chain_off), chain);
  emit_move_insn (adjust_address (m_tramp, SImode, func_off), func);
  emit_library_call (gen_rtx_SYMBOL_REF (Pmode, "__xtensa_sync_caches"),
		     LCT_NORMAL, VOIDmode, XEXP (m_tramp, 0), Pmode);
}

/* Implement TARGET_LEGITIMATE_CONSTANT_P.  */

static bool
xtensa_legitimate_constant_p (machine_mode mode ATTRIBUTE_UNUSED, rtx x)
{
  if (CONST_INT_P (x))
    return TARGET_AUTO_LITPOOLS || TARGET_CONST16
	   || xtensa_simm12b (INTVAL (x));

  return !xtensa_tls_referenced_p (x);
}

/* Implement TARGET_CAN_USE_DOLOOP_P.  */

static bool
xtensa_can_use_doloop_p (const widest_int &, const widest_int &,
			 unsigned int loop_depth, bool entered_at_top)
{
  /* Considering limitations in the hardware, only use doloop
     for innermost loops which must be entered from the top.  */
  if (loop_depth > 1 || !entered_at_top)
    return false;

  return true;
}

/* NULL if INSN insn is valid within a low-overhead loop.
   Otherwise return why doloop cannot be applied.  */

static const char *
xtensa_invalid_within_doloop (const rtx_insn *insn)
{
  if (CALL_P (insn))
    return "Function call in the loop.";

  if (JUMP_P (insn) && INSN_CODE (insn) == CODE_FOR_return)
    return "Return from a call instruction in the loop.";

  return NULL;
}

/* Optimize LOOP.  */

static bool
hwloop_optimize (hwloop_info loop)
{
  int i;
  edge entry_edge;
  basic_block entry_bb;
  rtx iter_reg;
  rtx_insn *insn, *seq, *entry_after;

  if (loop->depth > 1)
    {
      if (dump_file)
	fprintf (dump_file, ";; loop %d is not innermost\n",
		 loop->loop_no);
      return false;
    }

  if (!loop->incoming_dest)
    {
      if (dump_file)
	fprintf (dump_file, ";; loop %d has more than one entry\n",
		 loop->loop_no);
      return false;
    }

  if (loop->incoming_dest != loop->head)
    {
      if (dump_file)
	fprintf (dump_file, ";; loop %d is not entered from head\n",
		 loop->loop_no);
      return false;
    }

  if (loop->has_call || loop->has_asm)
    {
      if (dump_file)
	fprintf (dump_file, ";; loop %d has invalid insn\n",
		 loop->loop_no);
      return false;
    }

  /* Scan all the blocks to make sure they don't use iter_reg.  */
  if (loop->iter_reg_used || loop->iter_reg_used_outside)
    {
      if (dump_file)
	fprintf (dump_file, ";; loop %d uses iterator\n",
		 loop->loop_no);
      return false;
    }

  /* Check if start_label appears before doloop_end.  */
  insn = loop->start_label;
  while (insn && insn != loop->loop_end)
    insn = NEXT_INSN (insn);

  if (!insn)
    {
      if (dump_file)
	fprintf (dump_file, ";; loop %d start_label not before loop_end\n",
		 loop->loop_no);
      return false;
    }

  /* Get the loop iteration register.  */
  iter_reg = loop->iter_reg;

  gcc_assert (REG_P (iter_reg));

  entry_edge = NULL;

  FOR_EACH_VEC_SAFE_ELT (loop->incoming, i, entry_edge)
    if (entry_edge->flags & EDGE_FALLTHRU)
      break;

  if (entry_edge == NULL)
    return false;

  /* Place the zero_cost_loop_start instruction before the loop.  */
  entry_bb = entry_edge->src;

  start_sequence ();

  insn = emit_insn (gen_zero_cost_loop_start (loop->iter_reg,
					      loop->start_label,
					      loop->iter_reg));

  seq = get_insns ();

  entry_after = BB_END (entry_bb);
  if (!single_succ_p (entry_bb) || vec_safe_length (loop->incoming) > 1
      || !entry_after)
    {
      basic_block new_bb;
      edge e;
      edge_iterator ei;

      emit_insn_before (seq, BB_HEAD (loop->head));
      seq = emit_label_before (gen_label_rtx (), seq);
      new_bb = create_basic_block (seq, insn, entry_bb);
      FOR_EACH_EDGE (e, ei, loop->incoming)
	{
	  if (!(e->flags & EDGE_FALLTHRU))
	    redirect_edge_and_branch_force (e, new_bb);
	  else
	    redirect_edge_succ (e, new_bb);
	}

      make_edge (new_bb, loop->head, 0);
    }
  else
    {
      while (DEBUG_INSN_P (entry_after)
	     || (NOTE_P (entry_after)
		 && NOTE_KIND (entry_after) != NOTE_INSN_BASIC_BLOCK))
	entry_after = PREV_INSN (entry_after);

      emit_insn_after (seq, entry_after);
    }

  end_sequence ();

  return true;
}

/* A callback for the hw-doloop pass.  Called when a loop we have discovered
   turns out not to be optimizable; we have to split the loop_end pattern into
   a subtract and a test.  */

static void
hwloop_fail (hwloop_info loop)
{
  rtx test;
  rtx_insn *insn = loop->loop_end;

  emit_insn_before (gen_addsi3 (loop->iter_reg,
				loop->iter_reg,
				constm1_rtx),
		    loop->loop_end);

  test = gen_rtx_NE (VOIDmode, loop->iter_reg, const0_rtx);
  insn = emit_jump_insn_before (gen_cbranchsi4 (test,
						loop->iter_reg, const0_rtx,
						loop->start_label),
				loop->loop_end);

  JUMP_LABEL (insn) = loop->start_label;
  LABEL_NUSES (loop->start_label)++;
  delete_insn (loop->loop_end);
}

/* A callback for the hw-doloop pass.  This function examines INSN; if
   it is a doloop_end pattern we recognize, return the reg rtx for the
   loop counter.  Otherwise, return NULL_RTX.  */

static rtx
hwloop_pattern_reg (rtx_insn *insn)
{
  rtx reg;

  if (!JUMP_P (insn) || recog_memoized (insn) != CODE_FOR_loop_end)
    return NULL_RTX;

  reg = SET_DEST (XVECEXP (PATTERN (insn), 0, 1));
  if (!REG_P (reg))
    return NULL_RTX;

  return reg;
}


static struct hw_doloop_hooks xtensa_doloop_hooks =
{
  hwloop_pattern_reg,
  hwloop_optimize,
  hwloop_fail
};

/* Run from machine_dependent_reorg, this pass looks for doloop_end insns
   and tries to rewrite the RTL of these loops so that proper Xtensa
   hardware loops are generated.  */

static void
xtensa_reorg_loops (void)
{
  if (TARGET_LOOPS)
    reorg_loops (false, &xtensa_doloop_hooks);
}

/* Implement the TARGET_MACHINE_DEPENDENT_REORG pass.  */

static void
xtensa_reorg (void)
{
  /* We are freeing block_for_insn in the toplev to keep compatibility
     with old MDEP_REORGS that are not CFG based.  Recompute it now.  */
  compute_bb_for_insn ();

  df_analyze ();

  /* Doloop optimization.  */
  xtensa_reorg_loops ();
}

/* Update register usage after having seen the compiler flags.  */

static void
xtensa_conditional_register_usage (void)
{
  unsigned i, c_mask;

  c_mask = TARGET_WINDOWED_ABI ? (1 << 1) : (1 << 2);

  for (i = 0; i < FIRST_PSEUDO_REGISTER; i++)
    {
      /* Set/reset conditionally defined registers from
	 CALL_USED_REGISTERS initializer.  */
      if (call_used_regs[i] > 1)
	call_used_regs[i] = !!(call_used_regs[i] & c_mask);
    }

  /* Remove hard FP register from the preferred reload registers set.  */
  CLEAR_HARD_REG_BIT (reg_class_contents[(int)RL_REGS],
		      HARD_FRAME_POINTER_REGNUM);

  /* Register A0 holds the return address upon entry to a function
     for the CALL0 ABI, but unlike the windowed register ABI, it is
     not reserved for this purpose and may hold other values after
     the return address has been saved.  */
  if (!TARGET_WINDOWED_ABI)
    fixed_regs[A0_REG] = 0;
}

/* Map hard register number to register class */

enum reg_class xtensa_regno_to_class (int regno)
{
  static const enum reg_class regno_to_class[FIRST_PSEUDO_REGISTER] =
    {
      RL_REGS,	SP_REG,		RL_REGS,	RL_REGS,
      RL_REGS,	RL_REGS,	RL_REGS,	RL_REGS,
      RL_REGS,	RL_REGS,	RL_REGS,	RL_REGS,
      RL_REGS,	RL_REGS,	RL_REGS,	RL_REGS,
      AR_REGS,	AR_REGS,	BR_REGS,
      FP_REGS,	FP_REGS,	FP_REGS,	FP_REGS,
      FP_REGS,	FP_REGS,	FP_REGS,	FP_REGS,
      FP_REGS,	FP_REGS,	FP_REGS,	FP_REGS,
      FP_REGS,	FP_REGS,	FP_REGS,	FP_REGS,
      ACC_REG,
    };

  if (regno == HARD_FRAME_POINTER_REGNUM)
    return GR_REGS;
  else
    return regno_to_class[regno];
}

/* Implement TARGET_CONSTANT_ALIGNMENT.  Align string constants and
   constructors to at least a word boundary.  The typical use of this
   macro is to increase alignment for string constants to be word
   aligned so that 'strcpy' calls that copy constants can be done
   inline.  */

static HOST_WIDE_INT
xtensa_constant_alignment (const_tree exp, HOST_WIDE_INT align)
{
  if ((TREE_CODE (exp) == STRING_CST || TREE_CODE (exp) == CONSTRUCTOR)
      && !optimize_size)
    return MAX (align, BITS_PER_WORD);
  return align;
}

static bool
xtensa_can_eliminate (const int from ATTRIBUTE_UNUSED, const int to)
{
  gcc_assert (from == ARG_POINTER_REGNUM || from == FRAME_POINTER_REGNUM);

  /* If we need a frame pointer, ARG_POINTER_REGNUM and FRAME_POINTER_REGNUM
     can only eliminate to HARD_FRAME_POINTER_REGNUM.  */
  return to == HARD_FRAME_POINTER_REGNUM
    || (!frame_pointer_needed && to == STACK_POINTER_REGNUM);
}

/* Implement TARGET_STARTING_FRAME_OFFSET.  */

static HOST_WIDE_INT
xtensa_starting_frame_offset (void)
{
  if (FRAME_GROWS_DOWNWARD)
    return 0;
  return crtl->outgoing_args_size;
}

/* Implement TARGET_ASAN_SHADOW_OFFSET.  */

static unsigned HOST_WIDE_INT
xtensa_asan_shadow_offset (void)
{
  return HOST_WIDE_INT_UC (0x10000000);
}

/* Implement TARGET_FUNCTION_OK_FOR_SIBCALL.  */
static bool
xtensa_function_ok_for_sibcall (tree decl ATTRIBUTE_UNUSED, tree exp ATTRIBUTE_UNUSED)
{
  /* Do not allow sibcalls when windowed registers ABI is in effect.  */
  if (TARGET_WINDOWED_ABI)
    return false;

  return true;
}

static bool
xtensa_can_output_mi_thunk (const_tree thunk_fndecl ATTRIBUTE_UNUSED,
			    HOST_WIDE_INT delta ATTRIBUTE_UNUSED,
			    HOST_WIDE_INT vcall_offset ATTRIBUTE_UNUSED,
			    const_tree function ATTRIBUTE_UNUSED)
{
  if (TARGET_WINDOWED_ABI)
    return false;

  return true;
}

/* Output code to add DELTA to the first argument, and then jump
   to FUNCTION.  Used for C++ multiple inheritance.  */
static void
xtensa_output_mi_thunk (FILE *file, tree thunk ATTRIBUTE_UNUSED,
			HOST_WIDE_INT delta,
			HOST_WIDE_INT vcall_offset,
			tree function)
{
  rtx this_rtx;
  rtx funexp;
  rtx_insn *insn;
  int this_reg_no;
  rtx temp0 = gen_rtx_REG (Pmode, A9_REG);
  const char *fnname = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (thunk));

  reload_completed = 1;

  if (aggregate_value_p (TREE_TYPE (TREE_TYPE (function)), function))
    this_reg_no = 3;
  else
    this_reg_no = 2;

  this_rtx = gen_rtx_REG (Pmode, A0_REG + this_reg_no);

  if (delta)
    xtensa_emit_add_imm (this_rtx, this_rtx, delta, temp0, false);

  if (vcall_offset)
    {
      rtx temp1 = gen_rtx_REG (Pmode, A0_REG + 10);
      rtx addr = temp1;

      emit_move_insn (temp0, gen_rtx_MEM (Pmode, this_rtx));
      if (xtensa_uimm8x4 (vcall_offset))
	addr = plus_constant (Pmode, temp0, vcall_offset);
      else
	xtensa_emit_add_imm (temp1, temp0, vcall_offset, temp1, false);
      emit_move_insn (temp1, gen_rtx_MEM (Pmode, addr));
      emit_insn (gen_add2_insn (this_rtx, temp1));
    }

  /* Generate a tail call to the target function.  */
  if (!TREE_USED (function))
    {
      assemble_external (function);
      TREE_USED (function) = 1;
    }

  funexp = XEXP (DECL_RTL (function), 0);
  funexp = gen_rtx_MEM (FUNCTION_MODE, funexp);
  insn = emit_call_insn (gen_sibcall (funexp, const0_rtx));
  SIBLING_CALL_P (insn) = 1;

  insn = get_insns ();
  shorten_branches (insn);
  assemble_start_function (thunk, fnname);
  final_start_function (insn, file, 1);
  final (insn, file, 1);
  final_end_function ();
  assemble_end_function (thunk, fnname);

  /* Stop pretending to be a post-reload pass.  */
  reload_completed = 0;
}

static rtx
xtensa_delegitimize_address (rtx op)
{
  switch (GET_CODE (op))
    {
    case CONST:
      return xtensa_delegitimize_address (XEXP (op, 0));

    case UNSPEC:
      if (XINT (op, 1) == UNSPEC_PLT)
	return XVECEXP(op, 0, 0);
      break;

    default:
      break;
    }
  return op;
}

/* Implement TARGET_IRA_CHANGE_PSEUDO_ALLOCNO_CLASS, in order to tell
   the register allocator to avoid using ALL_REGS rclass.  */

static reg_class_t
xtensa_ira_change_pseudo_allocno_class (int regno, reg_class_t allocno_class,
					reg_class_t best_class)
{
  if (allocno_class != ALL_REGS)
    return allocno_class;

  if (best_class != ALL_REGS)
    return best_class;

  return FLOAT_MODE_P (PSEUDO_REGNO_MODE (regno)) ? FP_REGS : AR_REGS;
}

/* Implement TARGET_ZERO_CALL_USED_REGS.  */

static HARD_REG_SET
xtensa_zero_call_used_regs (HARD_REG_SET selected_regs)
{
  unsigned int regno;
  int zeroed_regno = -1;
  hard_reg_set_iterator hrsi;
  rtvec argvec, convec;

  EXECUTE_IF_SET_IN_HARD_REG_SET (selected_regs, 1, regno, hrsi)
    {
      if (GP_REG_P (regno))
	{
	  emit_move_insn (gen_rtx_REG (SImode, regno), const0_rtx);
	  if (zeroed_regno < 0)
	    zeroed_regno = regno;
	  continue;
	}
      if (TARGET_BOOLEANS && BR_REG_P (regno))
	{
	  gcc_assert (zeroed_regno >= 0);
	  argvec = rtvec_alloc (1);
	  RTVEC_ELT (argvec, 0) = gen_rtx_REG (SImode, zeroed_regno);
	  convec = rtvec_alloc (1);
	  RTVEC_ELT (convec, 0) = gen_rtx_ASM_INPUT (SImode, "r");
	  emit_insn (gen_rtx_ASM_OPERANDS (VOIDmode, "wsr\t%0, BR",
					   "", 0, argvec, convec,
					   rtvec_alloc (0),
					   UNKNOWN_LOCATION));
	  continue;
	}
      if (TARGET_HARD_FLOAT && FP_REG_P (regno))
	{
	  gcc_assert (zeroed_regno >= 0);
	  emit_move_insn (gen_rtx_REG (SFmode, regno),
			  gen_rtx_REG (SFmode, zeroed_regno));
	  continue;
	}
      if (TARGET_MAC16 && ACC_REG_P (regno))
	{
	  gcc_assert (zeroed_regno >= 0);
	  emit_move_insn (gen_rtx_REG (SImode, regno),
			  gen_rtx_REG (SImode, zeroed_regno));
	  continue;
	}
      CLEAR_HARD_REG_BIT (selected_regs, regno);
    }

  return selected_regs;
}

#include "gt-xtensa.h"
