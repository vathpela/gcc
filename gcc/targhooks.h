/* Default target hook functions.
   Copyright (C) 2003-2025 Free Software Foundation, Inc.

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

#ifndef GCC_TARGHOOKS_H
#define GCC_TARGHOOKS_H

extern bool default_legitimate_address_p (machine_mode, rtx, bool, code_helper);

extern void default_external_libcall (rtx);
extern rtx default_legitimize_address (rtx, rtx, machine_mode);
extern bool default_legitimize_address_displacement (rtx *, rtx *,
						     poly_int64, machine_mode);
extern bool default_const_not_ok_for_debug_p (rtx);

extern int default_unspec_may_trap_p (const_rtx, unsigned);
extern machine_mode default_promote_function_mode (const_tree, machine_mode,
							int *, const_tree, int);
extern machine_mode default_promote_function_mode_always_promote
			(const_tree, machine_mode, int *, const_tree, int);

extern machine_mode default_cc_modes_compatible (machine_mode,
						      machine_mode);

extern bool default_return_in_memory (const_tree, const_tree);

extern rtx default_expand_builtin_saveregs (void);
extern void default_setup_incoming_varargs (cumulative_args_t,
					    const function_arg_info &,
					    int *, int);
extern rtx default_builtin_setjmp_frame_value (void);
extern bool default_pretend_outgoing_varargs_named (cumulative_args_t);

extern scalar_int_mode default_eh_return_filter_mode (void);
extern scalar_int_mode default_libgcc_cmp_return_mode (void);
extern scalar_int_mode default_libgcc_shift_count_mode (void);
extern scalar_int_mode default_unwind_word_mode (void);
extern unsigned HOST_WIDE_INT default_shift_truncation_mask
  (machine_mode);
extern unsigned int default_min_divisions_for_recip_mul (machine_mode);
extern bool default_preferred_div_as_shifts_over_mult
  (const_tree);
extern int default_mode_rep_extended (scalar_int_mode, scalar_int_mode);

extern tree default_stack_protect_guard (void);
extern tree default_external_stack_protect_fail (void);
extern tree default_hidden_stack_protect_fail (void);

extern machine_mode default_mode_for_suffix (char);

extern tree default_cxx_guard_type (void);
extern tree default_cxx_get_cookie_size (tree);
extern tree default_cxx_adjust_cdtor_callabi_fntype (tree);

extern bool hook_pass_by_reference_must_pass_in_stack
  (cumulative_args_t, const function_arg_info &);
extern bool hook_callee_copies_named
  (cumulative_args_t ca, const function_arg_info &);

extern void default_print_operand (FILE *, rtx, int);
extern void default_print_operand_address (FILE *, machine_mode, rtx);
extern bool default_print_operand_punct_valid_p (unsigned char);
extern tree default_mangle_assembler_name (const char *);

extern machine_mode default_translate_mode_attribute (machine_mode);
extern bool default_scalar_mode_supported_p (scalar_mode);
extern bool default_libgcc_floating_mode_supported_p (scalar_float_mode);
extern opt_scalar_float_mode default_floatn_mode (int, bool);
extern bool default_floatn_builtin_p (int);
extern bool targhook_words_big_endian (void);
extern bool targhook_float_words_big_endian (void);
extern bool default_float_exceptions_rounding_supported_p (void);
extern bool default_decimal_float_supported_p (void);
extern bool default_fixed_point_supported_p (void);

extern bool default_has_ifunc_p (void);

extern bool default_predict_doloop_p (class loop *);
extern machine_mode default_preferred_doloop_mode (machine_mode);
extern const char * default_invalid_within_doloop (const rtx_insn *);

extern tree default_builtin_vectorized_function (unsigned int, tree, tree);
extern tree default_builtin_md_vectorized_function (tree, tree, tree);

extern int default_builtin_vectorization_cost (enum vect_cost_for_stmt, tree, int);

extern tree default_builtin_reciprocal (tree);

extern void default_emit_support_tinfos (emit_support_tinfos_callback);

extern HOST_WIDE_INT default_static_rtx_alignment (machine_mode);
extern HOST_WIDE_INT default_constant_alignment (const_tree, HOST_WIDE_INT);
extern HOST_WIDE_INT constant_alignment_word_strings (const_tree,
						      HOST_WIDE_INT);
extern HOST_WIDE_INT default_vector_alignment (const_tree);

extern poly_uint64 default_preferred_vector_alignment (const_tree);
extern bool default_builtin_vector_alignment_reachable (const_tree, bool);
extern bool
default_builtin_support_vector_misalignment (machine_mode mode,
					     const_tree,
					     int, bool, bool);
extern machine_mode default_preferred_simd_mode (scalar_mode mode);
extern machine_mode default_split_reduction (machine_mode);
extern unsigned int default_autovectorize_vector_modes (vector_modes *, bool);
extern opt_machine_mode default_vectorize_related_mode (machine_mode,
							scalar_mode,
							poly_uint64);
extern opt_machine_mode default_get_mask_mode (machine_mode);
extern bool default_empty_mask_is_expensive (unsigned);
extern bool default_conditional_operation_is_expensive (unsigned);
extern vector_costs *default_vectorize_create_costs (vec_info *, bool);

/* OpenACC hooks.  */
extern bool default_goacc_validate_dims (tree, int [], int, unsigned);
extern int default_goacc_dim_limit (int);
extern bool default_goacc_fork_join (gcall *, const int [], bool);
extern void default_goacc_reduction (gcall *);

/* These are here, and not in hooks.[ch], because not all users of
   hooks.h include tm.h, and thus we don't have CUMULATIVE_ARGS.  */

extern bool hook_bool_CUMULATIVE_ARGS_false (cumulative_args_t);
extern bool hook_bool_CUMULATIVE_ARGS_true (cumulative_args_t);

extern bool hook_bool_CUMULATIVE_ARGS_arg_info_false
  (cumulative_args_t, const function_arg_info &);
extern bool hook_bool_CUMULATIVE_ARGS_arg_info_true
  (cumulative_args_t, const function_arg_info &);
extern int hook_int_CUMULATIVE_ARGS_arg_info_0
  (cumulative_args_t, const function_arg_info &);
extern void hook_void_CUMULATIVE_ARGS (cumulative_args_t);
extern void hook_void_CUMULATIVE_ARGS_tree (cumulative_args_t, tree);
extern void hook_void_CUMULATIVE_ARGS_rtx_tree (cumulative_args_t, rtx, tree);
extern const char *hook_invalid_arg_for_unprototyped_fn
  (const_tree, const_tree, const_tree);
extern void default_function_arg_advance
  (cumulative_args_t, const function_arg_info &);
extern bool default_push_argument (unsigned int);
extern HOST_WIDE_INT default_function_arg_offset (machine_mode, const_tree);
extern pad_direction default_function_arg_padding (machine_mode, const_tree);
extern rtx default_function_arg (cumulative_args_t, const function_arg_info &);
extern rtx default_function_incoming_arg (cumulative_args_t,
					  const function_arg_info &);
extern unsigned int default_function_arg_boundary (machine_mode,
						   const_tree);
extern unsigned int default_function_arg_round_boundary (machine_mode,
							 const_tree);
extern bool hook_bool_const_rtx_commutative_p (const_rtx, int);
extern rtx default_function_value (const_tree, const_tree, bool);
extern HARD_REG_SET default_zero_call_used_regs (HARD_REG_SET);
extern rtx default_libcall_value (machine_mode, const_rtx);
extern bool default_function_value_regno_p (const unsigned int);
extern rtx default_internal_arg_pointer (void);
extern rtx default_static_chain (const_tree, bool);
extern void default_trampoline_init (rtx, tree, rtx);
extern void default_emit_call_builtin___clear_cache (rtx, rtx);
extern poly_int64 default_return_pops_args (tree, tree, poly_int64);
extern reg_class_t default_ira_change_pseudo_allocno_class (int, reg_class_t,
							    reg_class_t);
extern int default_ira_callee_saved_register_cost_scale (int);
extern bool default_lra_p (void);
extern int default_register_priority (int);
extern bool default_register_usage_leveling_p (void);
extern bool default_different_addr_displacement_p (void);
extern reg_class_t default_secondary_reload (bool, rtx, reg_class_t,
					     machine_mode,
					     secondary_reload_info *);
extern machine_mode default_secondary_memory_needed_mode (machine_mode);
extern void default_target_option_override (void);
extern void hook_void_bitmap (bitmap);
extern int default_reloc_rw_mask (void);
extern bool default_generate_pic_addr_diff_vec (void);
extern void default_asm_out_constructor (rtx, int);
extern void default_asm_out_destructor (rtx, int);
extern tree default_mangle_decl_assembler_name (tree, tree);
extern tree default_emutls_var_fields (tree, tree *);
extern tree default_emutls_var_init (tree, tree, tree);
extern unsigned int default_hard_regno_nregs (unsigned int, machine_mode);
extern bool default_hard_regno_scratch_ok (unsigned int);
extern bool default_mode_dependent_address_p (const_rtx, addr_space_t);
extern bool default_new_address_profitable_p (rtx, rtx_insn *, rtx);
extern bool default_target_option_valid_attribute_p (tree, tree, tree, int);
extern bool default_target_option_valid_version_attribute_p (tree, tree, tree, int);
extern bool default_target_option_pragma_parse (tree, tree);
extern bool default_target_can_inline_p (tree, tree);
extern bool default_update_ipa_fn_target_info (unsigned int &, const gimple *);
extern bool default_need_ipa_fn_target_info (const_tree, unsigned int &);
extern bool default_valid_pointer_mode (scalar_int_mode);
extern bool default_ref_may_alias_errno (class ao_ref *);
extern scalar_int_mode default_addr_space_pointer_mode (addr_space_t);
extern scalar_int_mode default_addr_space_address_mode (addr_space_t);
extern bool default_addr_space_valid_pointer_mode (scalar_int_mode,
						   addr_space_t);
extern bool default_addr_space_legitimate_address_p (machine_mode, rtx, bool,
						     addr_space_t, code_helper);
extern rtx default_addr_space_legitimize_address (rtx, rtx, machine_mode,
						  addr_space_t);
extern bool default_addr_space_subset_p (addr_space_t, addr_space_t);
extern bool default_addr_space_zero_address_valid (addr_space_t);
extern int default_addr_space_debug (addr_space_t);
extern void default_addr_space_diagnose_usage (addr_space_t, location_t);
extern rtx default_addr_space_convert (rtx, tree, tree);
extern unsigned int default_case_values_threshold (void);
extern bool default_have_conditional_execution (void);
extern bool default_have_ccmp (void);

extern bool default_libc_has_function (enum function_class, tree);
extern bool default_libc_has_fast_function (int fcode);
extern bool no_c99_libc_has_function (enum function_class, tree);
extern bool gnu_libc_has_function (enum function_class, tree);
extern bool bsd_libc_has_function (enum function_class, tree);
extern unsigned default_fortify_source_default_level (void);
extern unsigned default_libm_function_max_error (unsigned, machine_mode, bool);
extern unsigned glibc_linux_libm_function_max_error (unsigned, machine_mode,
						     bool);

extern tree default_builtin_tm_load_store (tree);

extern int default_memory_move_cost (machine_mode, reg_class_t, bool);
extern int default_register_move_cost (machine_mode, reg_class_t,
				       reg_class_t);
extern int default_callee_save_cost (spill_cost_type, unsigned int,
				     machine_mode, unsigned int, int,
				     const HARD_REG_SET &, bool);
extern int default_frame_allocation_cost (frame_cost_type,
					  const HARD_REG_SET &);
extern bool default_slow_unaligned_access (machine_mode, unsigned int);
extern HOST_WIDE_INT default_estimated_poly_value (poly_int64,
						   poly_value_estimate_kind);

extern bool default_use_by_pieces_infrastructure_p (unsigned HOST_WIDE_INT,
						    unsigned int,
						    enum by_pieces_operation,
						    bool);
extern int default_compare_by_pieces_branch_ratio (machine_mode);

extern void default_print_patchable_function_entry (FILE *,
						    unsigned HOST_WIDE_INT,
						    bool);
extern bool default_profile_before_prologue (void);
extern reg_class_t default_preferred_reload_class (rtx, reg_class_t);
extern reg_class_t default_preferred_output_reload_class (rtx, reg_class_t);
extern reg_class_t default_preferred_rename_class (reg_class_t rclass);
extern bool default_class_likely_spilled_p (reg_class_t);
extern unsigned char default_class_max_nregs (reg_class_t, machine_mode);

extern bool default_avoid_store_forwarding_p (vec<store_fwd_info>, rtx, int,
					      bool);

extern enum unwind_info_type default_debug_unwind_info (void);

extern void default_canonicalize_comparison (int *, rtx *, rtx *, bool);

extern section * default_function_section(tree decl, enum node_frequency freq,
					  bool startup, bool exit);
extern unsigned int default_dwarf_poly_indeterminate_value (unsigned int,
							    unsigned int *,
							    int *);
extern machine_mode default_dwarf_frame_reg_mode (int);
extern fixed_size_mode default_get_reg_raw_mode (int);
extern bool default_keep_leaf_when_profiled ();

extern void *default_get_pch_validity (size_t *);
extern const char *default_pch_valid_p (const void *, size_t);

extern void default_asm_output_ident_directive (const char*);

extern scalar_int_mode default_cstore_mode (enum insn_code);
extern bool default_member_type_forces_blk (const_tree, machine_mode);
extern void default_atomic_assign_expand_fenv (tree *, tree *, tree *);
extern tree build_va_arg_indirect_ref (tree);
extern tree std_gimplify_va_arg_expr (tree, tree, gimple_seq *, gimple_seq *);
extern bool can_use_doloop_if_innermost (const widest_int &,
					 const widest_int &,
					 unsigned int, bool);

extern bool default_optab_supported_p (int, machine_mode, machine_mode,
				       optimization_type);
extern unsigned int default_max_noce_ifcvt_seq_cost (edge);
extern bool default_noce_conversion_profitable_p (rtx_insn *,
						  struct noce_if_info *);
extern unsigned int default_min_arithmetic_precision (void);

extern enum flt_eval_method
default_excess_precision (enum excess_precision_type ATTRIBUTE_UNUSED);
extern bool default_bitint_type_info (int, struct bitint_info *);
extern machine_mode default_mode_for_floating_type (enum tree_index);
extern HOST_WIDE_INT default_stack_clash_protection_alloca_probe_range (void);
extern void default_select_early_remat_modes (sbitmap);
extern tree default_preferred_else_value (unsigned, tree, unsigned, tree *);

extern bool default_have_speculation_safe_value (bool);
extern bool speculation_safe_value_not_needed (bool);
extern rtx default_speculation_safe_value (machine_mode, rtx, rtx, rtx);

extern bool default_memtag_can_tag_addresses ();
extern uint8_t default_memtag_tag_size ();
extern uint8_t default_memtag_granule_size ();
extern rtx default_memtag_insert_random_tag (rtx, rtx);
extern rtx default_memtag_add_tag (rtx, poly_int64, uint8_t);
extern rtx default_memtag_set_tag (rtx, rtx, rtx);
extern rtx default_memtag_extract_tag (rtx, rtx);
extern rtx default_memtag_untagged_pointer (rtx, rtx);

#endif /* GCC_TARGHOOKS_H */
