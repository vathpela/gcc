;; GCC machine description for MMX and 3dNOW! instructions
;; Copyright (C) 2005-2025 Free Software Foundation, Inc.
;;
;; This file is part of GCC.
;;
;; GCC is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 3, or (at your option)
;; any later version.
;;
;; GCC is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with GCC; see the file COPYING3.  If not see
;; <http://www.gnu.org/licenses/>.

;; The MMX and 3dNOW! patterns are in the same file because they use
;; the same register file, and 3dNOW! adds a number of extensions to
;; the base integer MMX isa.

;; Note!  Except for the basic move instructions, *all* of these
;; patterns are outside the normal optabs namespace.  This is because
;; use of these registers requires the insertion of emms or femms
;; instructions to return to normal fpu mode.  The compiler doesn't
;; know how to do that itself, which means it's up to the user.  Which
;; means that we should never use any of these patterns except at the
;; direction of the user via a builtin.

(define_c_enum "unspec" [
  UNSPEC_3DNOW

  UNSPEC_MOVNTQ
  UNSPEC_PFRCP
  UNSPEC_PFRCPIT1
  UNSPEC_PFRCPIT2
  UNSPEC_PFRSQRT
  UNSPEC_PFRSQIT1
])

(define_c_enum "unspecv" [
  UNSPECV_EMMS
  UNSPECV_FEMMS
])

;; 8 byte integral modes handled by MMX (and by extension, SSE)
(define_mode_iterator MMXMODEI [V8QI V4HI V2SI])
(define_mode_iterator MMXMODEI8 [V8QI V4HI V2SI (V1DI "TARGET_SSE2")])

;; All 8-byte vector modes handled by MMX
(define_mode_iterator MMXMODE [V8QI V4HI V2SI V1DI V2SF V4HF V4BF])
(define_mode_iterator MMXMODE124 [V8QI V4HI V2SI V2SF])

;; Mix-n-match
(define_mode_iterator MMXMODE12 [V8QI V4HI])
(define_mode_iterator MMXMODE14 [V8QI V2SI])
(define_mode_iterator MMXMODE24 [V4HI V2SI])
(define_mode_iterator MMXMODE248 [V4HI V2SI V1DI])

;; All 4-byte integer/float16 vector modes
(define_mode_iterator V_32 [V4QI V2HI V1SI V2HF V2BF])

(define_mode_iterator V2FI_32 [V2HF V2BF V2HI])
(define_mode_iterator V4FI_64 [V4HF V4BF V4HI])
(define_mode_iterator V4F_64 [V4HF V4BF])
(define_mode_iterator V2F_32 [V2HF V2BF])
;; 4-byte integer vector modes
(define_mode_iterator VI_32 [V4QI V2HI])

;; 8-byte and 4-byte HImode vector modes
(define_mode_iterator VI2_32_64 [(V4HI "TARGET_MMX_WITH_SSE") V2HI])

;; 8-byte, 4-byte and 2-byte QImode vector modes
(define_mode_iterator VI1_16_32_64 [(V8QI "TARGET_MMX_WITH_SSE") V4QI V2QI])

;; 4-byte and 2-byte integer vector modes
(define_mode_iterator VI_16_32 [V4QI V2QI V2HI])

;; 4-byte and 2-byte QImode vector modes
(define_mode_iterator VI1_16_32 [V4QI V2QI])

;; All 2-byte, 4-byte and 8-byte vector modes.
(define_mode_iterator V_16_32_64
   [V2QI V4QI V2HI V1SI V2HF V2BF
    (V8QI "TARGET_64BIT") (V4HI "TARGET_64BIT")
    (V4HF "TARGET_64BIT") (V4BF "TARGET_64BIT")
    (V2SI "TARGET_64BIT") (V2SF "TARGET_64BIT")
    (V1DI "TARGET_64BIT")])

;; V2S* modes
(define_mode_iterator V2FI [V2SF V2SI])

(define_mode_iterator V24FI [V2SF V2SI V4HF V4HI])

(define_mode_iterator V248FI [V2SF V2SI V4HF V4BF V4HI V8QI])

(define_mode_iterator V24FI_32 [V2HF V2BF V2HI V4QI])

;; Mapping from integer vector mode to mnemonic suffix
(define_mode_attr mmxvecsize
  [(V8QI "b") (V4QI "b") (V2QI "b")
   (V4HI "w") (V2HI "w") (V2SI "d") (V1DI "q")])

;; Mapping to same size integral mode.
(define_mode_attr mmxinsnmode
  [(V8QI "DI") (V4QI "SI") (V2QI "HI")
   (V4HI "DI") (V2HI "SI")
   (V2SI "DI")
   (V1DI "DI") (V1SI "SI")
   (V4HF "DI") (V2HF "SI")
   (V4BF "DI") (V2BF "SI")
   (V2SF "DI")])

(define_mode_attr mmxdoublemode
  [(V8QI "V8HI") (V4HI "V4SI")])

(define_mode_attr mmxhalfmode
  [(V4HI "V4QI") (V2HI "V2QI")])

(define_mode_attr mmxbytemode
  [(V4HI "V8QI") (V2HI "V4QI")])

(define_mode_attr mmxhalfmodelower
  [(V4HI "v4qi") (V2HI "v2qi")])

;; Mapping of vector float modes to an integer mode of the same size
(define_mode_attr mmxintvecmode
  [(V2SF "V2SI") (V2SI "V2SI") (V4HI "V4HI") (V8QI "V8QI")
   (V4HF "V4HI") (V2HF "V2HI") (V4BF "V4HI") (V2BF "V2HI")])

(define_mode_attr mmxintvecmodelower
  [(V2SF "v2si") (V2SI "v2si") (V4HI "v4hi") (V8QI "v8qi")
   (V4HF "v4hi") (V2HF "v2hi")])

;; Mapping of vector modes to a vector mode of double size
(define_mode_attr mmxdoublevecmode
  [(V2SF "V4SF") (V2SI "V4SI") (V4HF "V8HF") (V4HI "V8HI")
   (V2HI "V4HI") (V2HF "V4HF") (V2BF "V4BF")])

;; Mapping of vector modes back to the scalar modes
(define_mode_attr mmxscalarmode
  [(V2SI "SI") (V2SF "SF")
   (V4HF "HF") (V4BF "BF")
   (V2HF "HF") (V2BF "BF")
   (V4HI "HI") (V2HI "HI")
   (V8QI "QI")])

(define_mode_attr mmxscalarmodelower
  [(V2SI "si") (V2SF "sf")
   (V4HF "hf") (V4BF "bf")
   (V2HF "hf") (V2BF "bf")
   (V4HI "hi") (V2HI "hi")
   (V8QI "qi")])

(define_mode_attr mmxscalarsize
  [(V1DI "64")
   (V2SI "32") (V2SF "32")
   (V4HF "16") (V4BF "16")
   (V2HF "16") (V2BF "16")
   (V4HI "16") (V2HI "16")
   (V8QI "8")])

(define_mode_attr Yv_Yw
  [(V8QI "Yw") (V4HI "Yw") (V2SI "Yv") (V1DI "Yv") (V2SF "Yv")])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Move patterns
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; All of these patterns are enabled for MMX as well as 3dNOW.
;; This is essential for maintaining stable calling conventions.

(define_expand "mov<mode>"
  [(set (match_operand:MMXMODE 0 "nonimmediate_operand")
	(match_operand:MMXMODE 1 "nonimm_or_0_operand"))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
{
  ix86_expand_vector_move (<MODE>mode, operands);
  DONE;
})

(define_insn "*mov<mode>_internal"
  [(set (match_operand:MMXMODE 0 "nonimmediate_operand"
    "=r ,o ,r,r ,m ,?!y,!y,?!y,m  ,r  ,?!y,v,v,v,m,r,v,!y,*x")
	(match_operand:MMXMODE 1 "nonimm_or_0_operand"
    "rCo,rC,C,rm,rC,C  ,!y,m  ,?!y,?!y,r  ,C,v,m,v,v,r,*x,!y"))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && !(MEM_P (operands[0]) && MEM_P (operands[1]))
   && ix86_hardreg_mov_ok (operands[0], operands[1])"
{
  switch (get_attr_type (insn))
    {
    case TYPE_MULTI:
      return "#";

    case TYPE_IMOV:
      if (get_attr_mode (insn) == MODE_SI)
	return "mov{l}\t{%1, %k0|%k0, %1}";
      else
	return "mov{q}\t{%1, %0|%0, %1}";

    case TYPE_MMX:
      return "pxor\t%0, %0";

    case TYPE_MMXMOV:
      /* Handle broken assemblers that require movd instead of movq.  */
      if (!HAVE_AS_IX86_INTERUNIT_MOVQ
	  && (GENERAL_REG_P (operands[0]) || GENERAL_REG_P (operands[1])))
	return "movd\t{%1, %0|%0, %1}";
      return "movq\t{%1, %0|%0, %1}";

    case TYPE_SSECVT:
      if (SSE_REG_P (operands[0]))
	return "movq2dq\t{%1, %0|%0, %1}";
      else
	return "movdq2q\t{%1, %0|%0, %1}";

    case TYPE_SSELOG1:
      return standard_sse_constant_opcode (insn, operands);

    case TYPE_SSEMOV:
      return ix86_output_ssemov (insn, operands);

    default:
      gcc_unreachable ();
    }
}
  [(set (attr "isa")
     (cond [(eq_attr "alternative" "0,1")
	      (const_string "nox64")
	    (eq_attr "alternative" "2,3,4,9,10")
	      (const_string "x64")
	    (eq_attr "alternative" "15,16")
	      (const_string "x64_sse2")
	    (eq_attr "alternative" "17,18")
	      (const_string "sse2")
	   ]
	   (const_string "*")))
   (set (attr "type")
     (cond [(eq_attr "alternative" "0,1")
	      (const_string "multi")
	    (eq_attr "alternative" "2,3,4")
	      (const_string "imov")
	    (eq_attr "alternative" "5")
	      (const_string "mmx")
	    (eq_attr "alternative" "6,7,8,9,10")
	      (const_string "mmxmov")
	    (eq_attr "alternative" "11")
	      (const_string "sselog1")
	    (eq_attr "alternative" "17,18")
	      (const_string "ssecvt")
	   ]
	   (const_string "ssemov")))
   (set (attr "prefix_rex")
     (if_then_else (eq_attr "alternative" "9,10,15,16")
       (const_string "1")
       (const_string "*")))
   (set (attr "prefix")
     (if_then_else (eq_attr "type" "sselog1,ssemov")
       (const_string "maybe_vex")
       (const_string "orig")))
   (set (attr "prefix_data16")
     (if_then_else
       (and (eq_attr "type" "ssemov") (eq_attr "mode" "DI"))
       (const_string "1")
       (const_string "*")))
   (set (attr "mode")
     (cond [(eq_attr "alternative" "2")
	      (const_string "SI")
	    (eq_attr "alternative" "11,12")
	      (cond [(match_test "<MODE>mode == V2SFmode
				  || <MODE>mode == V4HFmode
				  || <MODE>mode == V4BFmode")
		       (const_string "V4SF")
		     (ior (not (match_test "TARGET_SSE2"))
			  (match_test "optimize_function_for_size_p (cfun)"))
		       (const_string "V4SF")
		    ]
		    (const_string "TI"))

	    (and (eq_attr "alternative" "13")
		 (ior (ior (and (match_test "<MODE>mode == V2SFmode")
				(not (match_test "TARGET_MMX_WITH_SSE")))
			   (not (match_test "TARGET_SSE2")))
		      (match_test "<MODE>mode == V4HFmode
				  || <MODE>mode == V4BFmode")))
	      (const_string "V2SF")

	    (and (eq_attr "alternative" "14")
		 (ior (ior (match_test "<MODE>mode == V2SFmode")
			   (not (match_test "TARGET_SSE2")))
		      (match_test "<MODE>mode == V4HFmode
				  || <MODE>mode == V4BFmode")))
	      (const_string "V2SF")
	   ]
	   (const_string "DI")))
   (set (attr "preferred_for_speed")
     (cond [(eq_attr "alternative" "9,15")
	      (symbol_ref "TARGET_INTER_UNIT_MOVES_FROM_VEC")
	    (eq_attr "alternative" "10,16")
	      (symbol_ref "TARGET_INTER_UNIT_MOVES_TO_VEC")
	   ]
	   (symbol_ref "true")))])

(define_split
  [(set (match_operand:MMXMODE 0 "nonimmediate_gr_operand")
	(match_operand:MMXMODE 1 "nonimmediate_gr_operand"))]
  "!TARGET_64BIT && reload_completed"
  [(const_int 0)]
  "ix86_split_long_move (operands); DONE;")

(define_split
  [(set (match_operand:MMXMODE 0 "nonimmediate_gr_operand")
	(match_operand:MMXMODE 1 "const0_operand"))]
  "!TARGET_64BIT && reload_completed"
  [(const_int 0)]
  "ix86_split_long_move (operands); DONE;")

(define_expand "movmisalign<mode>"
  [(set (match_operand:MMXMODE 0 "nonimmediate_operand")
	(match_operand:MMXMODE 1 "nonimmediate_operand"))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
{
  ix86_expand_vector_move (<MODE>mode, operands);
  DONE;
})

(define_expand "mov<mode>"
  [(set (match_operand:V_32 0 "nonimmediate_operand")
	(match_operand:V_32 1 "nonimm_or_0_operand"))]
  ""
{
  ix86_expand_vector_move (<MODE>mode, operands);
  DONE;
})

(define_insn "*mov<mode>_internal"
  [(set (match_operand:V_32 0 "nonimmediate_operand"
    "=r ,m ,v,v,v,m,r,v")
	(match_operand:V_32 1 "nonimm_or_0_operand"
    "rmC,rC,C,v,m,v,v,r"))]
  "!(MEM_P (operands[0]) && MEM_P (operands[1]))
   && ix86_hardreg_mov_ok (operands[0], operands[1])"
{
  switch (get_attr_type (insn))
    {
    case TYPE_IMOV:
      return "mov{l}\t{%1, %0|%0, %1}";

    case TYPE_SSELOG1:
      return standard_sse_constant_opcode (insn, operands);

    case TYPE_SSEMOV:
      return ix86_output_ssemov (insn, operands);

    default:
      gcc_unreachable ();
    }
}
  [(set (attr "isa")
     (cond [(eq_attr "alternative" "6,7")
	      (const_string "sse2")
	   ]
	   (const_string "*")))
   (set (attr "type")
     (cond [(eq_attr "alternative" "2")
	      (const_string "sselog1")
	    (eq_attr "alternative" "3,4,5,6,7")
	      (const_string "ssemov")
	   ]
	   (const_string "imov")))
   (set (attr "prefix")
     (if_then_else (eq_attr "type" "sselog1,ssemov")
       (const_string "maybe_vex")
       (const_string "orig")))
   (set (attr "prefix_data16")
     (if_then_else (and (eq_attr "type" "ssemov") (eq_attr "mode" "SI"))
       (const_string "1")
       (const_string "*")))
   (set (attr "mode")
     (cond [(eq_attr "alternative" "2,3")
	      (cond [(match_test "<MODE>mode == V2HFmode
				 || <MODE>mode == V2BFmode")
		       (const_string "V4SF")
		     (match_test "TARGET_AVX")
		       (const_string "TI")
		     (ior (not (match_test "TARGET_SSE2"))
			  (match_test "optimize_function_for_size_p (cfun)"))
		       (const_string "V4SF")
		    ]
		    (const_string "TI"))

	    (and (eq_attr "alternative" "4,5")
		 (ior (match_test "<MODE>mode == V2HFmode
				 || <MODE>mode == V2BFmode")
		      (not (match_test "TARGET_SSE2"))))
	      (const_string "SF")
	   ]
	   (const_string "SI")))
   (set (attr "preferred_for_speed")
     (cond [(eq_attr "alternative" "6")
	      (symbol_ref "TARGET_INTER_UNIT_MOVES_FROM_VEC")
	    (eq_attr "alternative" "7")
	      (symbol_ref "TARGET_INTER_UNIT_MOVES_TO_VEC")
	   ]
	   (symbol_ref "true")))])

;; For TARGET_64BIT we always round up to 8 bytes.
(define_insn "*push<mode>2_rex64"
  [(set (match_operand:V_32 0 "push_operand" "=X,X")
	(match_operand:V_32 1 "nonmemory_no_elim_operand" "rC,*v"))]
  "TARGET_64BIT"
  "@
   push{q}\t%q1
   #"
  [(set_attr "type" "push,multi")
   (set_attr "mode" "DI")])

(define_split
  [(set (match_operand:V_32 0 "push_operand")
	(match_operand:V_32 1 "sse_reg_operand"))]
  "TARGET_64BIT && TARGET_SSE && reload_completed"
  [(set (reg:P SP_REG) (plus:P (reg:P SP_REG) (match_dup 2)))
   (set (match_dup 0) (match_dup 1))]
{
  operands[2] = GEN_INT (-PUSH_ROUNDING (GET_MODE_SIZE (<V_32:MODE>mode)));
  /* Preserve memory attributes. */
  operands[0] = replace_equiv_address (operands[0], stack_pointer_rtx);
})

(define_expand "movmisalign<mode>"
  [(set (match_operand:V_32 0 "nonimmediate_operand")
	(match_operand:V_32 1 "nonimmediate_operand"))]
  ""
{
  ix86_expand_vector_move (<MODE>mode, operands);
  DONE;
})

(define_expand "movv2qi"
  [(set (match_operand:V2QI 0 "nonimmediate_operand")
	(match_operand:V2QI 1 "nonimm_or_0_operand"))]
  ""
{
  ix86_expand_vector_move (V2QImode, operands);
  DONE;
})

(define_insn "*movv2qi_internal"
  [(set (match_operand:V2QI 0 "nonimmediate_operand"
    "=r,r,r,m ,v,v,v,jm,m,r,v")
	(match_operand:V2QI 1 "nonimm_or_0_operand"
    "r ,C,m,rC,C,v,m,x,v,v,r"))]
  "!(MEM_P (operands[0]) && MEM_P (operands[1]))
   && ix86_hardreg_mov_ok (operands[0], operands[1])"
{
  switch (get_attr_type (insn))
    {
    case TYPE_IMOV:
      if (get_attr_mode (insn) == MODE_SI)
	return "mov{l}\t{%k1, %k0|%k0, %k1}";
      else
	return "mov{w}\t{%1, %0|%0, %1}";

    case TYPE_IMOVX:
      /* movzwl is faster than movw on p2 due to partial word stalls,
	 though not as fast as an aligned movl.  */
      return "movz{wl|x}\t{%1, %k0|%k0, %1}";

    case TYPE_SSELOG1:
      if (satisfies_constraint_C (operands[1]))
	return standard_sse_constant_opcode (insn, operands);

      if (SSE_REG_P (operands[0]))
	return "%vpinsrw\t{$0, %1, %d0|%d0, %1, 0}";
      else
	return "%vpextrw\t{$0, %1, %0|%0, %1, 0}";

    case TYPE_SSEMOV:
      return ix86_output_ssemov (insn, operands);

    default:
      gcc_unreachable ();
    }
}
  [(set (attr "isa")
	(cond [(eq_attr "alternative" "6,9,10")
		  (const_string "sse2")
	       (eq_attr "alternative" "7")
		  (const_string "sse4_noavx")
	       (eq_attr "alternative" "8")
		  (const_string "avx")
	       ]
	       (const_string "*")))
   (set (attr "addr")
	(if_then_else (eq_attr "alternative" "7")
		      (const_string "gpr16")
		      (const_string "*")))
   (set (attr "type")
     (cond [(eq_attr "alternative" "6,7,8")
	      (if_then_else (match_test "TARGET_AVX512FP16")
		(const_string "ssemov")
		(const_string "sselog1"))
	    (eq_attr "alternative" "4")
	      (const_string "sselog1")
	    (eq_attr "alternative" "5,9,10")
	      (const_string "ssemov")
	    (match_test "optimize_function_for_size_p (cfun)")
	      (const_string "imov")
	    (and (eq_attr "alternative" "0")
		 (ior (not (match_test "TARGET_PARTIAL_REG_STALL"))
		      (not (match_test "TARGET_HIMODE_MATH"))))
	      (const_string "imov")
	    (and (eq_attr "alternative" "1,2")
		 (match_operand:V2QI 1 "aligned_operand"))
	      (const_string "imov")
	    (and (match_test "TARGET_MOVX")
		 (eq_attr "alternative" "0,2"))
	      (const_string "imovx")
	   ]
	   (const_string "imov")))
   (set (attr "prefix")
	(cond [(eq_attr "alternative" "4,5,6,7,8,9,10")
		 (const_string "maybe_evex")
	      ]
	      (const_string "orig")))
   (set (attr "mode")
     (cond [(eq_attr "alternative" "6,7,8")
	      (if_then_else (match_test "TARGET_AVX512FP16")
		(const_string "HI")
		(const_string "TI"))
	    (eq_attr "alternative" "9,10")
	      (if_then_else (match_test "TARGET_AVX512FP16")
		(const_string "HI")
		(const_string "SI"))
	    (eq_attr "alternative" "4")
	      (cond [(match_test "TARGET_AVX")
		       (const_string "TI")
		     (ior (not (match_test "TARGET_SSE2"))
			  (match_test "optimize_function_for_size_p (cfun)"))
		       (const_string "V4SF")
		    ]
		    (const_string "TI"))
	    (eq_attr "alternative" "5")
	      (cond [(match_test "TARGET_AVX512FP16")
		       (const_string "HF")
		     (match_test "TARGET_AVX")
		       (const_string "TI")
		     (ior (not (match_test "TARGET_SSE2"))
			  (match_test "optimize_function_for_size_p (cfun)"))
		       (const_string "V4SF")
		    ]
		    (const_string "TI"))
	    (eq_attr "type" "imovx")
	      (const_string "SI")
	    (and (eq_attr "alternative" "1,2")
		 (match_operand:V2QI 1 "aligned_operand"))
	      (const_string "SI")
	    (and (eq_attr "alternative" "0")
		 (ior (not (match_test "TARGET_PARTIAL_REG_STALL"))
		      (not (match_test "TARGET_HIMODE_MATH"))))
	      (const_string "SI")
	    ]
	    (const_string "HI")))
   (set (attr "preferred_for_speed")
     (cond [(eq_attr "alternative" "9")
	      (symbol_ref "TARGET_INTER_UNIT_MOVES_FROM_VEC")
	    (eq_attr "alternative" "10")
	      (symbol_ref "TARGET_INTER_UNIT_MOVES_TO_VEC")
	   ]
	   (symbol_ref "true")))])

(define_split
  [(set (match_operand:V_16_32_64 0 "general_reg_operand")
	(match_operand:V_16_32_64 1 "memory_operand"))]
  "reload_completed
   && SYMBOL_REF_P (XEXP (operands[1], 0))
   && CONSTANT_POOL_ADDRESS_P (XEXP (operands[1], 0))"
  [(set (match_dup 0) (match_dup 1))]
{
  rtx op1 = avoid_constant_pool_reference (operands[1]);

  if (!CONST_VECTOR_P (op1))
    FAIL;

  HOST_WIDE_INT val = ix86_convert_const_vector_to_integer (op1, <MODE>mode);

  operands[0] = lowpart_subreg (<mmxinsnmode>mode, operands[0], <MODE>mode);
  operands[1] = GEN_INT (val);
})

;; 16-bit, 32-bit and 64-bit constant vector stores.  After reload,
;; convert them to immediate integer stores.
(define_insn_and_split "*mov<mode>_imm"
  [(set (match_operand:V_16_32_64 0 "memory_operand" "=m")
	(match_operand:V_16_32_64 1 "x86_64_const_vector_operand" "i"))]
  ""
  "#"
  "&& reload_completed"
  [(set (match_dup 0) (match_dup 1))]
{
  rtx op1 = operands[1];
  HOST_WIDE_INT val = ix86_convert_const_vector_to_integer (op1, <MODE>mode);

  operands[0] = adjust_address (operands[0], <mmxinsnmode>mode, 0);
  operands[1] = GEN_INT (val);
})

;; We always round up to UNITS_PER_WORD bytes.
(define_insn "*pushv2qi2"
  [(set (match_operand:V2QI 0 "push_operand" "=X,X")
	(match_operand:V2QI 1 "nonmemory_no_elim_operand" "rC,v"))]
  ""
  "* return TARGET_64BIT ? \"push{q}\t%q1\" : \"push{l}\t%k1\";
   #"
  [(set_attr "isa" "*,sse4")
   (set_attr "type" "push,multi")
   (set (attr "mode")
     (cond [(eq_attr "alternative" "0")
	      (if_then_else (match_test "TARGET_64BIT")
		(const_string "DI")
		(const_string "SI"))
	    (eq_attr "alternative" "1")
	      (if_then_else (match_test "TARGET_AVX512FP16")
		(const_string "HI")
		(const_string "TI"))
	   ]
	   (const_string "HI")))])

(define_split
  [(set (match_operand:V2QI 0 "push_operand")
	(match_operand:V2QI 1 "sse_reg_operand"))]
  "TARGET_SSE4_1 && reload_completed"
  [(set (reg:P SP_REG) (plus:P (reg:P SP_REG) (match_dup 2)))
   (set (match_dup 0) (match_dup 1))]
{
  operands[2] = GEN_INT (-PUSH_ROUNDING (GET_MODE_SIZE (V2QImode)));
  /* Preserve memory attributes. */
  operands[0] = replace_equiv_address (operands[0], stack_pointer_rtx);
})

(define_expand "movmisalignv2qi"
  [(set (match_operand:V2QI 0 "nonimmediate_operand")
	(match_operand:V2QI 1 "nonimmediate_operand"))]
  ""
{
  ix86_expand_vector_move (V2QImode, operands);
  DONE;
})

(define_insn "sse_movntq"
  [(set (match_operand:DI 0 "memory_operand" "=m,m")
	(unspec:DI [(match_operand:DI 1 "register_operand" "y,r")]
		   UNSPEC_MOVNTQ))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && (TARGET_SSE || TARGET_3DNOW_A)"
  "@
   movntq\t{%1, %0|%0, %1}
   movnti\t{%1, %0|%0, %1}"
  [(set_attr "isa" "*,x64")
   (set_attr "mmx_isa" "native,*")
   (set_attr "type" "mmxmov,ssemov")
   (set_attr "mode" "DI")])

(define_expand "movq_<mode>_to_sse"
  [(set (match_operand:<mmxdoublevecmode> 0 "register_operand")
	(vec_concat:<mmxdoublevecmode>
	  (match_operand:V24FI 1 "nonimmediate_operand")
	  (match_dup 2)))]
  "TARGET_SSE2"
{
  if (<MODE>mode != V2SImode
      && !flag_trapping_math)
    {
      rtx op1 = force_reg (<MODE>mode, operands[1]);
      emit_move_insn (operands[0], lowpart_subreg (<mmxdoublevecmode>mode,
						   op1, <MODE>mode));
      DONE;
    }

  operands[2] = CONST0_RTX (<MODE>mode);
})

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel single-precision floating point arithmetic
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_expand "<code>v2sf2"
  [(set (match_operand:V2SF 0 "register_operand")
	(absneg:V2SF
	  (match_operand:V2SF 1 "register_operand")))]
  "TARGET_MMX_WITH_SSE"
  "ix86_expand_fp_absneg_operator (<CODE>, V2SFmode, operands); DONE;")

(define_insn_and_split "*mmx_<code>v2sf2"
  [(set (match_operand:V2SF 0 "register_operand" "=x,x,x")
	(absneg:V2SF
	  (match_operand:V2SF 1 "register_operand" "0,x,x")))
   (use (match_operand:V2SF 2 "nonimmediate_operand" "x,0,x"))]
  "TARGET_MMX_WITH_SSE"
  "#"
  "&& reload_completed"
  [(set (match_dup 0)
	(<absneg_op>:V2SF (match_dup 1) (match_dup 2)))]
{
  if (!TARGET_AVX && operands_match_p (operands[0], operands[2]))
    std::swap (operands[1], operands[2]);
}
  [(set_attr "isa" "noavx,noavx,avx")])

(define_insn_and_split "*mmx_nabsv2sf2"
  [(set (match_operand:V2SF 0 "register_operand" "=x,x,x")
	(neg:V2SF
	  (abs:V2SF
	    (match_operand:V2SF 1 "register_operand" "0,x,x"))))
   (use (match_operand:V2SF 2 "nonimmediate_operand" "x,0,x"))]
  "TARGET_MMX_WITH_SSE"
  "#"
  "&& reload_completed"
  [(set (match_dup 0)
	(ior:V2SF (match_dup 1) (match_dup 2)))]
{
  if (!TARGET_AVX && operands_match_p (operands[0], operands[2]))
    std::swap (operands[1], operands[2]);
}
  [(set_attr "isa" "noavx,noavx,avx")])

(define_expand "<insn>v2sf3"
  [(set (match_operand:V2SF 0 "register_operand")
	(plusminusmult:V2SF
	  (match_operand:V2SF 1 "nonimmediate_operand")
	  (match_operand:V2SF 2 "nonimmediate_operand")))]
  "TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op2 = gen_reg_rtx (V4SFmode);
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2sf_to_sse (op2, operands[2]));
  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_<insn>v4sf3 (op0, op1, op2));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

(define_expand "mmx_addv2sf3"
  [(parallel
     [(set (match_operand:V2SF 0 "register_operand")
	   (plus:V2SF
	     (match_operand:V2SF 1 "nonimmediate_operand")
	     (match_operand:V2SF 2 "nonimmediate_operand")))
      (unspec [(const_int 0)] UNSPEC_3DNOW)])]
  "TARGET_3DNOW"
  "ix86_fixup_binary_operands_no_copy (PLUS, V2SFmode, operands);")

(define_insn "*mmx_addv2sf3"
  [(set (match_operand:V2SF 0 "register_operand" "=y")
	(plus:V2SF (match_operand:V2SF 1 "nonimmediate_operand" "%0")
		   (match_operand:V2SF 2 "nonimmediate_operand" "ym")))
   (unspec [(const_int 0)] UNSPEC_3DNOW)]
  "TARGET_3DNOW && ix86_binary_operator_ok (PLUS, V2SFmode, operands)"
  "pfadd\t{%2, %0|%0, %2}"
  [(set_attr "type" "mmxadd")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

(define_expand "mmx_subv2sf3"
  [(parallel
     [(set (match_operand:V2SF 0 "register_operand")
	   (minus:V2SF (match_operand:V2SF 1 "register_operand")
		       (match_operand:V2SF 2 "nonimmediate_operand")))
      (unspec [(const_int 0)] UNSPEC_3DNOW)])]
  "TARGET_3DNOW")

(define_expand "mmx_subrv2sf3"
  [(parallel
     [(set (match_operand:V2SF 0 "register_operand")
	   (minus:V2SF (match_operand:V2SF 2 "register_operand")
		       (match_operand:V2SF 1 "nonimmediate_operand")))
      (unspec [(const_int 0)] UNSPEC_3DNOW)])]
  "TARGET_3DNOW")

(define_insn "*mmx_subv2sf3"
  [(set (match_operand:V2SF 0 "register_operand" "=y,y")
	(minus:V2SF (match_operand:V2SF 1 "nonimmediate_operand" "0,ym")
		    (match_operand:V2SF 2 "nonimmediate_operand" "ym,0")))
   (unspec [(const_int 0)] UNSPEC_3DNOW)]
  "TARGET_3DNOW && !(MEM_P (operands[0]) && MEM_P (operands[1]))"
  "@
   pfsub\t{%2, %0|%0, %2}
   pfsubr\t{%1, %0|%0, %1}"
  [(set_attr "type" "mmxadd")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

(define_expand "mmx_mulv2sf3"
  [(parallel
     [(set (match_operand:V2SF 0 "register_operand")
	   (mult:V2SF (match_operand:V2SF 1 "nonimmediate_operand")
		      (match_operand:V2SF 2 "nonimmediate_operand")))
      (unspec [(const_int 0)] UNSPEC_3DNOW)])]
  "TARGET_3DNOW"
  "ix86_fixup_binary_operands_no_copy (MULT, V2SFmode, operands);")

(define_insn "*mmx_mulv2sf3"
  [(set (match_operand:V2SF 0 "register_operand" "=y")
	(mult:V2SF (match_operand:V2SF 1 "nonimmediate_operand" "%0")
		   (match_operand:V2SF 2 "nonimmediate_operand" "ym")))
   (unspec [(const_int 0)] UNSPEC_3DNOW)]
  "TARGET_3DNOW && ix86_binary_operator_ok (MULT, V2SFmode, operands)"
  "pfmul\t{%2, %0|%0, %2}"
  [(set_attr "type" "mmxmul")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

(define_expand "divv2sf3"
  [(set (match_operand:V2SF 0 "register_operand")
	(div:V2SF (match_operand:V2SF 1 "register_operand")
		  (match_operand:V2SF 2 "register_operand")))]
  "TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op2 = gen_reg_rtx (V4SFmode);
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  rtx tmp = gen_rtx_VEC_CONCAT (V4SFmode, operands[2],
				force_reg (V2SFmode, CONST1_RTX (V2SFmode)));
  emit_insn (gen_rtx_SET (op2, tmp));
  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_divv4sf3 (op0, op1, op2));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

(define_expand "<code>v2sf3"
  [(set (match_operand:V2SF 0 "register_operand")
	(smaxmin:V2SF
	  (match_operand:V2SF 1 "register_operand")
	  (match_operand:V2SF 2 "register_operand")))]
  "TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op2 = gen_reg_rtx (V4SFmode);
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2sf_to_sse (op2, operands[2]));
  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_<code>v4sf3 (op0, op1, op2));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

(define_expand "mmx_<code>v2sf3"
  [(parallel
     [(set (match_operand:V2SF 0 "register_operand")
	   (smaxmin:V2SF (match_operand:V2SF 1 "nonimmediate_operand")
			 (match_operand:V2SF 2 "nonimmediate_operand")))
      (unspec [(const_int 0)] UNSPEC_3DNOW)])]
  "TARGET_3DNOW"
{
  if (!flag_finite_math_only || flag_signed_zeros)
    {
      operands[1] = force_reg (V2SFmode, operands[1]);
      emit_insn (gen_mmx_ieee_<maxmin_float>v2sf3
		 (operands[0], operands[1], operands[2]));
      DONE;
    }
  else
    ix86_fixup_binary_operands_no_copy (<CODE>, V2SFmode, operands);
})

;; These versions of the min/max patterns are intentionally ignorant of
;; their behavior wrt -0.0 and NaN (via the commutative operand mark).
;; Since both the tree-level MAX_EXPR and the rtl-level SMAX operator
;; are undefined in this condition, we're certain this is correct.

(define_insn "*mmx_<code>v2sf3"
  [(set (match_operand:V2SF 0 "register_operand" "=y")
	(smaxmin:V2SF (match_operand:V2SF 1 "nonimmediate_operand" "%0")
		      (match_operand:V2SF 2 "nonimmediate_operand" "ym")))
   (unspec [(const_int 0)] UNSPEC_3DNOW)]
  "TARGET_3DNOW && ix86_binary_operator_ok (<CODE>, V2SFmode, operands)"
  "pf<maxmin_float>\t{%2, %0|%0, %2}"
  [(set_attr "type" "mmxadd")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

;; These versions of the min/max patterns implement exactly the operations
;;   min = (op1 < op2 ? op1 : op2)
;;   max = (!(op1 < op2) ? op1 : op2)
;; Their operands are not commutative, and thus they may be used in the
;; presence of -0.0 and NaN.

(define_insn "mmx_ieee_<ieee_maxmin>v2sf3"
  [(set (match_operand:V2SF 0 "register_operand" "=y")
	(unspec:V2SF
	  [(match_operand:V2SF 1 "register_operand" "0")
	   (match_operand:V2SF 2 "nonimmediate_operand" "ym")]
	  IEEE_MAXMIN))
   (unspec [(const_int 0)] UNSPEC_3DNOW)]
  "TARGET_3DNOW"
  "pf<ieee_maxmin>\t{%2, %0|%0, %2}"
  [(set_attr "type" "mmxadd")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

(define_insn "mmx_rcpv2sf2"
  [(set (match_operand:V2SF 0 "register_operand" "=y")
	(unspec:V2SF [(match_operand:V2SF 1 "nonimmediate_operand" "ym")]
		     UNSPEC_PFRCP))]
  "TARGET_3DNOW"
  "pfrcp\t{%1, %0|%0, %1}"
  [(set_attr "type" "mmx")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

(define_insn "mmx_rcpit1v2sf3"
  [(set (match_operand:V2SF 0 "register_operand" "=y")
	(unspec:V2SF [(match_operand:V2SF 1 "register_operand" "0")
		      (match_operand:V2SF 2 "nonimmediate_operand" "ym")]
		     UNSPEC_PFRCPIT1))]
  "TARGET_3DNOW"
  "pfrcpit1\t{%2, %0|%0, %2}"
  [(set_attr "type" "mmx")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

(define_insn "mmx_rcpit2v2sf3"
  [(set (match_operand:V2SF 0 "register_operand" "=y")
	(unspec:V2SF [(match_operand:V2SF 1 "register_operand" "0")
		      (match_operand:V2SF 2 "nonimmediate_operand" "ym")]
		     UNSPEC_PFRCPIT2))]
  "TARGET_3DNOW"
  "pfrcpit2\t{%2, %0|%0, %2}"
  [(set_attr "type" "mmx")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

(define_expand "sqrtv2sf2"
  [(set (match_operand:V2SF 0 "register_operand")
	(sqrt:V2SF (match_operand:V2SF 1 "nonimmediate_operand")))]
  "TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_sqrtv4sf2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

(define_insn "mmx_rsqrtv2sf2"
  [(set (match_operand:V2SF 0 "register_operand" "=y")
	(unspec:V2SF [(match_operand:V2SF 1 "nonimmediate_operand" "ym")]
		     UNSPEC_PFRSQRT))]
  "TARGET_3DNOW"
  "pfrsqrt\t{%1, %0|%0, %1}"
  [(set_attr "type" "mmx")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

(define_insn "mmx_rsqit1v2sf3"
  [(set (match_operand:V2SF 0 "register_operand" "=y")
	(unspec:V2SF [(match_operand:V2SF 1 "register_operand" "0")
		      (match_operand:V2SF 2 "nonimmediate_operand" "ym")]
		     UNSPEC_PFRSQIT1))]
  "TARGET_3DNOW"
  "pfrsqit1\t{%2, %0|%0, %2}"
  [(set_attr "type" "mmx")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

(define_expand "mmx_haddv2sf3"
  [(set (match_operand:V2SF 0 "register_operand")
	(vec_concat:V2SF
	  (plus:SF
	    (vec_select:SF
	      (match_operand:V2SF 1 "register_operand")
	      (parallel [(const_int 0)]))
	    (vec_select:SF (match_dup 1) (parallel [(const_int 1)])))
	  (plus:SF
	    (vec_select:SF
	      (match_operand:V2SF 2 "nonimmediate_operand")
	      (parallel [(const_int 0)]))
	    (vec_select:SF (match_dup 2) (parallel [(const_int 1)])))))]
  "TARGET_3DNOW")

(define_insn "*mmx_haddv2sf3"
  [(set (match_operand:V2SF 0 "register_operand" "=y")
	(vec_concat:V2SF
	  (plus:SF
	    (vec_select:SF
	      (match_operand:V2SF 1 "register_operand" "0")
	      (parallel [(match_operand:SI 3 "const_0_to_1_operand")]))
	    (vec_select:SF (match_dup 1)
	    (parallel [(match_operand:SI 4 "const_0_to_1_operand")])))
	  (plus:SF
	    (vec_select:SF
	      (match_operand:V2SF 2 "nonimmediate_operand" "ym")
	      (parallel [(match_operand:SI 5 "const_0_to_1_operand")]))
	    (vec_select:SF (match_dup 2)
	    (parallel [(match_operand:SI 6 "const_0_to_1_operand")])))))]
  "TARGET_3DNOW
   && INTVAL (operands[3]) != INTVAL (operands[4])
   && INTVAL (operands[5]) != INTVAL (operands[6])"
  "pfacc\t{%2, %0|%0, %2}"
  [(set_attr "type" "mmxadd")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

(define_insn_and_split "*mmx_haddv2sf3_low"
  [(set (match_operand:SF 0 "register_operand")
	(plus:SF
	  (vec_select:SF
	    (match_operand:V2SF 1 "nonimmediate_operand")
	    (parallel [(match_operand:SI 2 "const_0_to_1_operand")]))
	  (vec_select:SF
	    (match_dup 1)
	    (parallel [(match_operand:SI 3 "const_0_to_1_operand")]))))]
  "TARGET_SSE3 && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math
   && INTVAL (operands[2]) != INTVAL (operands[3])
   && ix86_pre_reload_split ()"
  "#"
  "&& 1"
  [(const_int 0)]
{
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_sse3_haddv4sf3 (op0, op1, op1));

  emit_move_insn (operands[0], lowpart_subreg (SFmode, op0, V4SFmode));
  DONE;
})

(define_insn "mmx_hsubv2sf3"
  [(set (match_operand:V2SF 0 "register_operand" "=y")
	(vec_concat:V2SF
	  (minus:SF
	    (vec_select:SF
	      (match_operand:V2SF 1 "register_operand" "0")
	      (parallel [(const_int  0)]))
	    (vec_select:SF (match_dup 1) (parallel [(const_int 1)])))
	  (minus:SF
	    (vec_select:SF
	      (match_operand:V2SF 2 "nonimmediate_operand" "ym")
	      (parallel [(const_int  0)]))
	    (vec_select:SF (match_dup 2) (parallel [(const_int 1)])))))]
  "TARGET_3DNOW_A"
  "pfnacc\t{%2, %0|%0, %2}"
  [(set_attr "type" "mmxadd")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

(define_insn_and_split "*mmx_hsubv2sf3_low"
  [(set (match_operand:SF 0 "register_operand")
	(minus:SF
	  (vec_select:SF
	    (match_operand:V2SF 1 "register_operand")
	    (parallel [(const_int 0)]))
	  (vec_select:SF
	    (match_dup 1)
	    (parallel [(const_int 1)]))))]
  "TARGET_SSE3 && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math
   && ix86_pre_reload_split ()"
  "#"
  "&& 1"
  [(const_int 0)]
{
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_sse3_hsubv4sf3 (op0, op1, op1));

  emit_move_insn (operands[0], lowpart_subreg (SFmode, op0, V4SFmode));
  DONE;
})

(define_expand "mmx_haddsubv2sf3"
  [(set (match_operand:V2SF 0 "register_operand")
	(vec_concat:V2SF
	  (minus:SF
	    (vec_select:SF
	      (match_operand:V2SF 1 "register_operand")
	      (parallel [(const_int 0)]))
	    (vec_select:SF (match_dup 1) (parallel [(const_int 1)])))
	  (plus:SF
	    (vec_select:SF
	      (match_operand:V2SF 2 "nonimmediate_operand")
	      (parallel [(const_int 0)]))
	    (vec_select:SF (match_dup 2) (parallel [(const_int 1)])))))]
  "TARGET_3DNOW_A")

(define_insn "*mmx_haddsubv2sf3"
  [(set (match_operand:V2SF 0 "register_operand" "=y")
	(vec_concat:V2SF
	  (minus:SF
	    (vec_select:SF
	      (match_operand:V2SF 1 "register_operand" "0")
	      (parallel [(const_int  0)]))
	    (vec_select:SF (match_dup 1) (parallel [(const_int 1)])))
	  (plus:SF
	    (vec_select:SF
	      (match_operand:V2SF 2 "nonimmediate_operand" "ym")
	      (parallel [(match_operand:SI 3 "const_0_to_1_operand")]))
	    (vec_select:SF
	      (match_dup 2)
	      (parallel [(match_operand:SI 4 "const_0_to_1_operand")])))))]
  "TARGET_3DNOW_A
   && INTVAL (operands[3]) != INTVAL (operands[4])"
  "pfpnacc\t{%2, %0|%0, %2}"
  [(set_attr "type" "mmxadd")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

(define_expand "vec_addsubv2sf3"
  [(set (match_operand:V2SF 0 "register_operand")
	(vec_merge:V2SF
	  (minus:V2SF
	    (match_operand:V2SF 1 "nonimmediate_operand")
	    (match_operand:V2SF 2 "nonimmediate_operand"))
	  (plus:V2SF (match_dup 1) (match_dup 2))
	  (const_int 1)))]
  "TARGET_SSE3 && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op2 = gen_reg_rtx (V4SFmode);
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2sf_to_sse (op2, operands[2]));
  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_vec_addsubv4sf3 (op0, op1, op2));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

(define_expand "vec_fmaddsubv2sf4"
  [(match_operand:V2SF 0 "register_operand")
   (match_operand:V2SF 1 "nonimmediate_operand")
   (match_operand:V2SF 2 "nonimmediate_operand")
   (match_operand:V2SF 3 "nonimmediate_operand")]
  "(TARGET_FMA || TARGET_FMA4 || TARGET_AVX512VL)
   && TARGET_MMX_WITH_SSE
   && ix86_partial_vec_fp_math"
{
  rtx op3 = gen_reg_rtx (V4SFmode);
  rtx op2 = gen_reg_rtx (V4SFmode);
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2sf_to_sse (op3, operands[3]));
  emit_insn (gen_movq_v2sf_to_sse (op2, operands[2]));
  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_vec_fmaddsubv4sf4 (op0, op1, op2, op3));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

(define_expand "vec_fmsubaddv2sf4"
  [(match_operand:V2SF 0 "register_operand")
   (match_operand:V2SF 1 "nonimmediate_operand")
   (match_operand:V2SF 2 "nonimmediate_operand")
   (match_operand:V2SF 3 "nonimmediate_operand")]
  "(TARGET_FMA || TARGET_FMA4 || TARGET_AVX512VL)
   && TARGET_MMX_WITH_SSE
   && ix86_partial_vec_fp_math"
{
  rtx op3 = gen_reg_rtx (V4SFmode);
  rtx op2 = gen_reg_rtx (V4SFmode);
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2sf_to_sse (op3, operands[3]));
  emit_insn (gen_movq_v2sf_to_sse (op2, operands[2]));
  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_vec_fmsubaddv4sf4 (op0, op1, op2, op3));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel single-precision floating point comparisons
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_expand "mmx_eqv2sf3"
  [(parallel
     [(set (match_operand:V2SI 0 "register_operand")
	   (eq:V2SI (match_operand:V2SF 1 "nonimmediate_operand")
		    (match_operand:V2SF 2 "nonimmediate_operand")))
      (unspec [(const_int 0)] UNSPEC_3DNOW)])]
  "TARGET_3DNOW"
  "ix86_fixup_binary_operands_no_copy (EQ, V2SFmode, operands);")

(define_insn "*mmx_eqv2sf3"
  [(set (match_operand:V2SI 0 "register_operand" "=y")
	(eq:V2SI (match_operand:V2SF 1 "nonimmediate_operand" "%0")
		 (match_operand:V2SF 2 "nonimmediate_operand" "ym")))
   (unspec [(const_int 0)] UNSPEC_3DNOW)]
  "TARGET_3DNOW && ix86_binary_operator_ok (EQ, V2SFmode, operands)"
  "pfcmpeq\t{%2, %0|%0, %2}"
  [(set_attr "type" "mmxcmp")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

(define_insn "mmx_gtv2sf3"
  [(set (match_operand:V2SI 0 "register_operand" "=y")
	(gt:V2SI (match_operand:V2SF 1 "register_operand" "0")
		 (match_operand:V2SF 2 "nonimmediate_operand" "ym")))
   (unspec [(const_int 0)] UNSPEC_3DNOW)]
  "TARGET_3DNOW"
  "pfcmpgt\t{%2, %0|%0, %2}"
  [(set_attr "type" "mmxcmp")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

(define_insn "mmx_gev2sf3"
  [(set (match_operand:V2SI 0 "register_operand" "=y")
	(ge:V2SI (match_operand:V2SF 1 "register_operand" "0")
		 (match_operand:V2SF 2 "nonimmediate_operand" "ym")))
   (unspec [(const_int 0)] UNSPEC_3DNOW)]
  "TARGET_3DNOW"
  "pfcmpge\t{%2, %0|%0, %2}"
  [(set_attr "type" "mmxcmp")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

(define_expand "vec_cmpv2sfv2si"
  [(set (match_operand:V2SI 0 "register_operand")
	(match_operator:V2SI 1 ""
	  [(match_operand:V2SF 2 "nonimmediate_operand")
	   (match_operand:V2SF 3 "nonimmediate_operand")]))]
  "TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx ops[4];
  ops[3] = gen_reg_rtx (V4SFmode);
  ops[2] = gen_reg_rtx (V4SFmode);
  ops[1] = gen_rtx_fmt_ee (GET_CODE (operands[1]), V4SImode, ops[2], ops[3]);
  ops[0] = gen_reg_rtx (V4SImode);

  emit_insn (gen_movq_v2sf_to_sse (ops[3], operands[3]));
  emit_insn (gen_movq_v2sf_to_sse (ops[2], operands[2]));

  bool ok = ix86_expand_fp_vec_cmp (ops);
  gcc_assert (ok);

  emit_move_insn (operands[0], lowpart_subreg (V2SImode, ops[0], V4SImode));
  DONE;
})

(define_insn "@sse4_1_insertps_<mode>"
  [(set (match_operand:V2FI 0 "register_operand" "=Yr,*x,v")
	(unspec:V2FI
	  [(match_operand:V2FI 2 "nonimmediate_operand" "Yrjm,*xjm,vm")
	   (match_operand:V2FI 1 "register_operand" "0,0,v")
	   (match_operand:SI 3 "const_0_to_255_operand")]
	  UNSPEC_INSERTPS))]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE"
{
  if (MEM_P (operands[2]))
    {
      unsigned count_s = INTVAL (operands[3]) >> 6;
      if (count_s)
	operands[3] = GEN_INT (INTVAL (operands[3]) & 0x3f);
      operands[2] = adjust_address_nv (operands[2],
				       <mmxscalarmode>mode, count_s * 4);
    }
  switch (which_alternative)
    {
    case 0:
    case 1:
      return "insertps\t{%3, %2, %0|%0, %2, %3}";
    case 2:
      return "vinsertps\t{%3, %2, %1, %0|%0, %1, %2, %3}";
    default:
      gcc_unreachable ();
    }
}
  [(set_attr "isa" "noavx,noavx,avx")
   (set_attr "addr" "gpr16,gpr16,*")
   (set_attr "type" "sselog")
   (set_attr "prefix_data16" "1,1,*")
   (set_attr "prefix_extra" "1")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "orig,orig,maybe_evex")
   (set_attr "mode" "V4SF")])

(define_insn "*mmx_blendps"
  [(set (match_operand:V2SF 0 "register_operand" "=Yr,*x,x")
	(vec_merge:V2SF
	  (match_operand:V2SF 2 "register_operand" "Yr,*x,x")
	  (match_operand:V2SF 1 "register_operand" "0,0,x")
	  (match_operand:SI 3 "const_0_to_3_operand")))]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE"
  "@
   blendps\t{%3, %2, %0|%0, %2, %3}
   blendps\t{%3, %2, %0|%0, %2, %3}
   vblendps\t{%3, %2, %1, %0|%0, %1, %2, %3}"
  [(set_attr "isa" "noavx,noavx,avx")
   (set_attr "type" "ssemov")
   (set_attr "length_immediate" "1")
   (set_attr "prefix_data16" "1,1,*")
   (set_attr "prefix_extra" "1")
   (set_attr "prefix" "orig,orig,vex")
   (set_attr "mode" "V4SF")])

(define_insn "mmx_blendvps"
  [(set (match_operand:V2SF 0 "register_operand" "=Yr,*x,x")
	(unspec:V2SF
	  [(match_operand:V2SF 1 "register_operand" "0,0,x")
	   (match_operand:V2SF 2 "register_operand" "Yr,*x,x")
	   (match_operand:V2SF 3 "register_operand" "Yz,Yz,x")]
	  UNSPEC_BLENDV))]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE"
  "@
   blendvps\t{%3, %2, %0|%0, %2, %3}
   blendvps\t{%3, %2, %0|%0, %2, %3}
   vblendvps\t{%3, %2, %1, %0|%0, %1, %2, %3}"
  [(set_attr "isa" "noavx,noavx,avx")
   (set_attr "type" "ssemov")
   (set_attr "length_immediate" "1")
   (set_attr "prefix_data16" "1,1,*")
   (set_attr "prefix_extra" "1")
   (set_attr "prefix" "orig,orig,vex")
   (set_attr "btver2_decode" "vector")
   (set_attr "mode" "V4SF")])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel single-precision floating point logical operations
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_insn "*mmx_andnotv2sf3"
  [(set (match_operand:V2SF 0 "register_operand" "=x,x")
	(and:V2SF
	  (not:V2SF
	    (match_operand:V2SF 1 "register_operand" "0,x"))
	  (match_operand:V2SF 2 "register_operand" "x,x")))]
  "TARGET_MMX_WITH_SSE"
  "@
   andnps\t{%2, %0|%0, %2}
   vandnps\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sselog")
   (set_attr "prefix" "orig,vex")
   (set_attr "mode" "V4SF")])

(define_insn "<code>v2sf3"
  [(set (match_operand:V2SF 0 "register_operand" "=x,x")
	(any_logic:V2SF
	  (match_operand:V2SF 1 "register_operand" "%0,x")
	  (match_operand:V2SF 2 "register_operand" "x,x")))]
  "TARGET_MMX_WITH_SSE"
  "@
   <logic>ps\t{%2, %0|%0, %2}
   v<logic>ps\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sselog")
   (set_attr "prefix" "orig,vex")
   (set_attr "mode" "V4SF")])

(define_expand "copysignv2sf3"
  [(set (match_dup 4)
	(and:V2SF
	  (not:V2SF (match_dup 3))
	  (match_operand:V2SF 1 "register_operand")))
   (set (match_dup 5)
	(and:V2SF (match_dup 3)
		  (match_operand:V2SF 2 "register_operand")))
   (set (match_operand:V2SF 0 "register_operand")
	(ior:V2SF (match_dup 4) (match_dup 5)))]
  "TARGET_MMX_WITH_SSE"
{
  operands[3] = ix86_build_signbit_mask (V2SFmode, true, false);

  operands[4] = gen_reg_rtx (V2SFmode);
  operands[5] = gen_reg_rtx (V2SFmode);
})

(define_expand "xorsignv2sf3"
  [(set (match_dup 4)
	(and:V2SF (match_dup 3)
		  (match_operand:V2SF 2 "register_operand")))
   (set (match_operand:V2SF 0 "register_operand")
	(xor:V2SF (match_dup 4)
		  (match_operand:V2SF 1 "register_operand")))]
  "TARGET_MMX_WITH_SSE"
{
  operands[3] = ix86_build_signbit_mask (V2SFmode, true, false);

  operands[4] = gen_reg_rtx (V2SFmode);
})

(define_expand "signbitv2sf2"
  [(set (match_operand:V2SI 0 "register_operand")
	(lshiftrt:V2SI
	  (subreg:V2SI
	    (match_operand:V2SF 1 "register_operand") 0)
	  (match_dup 2)))]
  "TARGET_MMX_WITH_SSE"
{
  operands[1] = force_reg (V2SFmode, operands[1]);
  operands[2] = GEN_INT (GET_MODE_UNIT_BITSIZE (V2SFmode)-1);
})

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel single-precision FMA multiply/accumulate instructions.
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_expand "fmav2sf4"
  [(set (match_operand:V2SF 0 "register_operand")
	(fma:V2SF
	  (match_operand:V2SF 1 "nonimmediate_operand")
	  (match_operand:V2SF 2 "nonimmediate_operand")
	  (match_operand:V2SF 3 "nonimmediate_operand")))]
  "(TARGET_FMA || TARGET_FMA4 || TARGET_AVX512VL)
   && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op3 = gen_reg_rtx (V4SFmode);
  rtx op2 = gen_reg_rtx (V4SFmode);
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2sf_to_sse (op3, operands[3]));
  emit_insn (gen_movq_v2sf_to_sse (op2, operands[2]));
  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_fmav4sf4 (op0, op1, op2, op3));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

(define_expand "fmsv2sf4"
  [(set (match_operand:V2SF 0 "register_operand")
	(fma:V2SF
	  (match_operand:V2SF   1 "nonimmediate_operand")
	  (match_operand:V2SF   2 "nonimmediate_operand")
	  (neg:V2SF
	    (match_operand:V2SF 3 "nonimmediate_operand"))))]
  "(TARGET_FMA || TARGET_FMA4 || TARGET_AVX512VL)
   && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op3 = gen_reg_rtx (V4SFmode);
  rtx op2 = gen_reg_rtx (V4SFmode);
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2sf_to_sse (op3, operands[3]));
  emit_insn (gen_movq_v2sf_to_sse (op2, operands[2]));
  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_fmsv4sf4 (op0, op1, op2, op3));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

(define_expand "fnmav2sf4"
  [(set (match_operand:V2SF 0 "register_operand")
	(fma:V2SF
	  (neg:V2SF
	    (match_operand:V2SF 1 "nonimmediate_operand"))
	  (match_operand:V2SF   2 "nonimmediate_operand")
	  (match_operand:V2SF   3 "nonimmediate_operand")))]
  "(TARGET_FMA || TARGET_FMA4 || TARGET_AVX512VL)
   && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op3 = gen_reg_rtx (V4SFmode);
  rtx op2 = gen_reg_rtx (V4SFmode);
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2sf_to_sse (op3, operands[3]));
  emit_insn (gen_movq_v2sf_to_sse (op2, operands[2]));
  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_fnmav4sf4 (op0, op1, op2, op3));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

(define_expand "fnmsv2sf4"
  [(set (match_operand:V2SF 0 "register_operand" "=v,v,x")
	(fma:V2SF
	  (neg:V2SF
	    (match_operand:V2SF 1 "nonimmediate_operand"))
	  (match_operand:V2SF   2 "nonimmediate_operand")
	  (neg:V2SF
	    (match_operand:V2SF 3 "nonimmediate_operand"))))]
  "(TARGET_FMA || TARGET_FMA4 || TARGET_AVX512VL)
   && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op3 = gen_reg_rtx (V4SFmode);
  rtx op2 = gen_reg_rtx (V4SFmode);
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2sf_to_sse (op3, operands[3]));
  emit_insn (gen_movq_v2sf_to_sse (op2, operands[2]));
  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_fnmsv4sf4 (op0, op1, op2, op3));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel single-precision floating point conversion operations
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_expand "fix_truncv2sfv2si2"
  [(set (match_operand:V2SI 0 "register_operand")
	(fix:V2SI (match_operand:V2SF 1 "nonimmediate_operand")))]
  "TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SImode);

  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_fix_truncv4sfv4si2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2SImode, op0, V4SImode));
  DONE;
})

(define_expand "fixuns_truncv2sfv2si2"
  [(set (match_operand:V2SI 0 "register_operand")
	(unsigned_fix:V2SI (match_operand:V2SF 1 "nonimmediate_operand")))]
  "TARGET_AVX512VL && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SImode);

  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_fixuns_truncv4sfv4si2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2SImode, op0, V4SImode));
  DONE;
})

(define_insn "mmx_fix_truncv2sfv2si2"
  [(set (match_operand:V2SI 0 "register_operand" "=y")
	(fix:V2SI (match_operand:V2SF 1 "nonimmediate_operand" "ym")))
   (unspec [(const_int 0)] UNSPEC_3DNOW)]
  "TARGET_3DNOW"
  "pf2id\t{%1, %0|%0, %1}"
  [(set_attr "type" "mmxcvt")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

(define_expand "floatv2siv2sf2"
  [(set (match_operand:V2SF 0 "register_operand")
	(float:V2SF (match_operand:V2SI 1 "nonimmediate_operand")))]
  "TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V4SImode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2si_to_sse (op1, operands[1]));

  emit_insn (gen_floatv4siv4sf2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

(define_expand "floatunsv2siv2sf2"
  [(set (match_operand:V2SF 0 "register_operand")
	(unsigned_float:V2SF (match_operand:V2SI 1 "nonimmediate_operand")))]
  "TARGET_AVX512VL && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V4SImode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2si_to_sse (op1, operands[1]));

  emit_insn (gen_floatunsv4siv4sf2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

(define_insn "mmx_floatv2siv2sf2"
  [(set (match_operand:V2SF 0 "register_operand" "=y")
	(float:V2SF (match_operand:V2SI 1 "nonimmediate_operand" "ym")))
   (unspec [(const_int 0)] UNSPEC_3DNOW)]
  "TARGET_3DNOW"
  "pi2fd\t{%1, %0|%0, %1}"
  [(set_attr "type" "mmxcvt")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

(define_insn "mmx_pf2iw"
  [(set (match_operand:V2SI 0 "register_operand" "=y")
	(sign_extend:V2SI
	  (ss_truncate:V2HI
	    (fix:V2SI
	      (match_operand:V2SF 1 "nonimmediate_operand" "ym")))))]
  "TARGET_3DNOW_A"
  "pf2iw\t{%1, %0|%0, %1}"
  [(set_attr "type" "mmxcvt")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

(define_insn "mmx_pi2fw"
  [(set (match_operand:V2SF 0 "register_operand" "=y")
	(float:V2SF
	  (sign_extend:V2SI
	    (truncate:V2HI
	      (match_operand:V2SI 1 "nonimmediate_operand" "ym")))))]
  "TARGET_3DNOW_A"
  "pi2fw\t{%1, %0|%0, %1}"
  [(set_attr "type" "mmxcvt")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "V2SF")])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel single-precision floating point element swizzling
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_insn "mmx_pswapdv2sf2"
  [(set (match_operand:V2SF 0 "register_operand" "=y,x,Yv")
	(vec_select:V2SF
	  (match_operand:V2SF 1 "register_mmxmem_operand" "ym,0,Yv")
	  (parallel [(const_int 1) (const_int 0)])))]
  "TARGET_3DNOW_A || TARGET_MMX_WITH_SSE"
  "@
   pswapd\t{%1, %0|%0, %1}
   shufps\t{$0xe1, %1, %0|%0, %1, 0xe1}
   vshufps\t{$0xe1, %1, %1, %0|%0, %1, %1, 0xe1}"
  [(set_attr "isa" "*,sse_noavx,avx")
   (set_attr "mmx_isa" "native,*,*")
   (set_attr "type" "mmxcvt,ssemov,ssemov")
   (set_attr "prefix_extra" "1,*,*")
   (set_attr "mode" "V2SF,V4SF,V4SF")])

(define_insn "*mmx_movshdup"
  [(set (match_operand:V2SF 0 "register_operand" "=v,x")
	(vec_select:V2SF
	  (match_operand:V2SF 1 "register_operand" "v,0")
	  (parallel [(const_int 1) (const_int 1)])))]
  "TARGET_MMX_WITH_SSE"
  "@
   %vmovshdup\t{%1, %0|%0, %1}
   shufps\t{$0xe5, %0, %0|%0, %0, 0xe5}"
  [(set_attr "isa" "sse3,*")
   (set_attr "type" "sse,sseshuf1")
   (set_attr "length_immediate" "*,1")
   (set_attr "prefix_rep" "1,*")
   (set_attr "prefix" "maybe_vex,orig")
   (set_attr "mode" "V4SF")])

(define_insn "*mmx_movsldup"
  [(set (match_operand:V2SF 0 "register_operand" "=v,x")
	(vec_select:V2SF
	  (match_operand:V2SF 1 "register_operand" "v,0")
	  (parallel [(const_int 0) (const_int 0)])))]
  "TARGET_MMX_WITH_SSE"
  "@
   %vmovsldup\t{%1, %0|%0, %1}
   shufps\t{$0xe0, %0, %0|%0, %0, 0xe0}"
  [(set_attr "isa" "sse3,*")
   (set_attr "type" "sse,sseshuf1")
   (set_attr "length_immediate" "*,1")
   (set_attr "prefix_rep" "1,*")
   (set_attr "prefix" "maybe_vex,orig")
   (set_attr "mode" "V4SF")])

(define_insn_and_split "*vec_interleave_lowv2sf"
  [(set (match_operand:V2SF 0 "register_operand" "=x,v")
	(vec_select:V2SF
	  (vec_concat:V4SF
	    (match_operand:V2SF 1 "register_operand" "0,v")
	    (match_operand:V2SF 2 "register_operand" "x,v"))
	  (parallel [(const_int 0) (const_int 2)])))]
  "TARGET_MMX_WITH_SSE"
  "#"
  "&& reload_completed"
  [(const_int 0)]
  "ix86_split_mmx_punpck (operands, false); DONE;"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sselog")
   (set_attr "prefix" "orig,maybe_evex")
   (set_attr "mode" "V4SF")])

(define_insn_and_split "*vec_interleave_highv2sf"
  [(set (match_operand:V2SF 0 "register_operand" "=x,v")
	(vec_select:V2SF
	  (vec_concat:V4SF
	    (match_operand:V2SF 1 "register_operand" "0,v")
	    (match_operand:V2SF 2 "register_operand" "x,v"))
	  (parallel [(const_int 1) (const_int 3)])))]
  "TARGET_MMX_WITH_SSE"
  "#"
  "&& reload_completed"
  [(const_int 0)]
  "ix86_split_mmx_punpck (operands, true); DONE;"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sselog")
   (set_attr "prefix" "orig,vex")
   (set_attr "mode" "V4SF")])

(define_insn "*vec_dupv2sf"
  [(set (match_operand:V2SF 0 "register_operand" "=y,Yv,x")
	(vec_duplicate:V2SF
	  (match_operand:SF 1 "register_operand" "0,Yv,0")))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "@
   punpckldq\t%0, %0
   %vmovsldup\t{%1, %0|%0, %1}
   shufps\t{$0xe0, %0, %0|%0, %0, 0xe0}"
  [(set_attr "isa" "*,sse3,sse_noavx")
   (set_attr "mmx_isa" "native,*,*")
   (set_attr "type" "mmxcvt,sse,sseshuf1")
   (set_attr "length_immediate" "*,*,1")
   (set_attr "prefix_rep" "*,1,*")
   (set_attr "prefix" "*,maybe_vex,orig")
   (set_attr "mode" "DI,V4SF,V4SF")])

(define_insn "*mmx_movss_<mode>"
  [(set (match_operand:V2FI 0 "register_operand"   "=x,v")
	(vec_merge:V2FI
	  (match_operand:V2FI 2 "register_operand" " x,v")
	  (match_operand:V2FI 1 "register_operand" " 0,v")
	  (const_int 1)))]
  "TARGET_MMX_WITH_SSE"
  "@
   movss\t{%2, %0|%0, %2}
   vmovss\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "ssemov")
   (set_attr "prefix" "orig,maybe_evex")
   (set_attr "mode" "SF")])

(define_insn "*mmx_concatv2sf"
  [(set (match_operand:V2SF 0 "register_operand"     "=y,y")
	(vec_concat:V2SF
	  (match_operand:SF 1 "nonimmediate_operand" " 0,rm")
	  (match_operand:SF 2 "nonimm_or_0_operand"  "ym,C")))]
  "TARGET_MMX && !TARGET_SSE"
  "@
   punpckldq\t{%2, %0|%0, %2}
   movd\t{%1, %0|%0, %1}"
  [(set_attr "type" "mmxcvt,mmxmov")
   (set_attr "mode" "DI")])

(define_expand "vec_setv2sf"
  [(match_operand:V2SF 0 "register_operand")
   (match_operand:SF 1 "register_operand")
   (match_operand 2 "vec_setm_mmx_operand")]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
{
  if (CONST_INT_P (operands[2]))
    ix86_expand_vector_set (TARGET_MMX_WITH_SSE, operands[0], operands[1],
			    INTVAL (operands[2]));
  else
    ix86_expand_vector_set_var (operands[0], operands[1], operands[2]);
  DONE;
})

;; Avoid combining registers from different units in a single alternative,
;; see comment above inline_secondary_memory_needed function in i386.cc
(define_insn_and_split "*vec_extractv2sf_0"
  [(set (match_operand:SF 0 "nonimmediate_operand"     "=x, m,y ,m,f,r")
	(vec_select:SF
	  (match_operand:V2SF 1 "nonimmediate_operand" " xm,x,ym,y,m,m")
	  (parallel [(const_int 0)])))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && !(MEM_P (operands[0]) && MEM_P (operands[1]))"
  "#"
  "&& reload_completed"
  [(set (match_dup 0) (match_dup 1))]
  "operands[1] = gen_lowpart (SFmode, operands[1]);"
  [(set_attr "mmx_isa" "*,*,native,native,*,*")])

;; Avoid combining registers from different units in a single alternative,
;; see comment above inline_secondary_memory_needed function in i386.cc
(define_insn "*vec_extractv2sf_1"
  [(set (match_operand:SF 0 "nonimmediate_operand"     "=y,x,x,y,x,f,r")
	(vec_select:SF
	  (match_operand:V2SF 1 "nonimmediate_operand" " 0,x,0,o,o,o,o")
	  (parallel [(const_int 1)])))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && !(MEM_P (operands[0]) && MEM_P (operands[1]))"
  "@
   punpckhdq\t%0, %0
   %vmovshdup\t{%1, %0|%0, %1}
   shufps\t{$0xe5, %0, %0|%0, %0, 0xe5}
   #
   #
   #
   #"
  [(set_attr "isa" "*,sse3,noavx,*,*,*,*")
   (set_attr "mmx_isa" "native,*,*,native,*,*,*")
   (set_attr "type" "mmxcvt,sse,sseshuf1,mmxmov,ssemov,fmov,imov")
   (set (attr "length_immediate")
     (if_then_else (eq_attr "alternative" "2")
		   (const_string "1")
		   (const_string "*")))
   (set (attr "prefix_rep")
     (if_then_else (eq_attr "alternative" "1")
		   (const_string "1")
		   (const_string "*")))
   (set_attr "prefix" "orig,maybe_vex,orig,orig,orig,orig,orig")
   (set_attr "mode" "DI,V4SF,V4SF,SF,SF,SF,SF")])

(define_split
  [(set (match_operand:SF 0 "register_operand")
	(vec_select:SF
	  (match_operand:V2SF 1 "memory_operand")
	  (parallel [(const_int 1)])))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE) && reload_completed"
  [(set (match_dup 0) (match_dup 1))]
  "operands[1] = adjust_address (operands[1], SFmode, 4);")

(define_expand "vec_extractv2sfsf"
  [(match_operand:SF 0 "register_operand")
   (match_operand:V2SF 1 "register_operand")
   (match_operand 2 "const_int_operand")]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
{
  ix86_expand_vector_extract (TARGET_MMX_WITH_SSE, operands[0],
			      operands[1], INTVAL (operands[2]));
  DONE;
})

(define_expand "vec_initv2sfsf"
  [(match_operand:V2SF 0 "register_operand")
   (match_operand 1)]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE) && TARGET_SSE"
{
  ix86_expand_vector_init (TARGET_MMX_WITH_SSE, operands[0],
			   operands[1]);
  DONE;
})

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel single-precision floating point rounding operations.
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_expand "nearbyintv2sf2"
  [(match_operand:V2SF 0 "register_operand")
   (match_operand:V2SF 1 "nonimmediate_operand")]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_nearbyintv4sf2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

(define_expand "rintv2sf2"
  [(match_operand:V2SF 0 "register_operand")
   (match_operand:V2SF 1 "nonimmediate_operand")]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_rintv4sf2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

(define_expand "lrintv2sfv2si2"
  [(match_operand:V2SI 0 "register_operand")
   (match_operand:V2SF 1 "nonimmediate_operand")]
  "TARGET_SSE4_1 && !flag_trapping_math
   && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SImode);

  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_lrintv4sfv4si2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2SImode, op0, V4SImode));
  DONE;
})

(define_expand "ceilv2sf2"
  [(match_operand:V2SF 0 "register_operand")
   (match_operand:V2SF 1 "nonimmediate_operand")]
  "TARGET_SSE4_1 && !flag_trapping_math
   && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_ceilv4sf2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

(define_expand "lceilv2sfv2si2"
  [(match_operand:V2SI 0 "register_operand")
   (match_operand:V2SF 1 "nonimmediate_operand")]
  "TARGET_SSE4_1 && !flag_trapping_math
   && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SImode);

  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_lceilv4sfv4si2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2SImode, op0, V4SImode));
  DONE;
})

(define_expand "floorv2sf2"
  [(match_operand:V2SF 0 "register_operand")
   (match_operand:V2SF 1 "nonimmediate_operand")]
  "TARGET_SSE4_1 && !flag_trapping_math
  && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_floorv4sf2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

(define_expand "lfloorv2sfv2si2"
  [(match_operand:V2SI 0 "register_operand")
   (match_operand:V2SF 1 "nonimmediate_operand")]
  "TARGET_SSE4_1 && !flag_trapping_math
   && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SImode);

  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_lfloorv4sfv4si2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2SImode, op0, V4SImode));
  DONE;
})

(define_expand "btruncv2sf2"
  [(match_operand:V2SF 0 "register_operand")
   (match_operand:V2SF 1 "nonimmediate_operand")]
  "TARGET_SSE4_1 && !flag_trapping_math
  && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_btruncv4sf2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

(define_expand "roundv2sf2"
  [(match_operand:V2SF 0 "register_operand")
   (match_operand:V2SF 1 "nonimmediate_operand")]
  "TARGET_SSE4_1 && !flag_trapping_math
   && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_roundv4sf2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

(define_expand "lroundv2sfv2si2"
  [(match_operand:V2SI 0 "register_operand")
   (match_operand:V2SF 1 "nonimmediate_operand")]
  "TARGET_SSE4_1 && !flag_trapping_math
   && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4SImode);

  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_lroundv4sfv4si2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2SImode, op0, V4SImode));
  DONE;
})

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel half-precision floating point arithmetic
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_mode_iterator VHF_32_64 [V2HF (V4HF "TARGET_MMX_WITH_SSE")])

(define_mode_iterator VBF_32_64 [V2BF (V4BF "TARGET_MMX_WITH_SSE")])

(define_expand "divv4hf3"
  [(set (match_operand:V4HF 0 "register_operand")
	(div:V4HF
	  (match_operand:V4HF 1 "nonimmediate_operand")
	  (match_operand:V4HF 2 "register_operand")))]
  "TARGET_AVX512FP16 && TARGET_AVX512VL && ix86_partial_vec_fp_math"
{
  rtx op2 = gen_reg_rtx (V8HFmode);
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_movq_v4hf_to_sse (op1, operands[1]));
  rtx tmp = gen_rtx_VEC_CONCAT (V8HFmode, operands[2],
				force_reg (V4HFmode, CONST1_RTX (V4HFmode)));
  emit_insn (gen_rtx_SET (op2, tmp));
  emit_insn (gen_divv8hf3 (op0, op1, op2));
  emit_move_insn (operands[0], lowpart_subreg (V4HFmode, op0, V8HFmode));
  DONE;
})

(define_mode_attr mov_to_sse_suffix
  [(V2HF "d") (V4HF "q") (V2HI "d") (V4HI "q")])

(define_mode_attr mmxxmmmode
  [(V2HF "V8HF") (V2HI "V8HI") (V2BF "V8BF")
   (V4HF "V8HF") (V4HI "V8HI") (V4BF "V8BF")])

(define_mode_attr mmxxmmmodelower
  [(V2HF "v8hf") (V2HI "v8hi") (V2BF "v8bf")
   (V4HF "v8hf") (V4HI "v8hi") (V4BF "v8bf")])

(define_expand "movd_<mode>_to_sse"
  [(set (match_operand:<mmxxmmmode> 0 "register_operand")
	(vec_merge:<mmxxmmmode>
	  (vec_duplicate:<mmxxmmmode>
	    (match_operand:V2FI_32 1 "nonimmediate_operand"))
	  (match_dup 2)
	  (const_int 3)))]
  "TARGET_SSE"
{
  if (!flag_trapping_math)
  {
    rtx op1 = force_reg (<MODE>mode, operands[1]);
    emit_move_insn (operands[0],
		   lowpart_subreg (<mmxxmmmode>mode, op1, <MODE>mode));
    DONE;
  }
  operands[2] = CONST0_RTX (<mmxxmmmode>mode);
})

(define_expand "movd_<mode>_to_sse_reg"
  [(set (match_operand:<mmxxmmmode> 0 "register_operand")
	(vec_merge:<mmxxmmmode>
	  (vec_duplicate:<mmxxmmmode>
	    (match_operand:V2FI_32 1 "nonimmediate_operand"))
	  (match_operand:<mmxxmmmode> 2 "register_operand")
	  (const_int 3)))]
  "TARGET_SSE")

(define_expand "<insn><mode>3"
  [(set (match_operand:VHF_32_64 0 "register_operand")
	(plusminusmult:VHF_32_64
	  (match_operand:VHF_32_64 1 "nonimmediate_operand")
	  (match_operand:VHF_32_64 2 "nonimmediate_operand")))]
  "TARGET_AVX512FP16 && TARGET_AVX512VL && ix86_partial_vec_fp_math"
{
  rtx op2 = gen_reg_rtx (V8HFmode);
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op2, operands[2]));
  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op1, operands[1]));
  emit_insn (gen_<insn>v8hf3 (op0, op1, op2));

  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8HFmode));
  DONE;
})

;; VDIVNEPBF16 does not generate floating point exceptions.
(define_expand "<insn><mode>3"
  [(set (match_operand:VBF_32_64 0 "register_operand")
    (plusminusmultdiv:VBF_32_64
      (match_operand:VBF_32_64 1 "nonimmediate_operand")
      (match_operand:VBF_32_64 2 "nonimmediate_operand")))]
  "TARGET_AVX10_2"
{
  rtx op0 = gen_reg_rtx (V8BFmode);
  rtx op1 = lowpart_subreg (V8BFmode,
			    force_reg (<MODE>mode, operands[1]), <MODE>mode);
  rtx op2 = lowpart_subreg (V8BFmode,
			    force_reg (<MODE>mode, operands[2]), <MODE>mode);

  emit_insn (gen_<insn>v8bf3 (op0, op1, op2));

  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8BFmode));
  DONE;
})

(define_expand "divv2hf3"
  [(set (match_operand:V2HF 0 "register_operand")
	(div:V2HF
	  (match_operand:V2HF 1 "nonimmediate_operand")
	  (match_operand:V2HF 2 "nonimmediate_operand")))]
  "TARGET_AVX512FP16 && TARGET_AVX512VL && ix86_partial_vec_fp_math"
{
  rtx op2 = gen_reg_rtx (V8HFmode);
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_movd_v2hf_to_sse_reg (op2, operands[2],
				  force_reg (V8HFmode, CONST1_RTX (V8HFmode))));
  emit_insn (gen_movd_v2hf_to_sse (op1, operands[1]));
  emit_insn (gen_divv8hf3 (op0, op1, op2));

  emit_move_insn (operands[0], lowpart_subreg (V2HFmode, op0, V8HFmode));
  DONE;
})

(define_expand "<code><mode>3"
  [(set (match_operand:VHF_32_64 0 "register_operand")
	(smaxmin:VHF_32_64
	  (match_operand:VHF_32_64 1 "nonimmediate_operand")
	  (match_operand:VHF_32_64 2 "nonimmediate_operand")))]
  "TARGET_AVX512FP16 && TARGET_AVX512VL && ix86_partial_vec_fp_math"
{
  rtx op2 = gen_reg_rtx (V8HFmode);
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op2, operands[2]));
  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op1, operands[1]));

  emit_insn (gen_<code>v8hf3 (op0, op1, op2));

  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8HFmode));
  DONE;
})

(define_expand "<code><mode>3"
  [(set (match_operand:VBF_32_64 0 "register_operand")
    (smaxmin:VBF_32_64
      (match_operand:VBF_32_64 1 "nonimmediate_operand")
      (match_operand:VBF_32_64 2 "nonimmediate_operand")))]
  "TARGET_AVX10_2"
{
  rtx op0 = gen_reg_rtx (V8BFmode);
  rtx op1 = lowpart_subreg (V8BFmode,
			    force_reg (<MODE>mode, operands[1]), <MODE>mode);
  rtx op2 = lowpart_subreg (V8BFmode,
			    force_reg (<MODE>mode, operands[2]), <MODE>mode);

  emit_insn (gen_<code>v8bf3 (op0, op1, op2));

  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8BFmode));
  DONE;
})

(define_expand "sqrt<mode>2"
  [(set (match_operand:VHF_32_64 0 "register_operand")
	(sqrt:VHF_32_64
	  (match_operand:VHF_32_64 1 "nonimmediate_operand")))]
  "TARGET_AVX512FP16 && TARGET_AVX512VL && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op1, operands[1]));
  emit_insn (gen_sqrtv8hf2 (op0, op1));
  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8HFmode));
  DONE;
})

(define_expand "sqrt<mode>2"
  [(set (match_operand:VBF_32_64 0 "register_operand")
	(sqrt:VBF_32_64 (match_operand:VBF_32_64 1 "vector_operand")))]
  "TARGET_AVX10_2"
{
  rtx op0 = gen_reg_rtx (V8BFmode);
  rtx op1 = lowpart_subreg (V8BFmode,
			    force_reg (<MODE>mode, operands[1]), <MODE>mode);

  emit_insn (gen_sqrtv8bf2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8BFmode));
  DONE;
})

(define_mode_iterator VHBF_32_64
 [V2BF (V4BF "TARGET_MMX_WITH_SSE")
  V2HF (V4HF "TARGET_MMX_WITH_SSE")]) 

(define_expand "<code><mode>2"
  [(set (match_operand:VHBF_32_64 0 "register_operand")
	(absneg:VHBF_32_64
	  (match_operand:VHBF_32_64 1 "register_operand")))]
  "TARGET_SSE"
  "ix86_expand_fp_absneg_operator (<CODE>, <MODE>mode, operands); DONE;")

(define_insn_and_split "*mmx_<code><mode>"
  [(set (match_operand:VHBF_32_64 0 "register_operand" "=x,x,x")
	(absneg:VHBF_32_64
	  (match_operand:VHBF_32_64 1 "register_operand" "0,x,x")))
   (use (match_operand:VHBF_32_64 2 "register_operand" "x,0,x"))]
  "TARGET_SSE"
  "#"
  "&& reload_completed"
  [(set (match_dup 0)
	(<absneg_op>:<MODE> (match_dup 1) (match_dup 2)))]
{
  if (!TARGET_AVX && operands_match_p (operands[0], operands[2]))
    std::swap (operands[1], operands[2]);
}
  [(set_attr "isa" "noavx,noavx,avx")])

(define_insn_and_split "*mmx_nabs<mode>2"
  [(set (match_operand:VHBF_32_64 0 "register_operand" "=x,x,x")
	(neg:VHBF_32_64
	  (abs:VHBF_32_64
	    (match_operand:VHBF_32_64 1 "register_operand" "0,x,x"))))
   (use (match_operand:VHBF_32_64 2 "register_operand" "x,0,x"))]
  "TARGET_SSE"
  "#"
  "&& reload_completed"
  [(set (match_dup 0)
	(ior:<MODE> (match_dup 1) (match_dup 2)))])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel half-precision floating point comparisons
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_expand "vec_cmpv4hfqi"
  [(set (match_operand:QI 0 "register_operand")
	(match_operator:QI 1 ""
	  [(match_operand:V4HF 2 "nonimmediate_operand")
	   (match_operand:V4HF 3 "nonimmediate_operand")]))]
  "TARGET_MMX_WITH_SSE && TARGET_AVX512FP16 && TARGET_AVX512VL
   && ix86_partial_vec_fp_math"
{
  rtx ops[4];
  ops[3] = gen_reg_rtx (V8HFmode);
  ops[2] = gen_reg_rtx (V8HFmode);

  emit_insn (gen_movq_v4hf_to_sse (ops[3], operands[3]));
  emit_insn (gen_movq_v4hf_to_sse (ops[2], operands[2]));
  emit_insn (gen_vec_cmpv8hfqi (operands[0], operands[1], ops[2], ops[3]));
  DONE;
})

(define_expand "vcond_mask_<mode>v4hi"
  [(set (match_operand:V4F_64 0 "register_operand")
	(vec_merge:V4F_64
	  (match_operand:V4F_64 1 "register_operand")
	  (match_operand:V4F_64 2 "register_operand")
	  (match_operand:V4HI 3  "register_operand")))]
  "TARGET_MMX_WITH_SSE && TARGET_SSE4_1"
{
  ix86_expand_sse_movcc (operands[0], operands[3],
			 operands[1], operands[2]);
  DONE;
})

(define_expand "vcond_mask_<mode>qi"
  [(set (match_operand:V4FI_64 0 "register_operand")
	(vec_merge:V4FI_64
	  (match_operand:V4FI_64 1 "register_operand")
	  (match_operand:V4FI_64 2 "register_operand")
	  (match_operand:QI 3 "register_operand")))]
  "TARGET_MMX_WITH_SSE && TARGET_AVX512BW && TARGET_AVX512VL"
{
  rtx op0 = gen_reg_rtx (<mmxxmmmode>mode);
  operands[1] = lowpart_subreg (<mmxxmmmode>mode, operands[1], <MODE>mode);
  operands[2] = lowpart_subreg (<mmxxmmmode>mode, operands[2], <MODE>mode);
  emit_insn (gen_vcond_mask_<mmxxmmmodelower>qi (op0, operands[1],
						 operands[2], operands[3]));
  emit_move_insn (operands[0],
		  lowpart_subreg (<MODE>mode, op0, <mmxxmmmode>mode));
  DONE;
})

(define_expand "vec_cmpv2hfqi"
  [(set (match_operand:QI 0 "register_operand")
	(match_operator:QI 1 ""
	  [(match_operand:V2HF 2 "nonimmediate_operand")
	   (match_operand:V2HF 3 "nonimmediate_operand")]))]
  "TARGET_AVX512FP16 && TARGET_AVX512VL
   && ix86_partial_vec_fp_math"
{
  rtx ops[4];
  ops[3] = gen_reg_rtx (V8HFmode);
  ops[2] = gen_reg_rtx (V8HFmode);

  emit_insn (gen_movd_v2hf_to_sse (ops[3], operands[3]));
  emit_insn (gen_movd_v2hf_to_sse (ops[2], operands[2]));
  emit_insn (gen_vec_cmpv8hfqi (operands[0], operands[1], ops[2], ops[3]));
  DONE;
})

(define_expand "vcond_mask_<mode>v2hi"
  [(set (match_operand:V2F_32 0 "register_operand")
	(vec_merge:V2F_32
	  (match_operand:V2F_32 1 "register_operand")
	  (match_operand:V2F_32 2 "register_operand")
	  (match_operand:V2HI 3 "register_operand")))]
  "TARGET_SSE4_1"
{
  ix86_expand_sse_movcc (operands[0], operands[3],
			 operands[1], operands[2]);
  DONE;
})

(define_expand "vcond_mask_<mode>qi"
  [(set (match_operand:V2FI_32 0 "register_operand")
	(vec_merge:V2FI_32
	  (match_operand:V2FI_32 1 "register_operand")
	  (match_operand:V2FI_32 2 "register_operand")
	  (match_operand:QI 3 "register_operand")))]
  "TARGET_AVX512BW && TARGET_AVX512VL"
{
  rtx op0 = gen_reg_rtx (<mmxxmmmode>mode);
  operands[1] = lowpart_subreg (<mmxxmmmode>mode, operands[1], <MODE>mode);
  operands[2] = lowpart_subreg (<mmxxmmmode>mode, operands[2], <MODE>mode);
  emit_insn (gen_vcond_mask_<mmxxmmmodelower>qi (op0, operands[1],
						 operands[2], operands[3]));
  emit_move_insn (operands[0],
		  lowpart_subreg (<MODE>mode, op0, <mmxxmmmode>mode));
  DONE;
})

;;This instruction does not generate floating point exceptions
(define_expand "vec_cmp<mode>qi"
  [(set (match_operand:QI 0 "register_operand")
	(match_operator:QI 1 ""
	  [(match_operand:VBF_32_64 2 "register_operand")
	   (match_operand:VBF_32_64 3 "nonimmediate_operand")]))]
  "TARGET_AVX10_2"
{
  rtx op2 = lowpart_subreg (V8BFmode,
			     force_reg (<MODE>mode, operands[2]), <MODE>mode);
  rtx op3 = lowpart_subreg (V8BFmode,
			     force_reg (<MODE>mode, operands[3]), <MODE>mode);

  emit_insn (gen_vec_cmpv8bfqi (operands[0], operands[1], op2, op3));
  DONE;
})

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel half-precision floating point rounding operations.
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_expand "btrunc<mode>2"
  [(match_operand:VHF_32_64 0 "register_operand")
   (match_operand:VHF_32_64 1 "nonimmediate_operand")]
  "TARGET_AVX512FP16 && TARGET_AVX512VL
   && ix86_partial_vec_fp_math
   && !flag_trapping_math"
{
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op1, operands[1]));
  emit_insn (gen_btruncv8hf2 (op0, op1));
  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8HFmode));

  DONE;
})

(define_expand "nearbyint<mode>2"
  [(match_operand:VHF_32_64 0 "register_operand")
   (match_operand:VHF_32_64 1 "nonimmediate_operand")]
  "TARGET_AVX512FP16 && TARGET_AVX512VL
   && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op1, operands[1]));
  emit_insn (gen_nearbyintv8hf2 (op0, op1));
  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8HFmode));

  DONE;
})

(define_expand "rint<mode>2"
  [(match_operand:VHF_32_64 0 "register_operand")
   (match_operand:VHF_32_64 1 "nonimmediate_operand")]
  "TARGET_AVX512FP16 && TARGET_AVX512VL
   && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op1, operands[1]));
  emit_insn (gen_rintv8hf2 (op0, op1));
  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8HFmode));

  DONE;
})

(define_expand "lrint<mode><mmxintvecmodelower>2"
  [(match_operand:<mmxintvecmode> 0 "register_operand")
   (match_operand:VHF_32_64 1 "nonimmediate_operand")]
  "TARGET_AVX512FP16 && TARGET_AVX512VL
   && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op1, operands[1]));
  emit_insn (gen_lrintv8hfv8hi2 (op0, op1));
  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8HFmode));

  DONE;
})

(define_expand "floor<mode>2"
  [(match_operand:VHF_32_64 0 "register_operand")
   (match_operand:VHF_32_64 1 "nonimmediate_operand")]
  "TARGET_AVX512FP16 && TARGET_AVX512VL
   && ix86_partial_vec_fp_math
   && !flag_trapping_math"
{
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op1, operands[1]));
  emit_insn (gen_floorv8hf2 (op0, op1));
  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8HFmode));

  DONE;
})

(define_expand "lfloor<mode><mmxintvecmodelower>2"
  [(match_operand:<mmxintvecmode> 0 "register_operand")
   (match_operand:VHF_32_64 1 "nonimmediate_operand")]
  "TARGET_AVX512FP16 && TARGET_AVX512VL
   && ix86_partial_vec_fp_math
   && !flag_trapping_math"
{
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op1, operands[1]));
  emit_insn (gen_lfloorv8hfv8hi2 (op0, op1));
  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8HFmode));

  DONE;
})

(define_expand "ceil<mode>2"
  [(match_operand:VHF_32_64 0 "register_operand")
   (match_operand:VHF_32_64 1 "nonimmediate_operand")]
  "TARGET_AVX512FP16 && TARGET_AVX512VL
   && ix86_partial_vec_fp_math
   && !flag_trapping_math"
{
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op1, operands[1]));
  emit_insn (gen_ceilv8hf2 (op0, op1));
  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8HFmode));

  DONE;
})

(define_expand "lceil<mode><mmxintvecmodelower>2"
  [(match_operand:<mmxintvecmode> 0 "register_operand")
   (match_operand:VHF_32_64 1 "nonimmediate_operand")]
  "TARGET_AVX512FP16 && TARGET_AVX512VL
   && ix86_partial_vec_fp_math
   && !flag_trapping_math"
{
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op1, operands[1]));
  emit_insn (gen_lceilv8hfv8hi2 (op0, op1));
  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8HFmode));

  DONE;
})

(define_expand "round<mode>2"
  [(match_operand:VHF_32_64 0 "register_operand")
   (match_operand:VHF_32_64 1 "nonimmediate_operand")]
  "TARGET_AVX512FP16 && TARGET_AVX512VL
   && ix86_partial_vec_fp_math
   && !flag_trapping_math"
{
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op1, operands[1]));
  emit_insn (gen_roundv8hf2 (op0, op1));
  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8HFmode));

  DONE;
})

(define_expand "lround<mode><mmxintvecmodelower>2"
  [(match_operand:<mmxintvecmode> 0 "register_operand")
   (match_operand:VHF_32_64 1 "nonimmediate_operand")]
  "TARGET_AVX512FP16 && TARGET_AVX512VL
   && ix86_partial_vec_fp_math
   && !flag_trapping_math"
{
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op1, operands[1]));
  emit_insn (gen_lroundv8hfv8hi2 (op0, op1));
  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8HFmode));

  DONE;
})

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel half-precision floating point logical operations
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_insn "*mmx_andnot<mode>3"
  [(set (match_operand:VHBF_32_64 0 "register_operand"    "=x,x")
	(and:VHBF_32_64
	  (not:VHBF_32_64
	    (match_operand:VHBF_32_64 1 "register_operand" "0,x"))
	  (match_operand:VHBF_32_64 2 "register_operand"   "x,x")))]
  "TARGET_SSE"
  "@
   andnps\t{%2, %0|%0, %2}
   vandnps\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sselog")
   (set_attr "prefix" "orig,vex")
   (set_attr "mode" "V4SF")])

(define_insn "<code><mode>3"
  [(set (match_operand:VHBF_32_64 0 "register_operand"   "=x,x")
	(any_logic:VHBF_32_64
	  (match_operand:VHBF_32_64 1 "register_operand" "%0,x")
	  (match_operand:VHBF_32_64 2 "register_operand" " x,x")))]
  "TARGET_SSE"
  "@
   <logic>ps\t{%2, %0|%0, %2}
   v<logic>ps\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sselog,sselog")
   (set_attr "prefix" "orig,vex")
   (set_attr "mode" "V4SF")])

(define_expand "copysign<mode>3"
  [(set (match_dup 4)
	(and:VHBF_32_64
	  (not:VHBF_32_64 (match_dup 3))
	  (match_operand:VHBF_32_64 1 "register_operand")))
   (set (match_dup 5)
	(and:VHBF_32_64 (match_dup 3)
		  (match_operand:VHBF_32_64 2 "register_operand")))
   (set (match_operand:VHBF_32_64 0 "register_operand")
	(ior:VHBF_32_64 (match_dup 4) (match_dup 5)))]
  "TARGET_SSE"
{
  operands[3] = ix86_build_signbit_mask (<MODE>mode, true, false);

  operands[4] = gen_reg_rtx (<MODE>mode);
  operands[5] = gen_reg_rtx (<MODE>mode);
})

(define_expand "xorsign<mode>3"
  [(set (match_dup 4)
	(and:VHBF_32_64 (match_dup 3)
		  (match_operand:VHBF_32_64 2 "register_operand")))
   (set (match_operand:VHBF_32_64 0 "register_operand")
	(xor:VHBF_32_64 (match_dup 4)
		  (match_operand:VHBF_32_64 1 "register_operand")))]
  "TARGET_SSE"
{
  operands[3] = ix86_build_signbit_mask (<MODE>mode, true, false);

  operands[4] = gen_reg_rtx (<MODE>mode);
})

(define_expand "signbit<mode>2"
  [(set (match_operand:<mmxintvecmode> 0 "register_operand")
	(lshiftrt:<mmxintvecmode>
	  (subreg:<mmxintvecmode>
	    (match_operand:VHBF_32_64 1 "register_operand") 0)
	  (match_dup 2)))]
  "TARGET_SSE2"
{
  operands[1] = force_reg (<MODE>mode, operands[1]);
  operands[2] = GEN_INT (GET_MODE_UNIT_BITSIZE (<MODE>mode)-1);
})

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel half-precision FMA multiply/accumulate instructions.
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_expand "fma<mode>4"
  [(set (match_operand:VHF_32_64 0 "register_operand")
	(fma:VHF_32_64
	  (match_operand:VHF_32_64 1 "nonimmediate_operand")
	  (match_operand:VHF_32_64 2 "nonimmediate_operand")
	  (match_operand:VHF_32_64 3 "nonimmediate_operand")))]
  "TARGET_AVX512FP16 && TARGET_AVX512VL && ix86_partial_vec_fp_math"
{
  rtx op3 = gen_reg_rtx (V8HFmode);
  rtx op2 = gen_reg_rtx (V8HFmode);
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op3, operands[3]));
  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op2, operands[2]));
  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op1, operands[1]));

  emit_insn (gen_fmav8hf4 (op0, op1, op2, op3));

  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8HFmode));
  DONE;
})

(define_expand "fms<mode>4"
  [(set (match_operand:VHF_32_64 0 "register_operand")
	(fma:VHF_32_64
	  (match_operand:VHF_32_64   1 "nonimmediate_operand")
	  (match_operand:VHF_32_64   2 "nonimmediate_operand")
	  (neg:VHF_32_64
	    (match_operand:VHF_32_64 3 "nonimmediate_operand"))))]
  "TARGET_AVX512FP16 && TARGET_AVX512VL && ix86_partial_vec_fp_math"
{
  rtx op3 = gen_reg_rtx (V8HFmode);
  rtx op2 = gen_reg_rtx (V8HFmode);
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op3, operands[3]));
  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op2, operands[2]));
  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op1, operands[1]));

  emit_insn (gen_fmsv8hf4 (op0, op1, op2, op3));

  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8HFmode));
  DONE;
})

(define_expand "fnma<mode>4"
  [(set (match_operand:VHF_32_64 0 "register_operand")
	(fma:VHF_32_64
	  (neg:VHF_32_64
	    (match_operand:VHF_32_64 1 "nonimmediate_operand"))
	  (match_operand:VHF_32_64   2 "nonimmediate_operand")
	  (match_operand:VHF_32_64   3 "nonimmediate_operand")))]
  "TARGET_AVX512FP16 && TARGET_AVX512VL && ix86_partial_vec_fp_math"
{
  rtx op3 = gen_reg_rtx (V8HFmode);
  rtx op2 = gen_reg_rtx (V8HFmode);
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op3, operands[3]));
  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op2, operands[2]));
  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op1, operands[1]));

  emit_insn (gen_fnmav8hf4 (op0, op1, op2, op3));

  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8HFmode));
  DONE;
})

(define_expand "fnms<mode>4"
  [(set (match_operand:VHF_32_64 0 "register_operand" "=v,v,x")
	(fma:VHF_32_64
	  (neg:VHF_32_64
	    (match_operand:VHF_32_64 1 "nonimmediate_operand"))
	  (match_operand:VHF_32_64   2 "nonimmediate_operand")
	  (neg:VHF_32_64
	    (match_operand:VHF_32_64 3 "nonimmediate_operand"))))]
  "TARGET_AVX512FP16 && TARGET_AVX512VL && ix86_partial_vec_fp_math"
{
  rtx op3 = gen_reg_rtx (V8HFmode);
  rtx op2 = gen_reg_rtx (V8HFmode);
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op3, operands[3]));
  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op2, operands[2]));
  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op1, operands[1]));

  emit_insn (gen_fnmsv8hf4 (op0, op1, op2, op3));

  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8HFmode));
  DONE;
})

(define_expand "vec_fmaddsubv4hf4"
  [(match_operand:V4HF 0 "register_operand")
   (match_operand:V4HF 1 "nonimmediate_operand")
   (match_operand:V4HF 2 "nonimmediate_operand")
   (match_operand:V4HF 3 "nonimmediate_operand")]
  "TARGET_AVX512FP16 && TARGET_AVX512VL
   && TARGET_MMX_WITH_SSE
   && ix86_partial_vec_fp_math"
{
  rtx op3 = gen_reg_rtx (V8HFmode);
  rtx op2 = gen_reg_rtx (V8HFmode);
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_movq_v4hf_to_sse (op3, operands[3]));
  emit_insn (gen_movq_v4hf_to_sse (op2, operands[2]));
  emit_insn (gen_movq_v4hf_to_sse (op1, operands[1]));

  emit_insn (gen_vec_fmaddsubv8hf4 (op0, op1, op2, op3));

  emit_move_insn (operands[0], lowpart_subreg (V4HFmode, op0, V8HFmode));
  DONE;
})

(define_expand "vec_fmsubaddv4hf4"
  [(match_operand:V4HF 0 "register_operand")
   (match_operand:V4HF 1 "nonimmediate_operand")
   (match_operand:V4HF 2 "nonimmediate_operand")
   (match_operand:V4HF 3 "nonimmediate_operand")]
  "TARGET_AVX512FP16 && TARGET_AVX512VL
   && ix86_partial_vec_fp_math
   && TARGET_MMX_WITH_SSE"
{
  rtx op3 = gen_reg_rtx (V8HFmode);
  rtx op2 = gen_reg_rtx (V8HFmode);
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_movq_v4hf_to_sse (op3, operands[3]));
  emit_insn (gen_movq_v4hf_to_sse (op2, operands[2]));
  emit_insn (gen_movq_v4hf_to_sse (op1, operands[1]));

  emit_insn (gen_vec_fmsubaddv8hf4 (op0, op1, op2, op3));

  emit_move_insn (operands[0], lowpart_subreg (V4HFmode, op0, V8HFmode));
  DONE;
})

(define_expand "fma<mode>4"
  [(set (match_operand:VBF_32_64 0 "register_operand")
	(fma:VBF_32_64
	  (match_operand:VBF_32_64 1 "nonimmediate_operand")
	  (match_operand:VBF_32_64 2 "nonimmediate_operand")
	  (match_operand:VBF_32_64 3 "nonimmediate_operand")))]
  "TARGET_AVX10_2"
{
  rtx op0 = gen_reg_rtx (V8BFmode);
  rtx op1 = lowpart_subreg (V8BFmode, force_reg (<MODE>mode, operands[1]), <MODE>mode);
  rtx op2 = lowpart_subreg (V8BFmode, force_reg (<MODE>mode, operands[2]), <MODE>mode);
  rtx op3 = lowpart_subreg (V8BFmode, force_reg (<MODE>mode, operands[3]), <MODE>mode);

  emit_insn (gen_fmav8bf4 (op0, op1, op2, op3));

  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8BFmode));
  DONE;
})

(define_expand "fms<mode>4"
  [(set (match_operand:VBF_32_64 0 "register_operand")
	(fma:VBF_32_64
	  (match_operand:VBF_32_64   1 "nonimmediate_operand")
	  (match_operand:VBF_32_64   2 "nonimmediate_operand")
	  (neg:VBF_32_64
	    (match_operand:VBF_32_64 3 "nonimmediate_operand"))))]
  "TARGET_AVX10_2"
{
  rtx op0 = gen_reg_rtx (V8BFmode);
  rtx op1 = lowpart_subreg (V8BFmode, force_reg (<MODE>mode, operands[1]), <MODE>mode);
  rtx op2 = lowpart_subreg (V8BFmode, force_reg (<MODE>mode, operands[2]), <MODE>mode);
  rtx op3 = lowpart_subreg (V8BFmode, force_reg (<MODE>mode, operands[3]), <MODE>mode);

  emit_insn (gen_fmsv8bf4 (op0, op1, op2, op3));

  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8BFmode));
  DONE;
})

(define_expand "fnma<mode>4"
  [(set (match_operand:VBF_32_64 0 "register_operand")
	(fma:VBF_32_64
	  (neg:VBF_32_64
	    (match_operand:VBF_32_64 1 "nonimmediate_operand"))
	  (match_operand:VBF_32_64   2 "nonimmediate_operand")
	  (match_operand:VBF_32_64   3 "nonimmediate_operand")))]
  "TARGET_AVX10_2"
{
  rtx op0 = gen_reg_rtx (V8BFmode);
  rtx op1 = lowpart_subreg (V8BFmode, force_reg (<MODE>mode, operands[1]), <MODE>mode);
  rtx op2 = lowpart_subreg (V8BFmode, force_reg (<MODE>mode, operands[2]), <MODE>mode);
  rtx op3 = lowpart_subreg (V8BFmode, force_reg (<MODE>mode, operands[3]), <MODE>mode);

  emit_insn (gen_fnmav8bf4 (op0, op1, op2, op3));

  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8BFmode));
  DONE;
})

(define_expand "fnms<mode>4"
  [(set (match_operand:VBF_32_64 0 "register_operand")
	(fma:VBF_32_64
	  (neg:VBF_32_64
	    (match_operand:VBF_32_64 1 "nonimmediate_operand"))
	  (match_operand:VBF_32_64   2 "nonimmediate_operand")
	  (neg:VBF_32_64
	    (match_operand:VBF_32_64 3 "nonimmediate_operand"))))]
  "TARGET_AVX10_2"
{
  rtx op0 = gen_reg_rtx (V8BFmode);
  rtx op1 = lowpart_subreg (V8BFmode, force_reg (<MODE>mode, operands[1]), <MODE>mode);
  rtx op2 = lowpart_subreg (V8BFmode, force_reg (<MODE>mode, operands[2]), <MODE>mode);
  rtx op3 = lowpart_subreg (V8BFmode, force_reg (<MODE>mode, operands[3]), <MODE>mode);

  emit_insn (gen_fnmsv8bf4 (op0, op1, op2, op3));

  emit_move_insn (operands[0], lowpart_subreg (<MODE>mode, op0, V8BFmode));
  DONE;
})

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel half-precision floating point complex type operations
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_expand "cmlav4hf4"
  [(match_operand:V4HF 0 "register_operand")
   (match_operand:V4HF 1 "vector_operand")
   (match_operand:V4HF 2 "vector_operand")
   (match_operand:V4HF 3 "vector_operand")]
  "TARGET_AVX512FP16 && TARGET_AVX512VL"
{
  rtx op3 = gen_reg_rtx (V8HFmode);
  rtx op2 = gen_reg_rtx (V8HFmode);
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_movq_v4hf_to_sse (op3, operands[3]));
  emit_insn (gen_movq_v4hf_to_sse (op2, operands[2]));
  emit_insn (gen_movq_v4hf_to_sse (op1, operands[1]));

  emit_insn (gen_cmlav8hf4 (op0, op1, op2, op3));

  emit_move_insn (operands[0], lowpart_subreg (V4HFmode, op0, V8HFmode));
  DONE;
})

(define_expand "cmla_conjv4hf4"
  [(match_operand:V4HF 0 "register_operand")
   (match_operand:V4HF 1 "vector_operand")
   (match_operand:V4HF 2 "vector_operand")
   (match_operand:V4HF 3 "vector_operand")]
  "TARGET_AVX512FP16 && TARGET_AVX512VL"
{
  rtx op3 = gen_reg_rtx (V8HFmode);
  rtx op2 = gen_reg_rtx (V8HFmode);
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_movq_v4hf_to_sse (op3, operands[3]));
  emit_insn (gen_movq_v4hf_to_sse (op2, operands[2]));
  emit_insn (gen_movq_v4hf_to_sse (op1, operands[1]));

  emit_insn (gen_cmla_conjv8hf4 (op0, op1, op2, op3));

  emit_move_insn (operands[0], lowpart_subreg (V4HFmode, op0, V8HFmode));
  DONE;
})

(define_expand "cmulv4hf3"
  [(match_operand:V4HF 0 "register_operand")
   (match_operand:V4HF 1 "vector_operand")
   (match_operand:V4HF 2 "vector_operand")]
  "TARGET_AVX512FP16 && TARGET_AVX512VL"
{
  rtx op2 = gen_reg_rtx (V8HFmode);
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_movq_v4hf_to_sse (op2, operands[2]));
  emit_insn (gen_movq_v4hf_to_sse (op1, operands[1]));

  emit_insn (gen_cmulv8hf3 (op0, op1, op2));
  emit_move_insn (operands[0], lowpart_subreg (V4HFmode, op0, V8HFmode));
  DONE;
})

(define_expand "cmul_conjv4hf3"
  [(match_operand:V4HF 0 "register_operand")
   (match_operand:V4HF 1 "vector_operand")
   (match_operand:V4HF 2 "vector_operand")]
  "TARGET_AVX512FP16 && TARGET_AVX512VL"
{
  rtx op2 = gen_reg_rtx (V8HFmode);
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_movq_v4hf_to_sse (op2, operands[2]));
  emit_insn (gen_movq_v4hf_to_sse (op1, operands[1]));

  emit_insn (gen_cmul_conjv8hf3 (op0, op1, op2));
  emit_move_insn (operands[0], lowpart_subreg (V4HFmode, op0, V8HFmode));
  DONE;
})

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel half-precision floating point conversion operations
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_expand "fix<fixunssuffix>_trunc<mode><mmxintvecmodelower>2"
  [(set (match_operand:<mmxintvecmode> 0 "register_operand")
	(any_fix:<mmxintvecmode>
	  (match_operand:VHF_32_64 1 "nonimmediate_operand")))]
  "TARGET_AVX512FP16 && TARGET_AVX512VL && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V8HImode);

  emit_insn (gen_mov<mov_to_sse_suffix>_<mode>_to_sse (op1, operands[1]));

  emit_insn (gen_fix<fixunssuffix>_truncv8hfv8hi2 (op0, op1));

  emit_move_insn (operands[0],
		 lowpart_subreg (<mmxintvecmode>mode, op0, V8HImode));
  DONE;
})

(define_expand "fix<fixunssuffix>_truncv2hfv2si2"
  [(set (match_operand:V2SI 0 "register_operand")
	(any_fix:V2SI
	  (match_operand:V2HF 1 "nonimmediate_operand")))]
  "TARGET_AVX512FP16 && TARGET_AVX512VL
   && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V4SImode);

  emit_insn (gen_movd_v2hf_to_sse (op1, operands[1]));

  emit_insn (gen_avx512fp16_fix<fixunssuffix>_truncv4si2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2SImode, op0, V4SImode));
  DONE;
})

(define_expand "float<floatunssuffix><mmxintvecmodelower><mode>2"
  [(set (match_operand:VHF_32_64 0 "register_operand")
	(any_float:VHF_32_64
	  (match_operand:<mmxintvecmode> 1 "nonimmediate_operand")))]
  "TARGET_AVX512FP16 && TARGET_AVX512VL && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V8HImode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  rtx (*gen_movd_sse) (rtx, rtx)
    = gen_mov<mov_to_sse_suffix>_<mmxintvecmodelower>_to_sse;
  emit_insn (gen_movd_sse (op1, operands[1]));

  emit_insn (gen_float<floatunssuffix>v8hiv8hf2 (op0, op1));

  emit_move_insn (operands[0],
		 lowpart_subreg (<MODE>mode, op0, V8HFmode));
  DONE;
})

(define_expand "float<floatunssuffix>v2siv2hf2"
  [(set (match_operand:V2HF 0 "register_operand")
	(any_float:V2HF
	  (match_operand:V2SI 1 "nonimmediate_operand")))]
  "TARGET_AVX512FP16 && TARGET_AVX512VL
   && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V4SImode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_movq_v2si_to_sse (op1, operands[1]));

  emit_insn (gen_avx512fp16_float<floatunssuffix>v4siv4hf2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2HFmode, op0, V8HFmode));
  DONE;
})

(define_expand "extendv2hfv2sf2"
  [(set (match_operand:V2SF 0 "register_operand")
	(float_extend:V2SF
	  (match_operand:V2HF 1 "nonimmediate_operand")))]
  "TARGET_AVX512FP16 && TARGET_AVX512VL
   && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V8HFmode);
  rtx op0 = gen_reg_rtx (V4SFmode);

  emit_insn (gen_movd_v2hf_to_sse (op1, operands[1]));

  emit_insn (gen_avx512fp16_float_extend_phv4sf2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

(define_expand "truncv2sfv2hf2"
  [(set (match_operand:V2HF 0 "register_operand")
	(float_truncate:V2HF
	  (match_operand:V2SF 1 "nonimmediate_operand")))]
  "TARGET_AVX512FP16 && TARGET_AVX512VL
   && TARGET_MMX_WITH_SSE && ix86_partial_vec_fp_math"
{
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V8HFmode);

  emit_insn (gen_movq_v2sf_to_sse (op1, operands[1]));

  emit_insn (gen_avx512fp16_truncv4sfv4hf2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2HFmode, op0, V8HFmode));
  DONE;
})

(define_expand "truncv2sfv2bf2"
  [(set (match_operand:V2BF 0 "register_operand")
	(float_truncate:V2BF
	  (match_operand:V2SF 1 "nonimmediate_operand")))]
  "TARGET_SSSE3 && TARGET_MMX_WITH_SSE
  && !HONOR_NANS (BFmode) && !flag_rounding_math
  && (flag_unsafe_math_optimizations
      || TARGET_AVXNECONVERT
      || (TARGET_AVX512BF16 && TARGET_AVX512VL))"
{
  rtx op1 = gen_reg_rtx (V4SFmode);
  rtx op0 = gen_reg_rtx (V4BFmode);

  emit_move_insn (op1, lowpart_subreg (V4SFmode,
				       force_reg (V2SFmode, operands[1]),
				       V2SFmode));
  emit_insn (gen_truncv4sfv4bf2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2BFmode, op0, V4BFmode));
  DONE;
})

(define_expand "extendv2bfv2sf2"
  [(set (match_operand:V2SF 0 "register_operand")
	(float_extend:V2SF
	  (match_operand:V2BF 1 "nonimmediate_operand")))]
  "TARGET_SSE2 && TARGET_MMX_WITH_SSE && !HONOR_NANS (BFmode)"
{
  rtx op0 = gen_reg_rtx (V4SFmode);
  rtx op1 = gen_reg_rtx (V4BFmode);

  emit_move_insn (op1, lowpart_subreg (V4BFmode,
				       force_reg (V2BFmode, operands[1]),
				       V2BFmode));
  emit_insn (gen_extendv4bfv4sf2 (op0, op1));

  emit_move_insn (operands[0], lowpart_subreg (V2SFmode, op0, V4SFmode));
  DONE;
})

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel integral arithmetic
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_expand "neg<mode>2"
  [(set (match_operand:MMXMODEI 0 "register_operand")
	(minus:MMXMODEI
	  (match_dup 2)
	  (match_operand:MMXMODEI 1 "register_operand")))]
  "TARGET_MMX_WITH_SSE"
  "operands[2] = force_reg (<MODE>mode, CONST0_RTX (<MODE>mode));")

(define_expand "neg<mode>2"
  [(set (match_operand:VI_32 0 "register_operand")
	(minus:VI_32
	  (match_dup 2)
	  (match_operand:VI_32 1 "register_operand")))]
  "TARGET_SSE2"
  "operands[2] = force_reg (<MODE>mode, CONST0_RTX (<MODE>mode));")

(define_insn "negv2qi2"
  [(set (match_operand:V2QI 0 "register_operand" "=?Q,&Yw")
	(neg:V2QI
	  (match_operand:V2QI 1 "register_operand" "0,Yw")))
   (clobber (reg:CC FLAGS_REG))]
  "!TARGET_PARTIAL_REG_STALL || optimize_size || TARGET_SSE2"
  "#"
  [(set_attr "isa" "*,sse2")
   (set_attr "type" "multi")
   (set_attr "mode" "QI,TI")
   (set (attr "enabled")
	(cond [(and (eq_attr "alternative" "0")
		    (and (match_test "TARGET_PARTIAL_REG_STALL")
			 (not (match_test "optimize_function_for_size_p (cfun)"))))
		(symbol_ref "false")
	      ]
	      (const_string "*")))])

(define_split
  [(set (match_operand:V2QI 0 "general_reg_operand")
	(neg:V2QI
	  (match_operand:V2QI 1 "general_reg_operand")))
   (clobber (reg:CC FLAGS_REG))]
  "(!TARGET_PARTIAL_REG_STALL || optimize_function_for_size_p (cfun))
   && reload_completed"
  [(parallel
     [(set (strict_low_part (match_dup 0))
	   (neg:QI (match_dup 1)))
      (clobber (reg:CC FLAGS_REG))])
   (parallel
     [(set (zero_extract:HI (match_dup 2) (const_int 8) (const_int 8))
	   (subreg:HI
	     (neg:QI
	       (subreg:QI
		 (zero_extract:HI (match_dup 3)
				  (const_int 8)
				  (const_int 8)) 0)) 0))
      (clobber (reg:CC FLAGS_REG))])]
{
  operands[3] = lowpart_subreg (HImode, operands[1], V2QImode);
  operands[2] = lowpart_subreg (HImode, operands[0], V2QImode);
  operands[1] = lowpart_subreg (QImode, operands[1], V2QImode);
  operands[0] = lowpart_subreg (QImode, operands[0], V2QImode);
})

(define_split
  [(set (match_operand:V2QI 0 "sse_reg_operand")
	(neg:V2QI
	  (match_operand:V2QI 1 "sse_reg_operand")))
   (clobber (reg:CC FLAGS_REG))]
  "TARGET_SSE2 && reload_completed"
  [(set (match_dup 0) (match_dup 2))
   (set (match_dup 0)
	(minus:V16QI (match_dup 0) (match_dup 1)))]
{
  operands[2] = CONST0_RTX (V16QImode);
  operands[1] = lowpart_subreg (V16QImode, operands[1], V2QImode);
  operands[0] = lowpart_subreg (V16QImode, operands[0], V2QImode);
})

(define_expand "mmx_<insn><mode>3"
  [(set (match_operand:MMXMODEI8 0 "register_operand")
	(plusminus:MMXMODEI8
	  (match_operand:MMXMODEI8 1 "register_mmxmem_operand")
	  (match_operand:MMXMODEI8 2 "register_mmxmem_operand")))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "ix86_fixup_binary_operands_no_copy (<CODE>, <MODE>mode, operands);")

(define_expand "<insn><mode>3"
  [(set (match_operand:MMXMODEI 0 "register_operand")
	(plusminus:MMXMODEI
	  (match_operand:MMXMODEI 1 "register_operand")
	  (match_operand:MMXMODEI 2 "register_operand")))]
  "TARGET_MMX_WITH_SSE")

(define_insn "*mmx_<insn><mode>3"
  [(set (match_operand:MMXMODEI8 0 "register_operand" "=y,x,<Yv_Yw>")
	(plusminus:MMXMODEI8
	  (match_operand:MMXMODEI8 1 "register_mmxmem_operand"
	    "<comm>0,0,<Yv_Yw>")
	  (match_operand:MMXMODEI8 2 "register_mmxmem_operand"
	    "ym,x,<Yv_Yw>")))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && ix86_binary_operator_ok (<CODE>, <MODE>mode, operands)"
  "@
   p<plusminus_mnemonic><mmxvecsize>\t{%2, %0|%0, %2}
   p<plusminus_mnemonic><mmxvecsize>\t{%2, %0|%0, %2}
   vp<plusminus_mnemonic><mmxvecsize>\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse2_noavx,avx")
   (set_attr "mmx_isa" "native,*,*")
   (set_attr "type" "mmxadd,sseadd,sseadd")
   (set_attr "mode" "DI,TI,TI")])

(define_insn "<insn><mode>3"
  [(set (match_operand:VI_32 0 "register_operand" "=x,Yw")
	(plusminus:VI_32
	  (match_operand:VI_32 1 "register_operand" "<comm>0,Yw")
	  (match_operand:VI_32 2 "register_operand" "x,Yw")))]
  "TARGET_SSE2"
  "@
   p<plusminus_mnemonic><mmxvecsize>\t{%2, %0|%0, %2}
   vp<plusminus_mnemonic><mmxvecsize>\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sseadd")
   (set_attr "mode" "TI")])

(define_insn "<insn>v2qi3"
  [(set (match_operand:V2QI 0 "register_operand" "=?Q,x,Yw")
	(plusminus:V2QI
	  (match_operand:V2QI 1 "register_operand" "<comm>0,0,Yw")
	  (match_operand:V2QI 2 "register_operand" "Q,x,Yw")))
   (clobber (reg:CC FLAGS_REG))]
  "!TARGET_PARTIAL_REG_STALL || optimize_size || TARGET_SSE2"
  "#"
  [(set_attr "isa" "*,sse2_noavx,avx")
   (set_attr "type" "multi,sseadd,sseadd")
   (set_attr "mode" "QI,TI,TI")
   (set (attr "enabled")
	(cond [(and (eq_attr "alternative" "0")
		    (and (match_test "TARGET_PARTIAL_REG_STALL")
			 (not (match_test "optimize_function_for_size_p (cfun)"))))
		(symbol_ref "false")
	      ]
	      (const_string "*")))])

(define_split
  [(set (match_operand:V2QI 0 "general_reg_operand")
	(plusminus:V2QI
	  (match_operand:V2QI 1 "general_reg_operand")
	  (match_operand:V2QI 2 "general_reg_operand")))
   (clobber (reg:CC FLAGS_REG))]
  "(!TARGET_PARTIAL_REG_STALL || optimize_function_for_size_p (cfun))
   && reload_completed"
  [(parallel
     [(set (strict_low_part (match_dup 0))
	   (plusminus:QI (match_dup 1) (match_dup 2)))
      (clobber (reg:CC FLAGS_REG))])
   (parallel
     [(set (zero_extract:HI (match_dup 3) (const_int 8) (const_int 8))
	   (subreg:HI
	     (plusminus:QI
	       (subreg:QI
		 (zero_extract:HI (match_dup 4)
				  (const_int 8)
				  (const_int 8)) 0)
	       (subreg:QI
		 (zero_extract:HI (match_dup 5)
				  (const_int 8)
				  (const_int 8)) 0)) 0))
      (clobber (reg:CC FLAGS_REG))])]
{
  operands[5] = lowpart_subreg (HImode, operands[2], V2QImode);
  operands[4] = lowpart_subreg (HImode, operands[1], V2QImode);
  operands[3] = lowpart_subreg (HImode, operands[0], V2QImode);
  operands[2] = lowpart_subreg (QImode, operands[2], V2QImode);
  operands[1] = lowpart_subreg (QImode, operands[1], V2QImode);
  operands[0] = lowpart_subreg (QImode, operands[0], V2QImode);
})

(define_split
  [(set (match_operand:V2QI 0 "sse_reg_operand")
	(plusminus:V2QI
	  (match_operand:V2QI 1 "sse_reg_operand")
	  (match_operand:V2QI 2 "sse_reg_operand")))
   (clobber (reg:CC FLAGS_REG))]
  "TARGET_SSE2 && reload_completed"
  [(set (match_dup 0)
	(plusminus:V16QI (match_dup 1) (match_dup 2)))]
{
  operands[2] = lowpart_subreg (V16QImode, operands[2], V2QImode);
  operands[1] = lowpart_subreg (V16QImode, operands[1], V2QImode);
  operands[0] = lowpart_subreg (V16QImode, operands[0], V2QImode);
})

(define_expand "mmx_<insn><mode>3"
  [(set (match_operand:MMXMODE12 0 "register_operand")
	(sat_plusminus:MMXMODE12
	  (match_operand:MMXMODE12 1 "register_mmxmem_operand")
	  (match_operand:MMXMODE12 2 "register_mmxmem_operand")))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "ix86_fixup_binary_operands_no_copy (<CODE>, <MODE>mode, operands);")

(define_expand "<insn><mode>3"
  [(set (match_operand:MMXMODE12 0 "register_operand")
	(sat_plusminus:MMXMODE12
	  (match_operand:MMXMODE12 1 "register_operand")
	  (match_operand:MMXMODE12 2 "register_operand")))]
  "TARGET_MMX_WITH_SSE")

(define_insn "*mmx_<insn><mode>3"
  [(set (match_operand:MMXMODE12 0 "register_operand" "=y,x,Yw")
	(sat_plusminus:MMXMODE12
	  (match_operand:MMXMODE12 1 "register_mmxmem_operand" "<comm>0,0,Yw")
	  (match_operand:MMXMODE12 2 "register_mmxmem_operand" "ym,x,Yw")))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && ix86_binary_operator_ok (<CODE>, <MODE>mode, operands)"
  "@
   p<plusminus_mnemonic><mmxvecsize>\t{%2, %0|%0, %2}
   p<plusminus_mnemonic><mmxvecsize>\t{%2, %0|%0, %2}
   vp<plusminus_mnemonic><mmxvecsize>\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse2_noavx,avx")
   (set_attr "mmx_isa" "native,*,*")
   (set_attr "type" "mmxadd,sseadd,sseadd")
   (set_attr "mode" "DI,TI,TI")])

(define_insn "<insn><mode>3"
  [(set (match_operand:VI_16_32 0 "register_operand" "=x,Yw")
	(sat_plusminus:VI_16_32
	  (match_operand:VI_16_32 1 "register_operand" "<comm>0,Yw")
	  (match_operand:VI_16_32 2 "register_operand" "x,Yw")))]
  "TARGET_SSE2"
  "@
   p<plusminus_mnemonic><mmxvecsize>\t{%2, %0|%0, %2}
   vp<plusminus_mnemonic><mmxvecsize>\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sseadd")
   (set_attr "mode" "TI")])

(define_insn "mulv2si3"
  [(set (match_operand:V2SI 0 "register_operand" "=Yr,*x,v")
	(mult:V2SI
	  (match_operand:V2SI 1 "register_operand" "%0,0,v")
	  (match_operand:V2SI 2 "register_operand" "Yr,*x,v")))]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE"
  "@
   pmulld\t{%2, %0|%0, %2}
   pmulld\t{%2, %0|%0, %2}
   vpmulld\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,noavx,avx")
   (set_attr "type" "sseimul")
   (set_attr "prefix_extra" "1")
   (set_attr "prefix" "orig,orig,vex")
   (set_attr "btver2_decode" "vector")
   (set_attr "mode" "TI")])

(define_expand "mmx_mulv4hi3"
  [(set (match_operand:V4HI 0 "register_operand")
	(mult:V4HI (match_operand:V4HI 1 "register_mmxmem_operand")
		   (match_operand:V4HI 2 "register_mmxmem_operand")))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "ix86_fixup_binary_operands_no_copy (MULT, V4HImode, operands);")

(define_expand "mulv4hi3"
  [(set (match_operand:V4HI 0 "register_operand")
	(mult:V4HI (match_operand:V4HI 1 "register_operand")
		   (match_operand:V4HI 2 "register_operand")))]
  "TARGET_MMX_WITH_SSE")

(define_insn "*mmx_mulv4hi3"
  [(set (match_operand:V4HI 0 "register_operand" "=y,x,Yw")
	(mult:V4HI (match_operand:V4HI 1 "register_mmxmem_operand" "%0,0,Yw")
		   (match_operand:V4HI 2 "register_mmxmem_operand" "ym,x,Yw")))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && ix86_binary_operator_ok (MULT, V4HImode, operands)"
  "@
   pmullw\t{%2, %0|%0, %2}
   pmullw\t{%2, %0|%0, %2}
   vpmullw\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse2_noavx,avx")
   (set_attr "mmx_isa" "native,*,*")
   (set_attr "type" "mmxmul,ssemul,ssemul")
   (set_attr "mode" "DI,TI,TI")])

(define_insn "mulv2hi3"
  [(set (match_operand:V2HI 0 "register_operand" "=x,Yw")
	(mult:V2HI (match_operand:V2HI 1 "register_operand" "%0,Yw")
		   (match_operand:V2HI 2 "register_operand" "x,Yw")))]
  "TARGET_SSE2"
  "@
   pmullw\t{%2, %0|%0, %2}
   vpmullw\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "ssemul")
   (set_attr "mode" "TI")])

(define_expand "mulv8qi3"
  [(set (match_operand:V8QI 0 "register_operand")
	(mult:V8QI (match_operand:V8QI 1 "register_operand")
		   (match_operand:V8QI 2 "register_operand")))]
  "TARGET_MMX_WITH_SSE"
{
  ix86_expand_vecop_qihi_partial (MULT, operands[0], operands[1], operands[2]);
  DONE;
})

(define_expand "mulv4qi3"
  [(set (match_operand:V4QI 0 "register_operand")
	(mult:V4QI (match_operand:V4QI 1 "register_operand")
		   (match_operand:V4QI 2 "register_operand")))]
  "TARGET_SSE2"
{
  ix86_expand_vecop_qihi_partial (MULT, operands[0], operands[1], operands[2]);
  DONE;
})

(define_expand "mmx_smulv4hi3_highpart"
  [(set (match_operand:V4HI 0 "register_operand")
	(truncate:V4HI
	  (lshiftrt:V4SI
	    (mult:V4SI
	      (sign_extend:V4SI
		(match_operand:V4HI 1 "register_mmxmem_operand"))
	      (sign_extend:V4SI
		(match_operand:V4HI 2 "register_mmxmem_operand")))
	    (const_int 16))))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "ix86_fixup_binary_operands_no_copy (MULT, V4HImode, operands);")

(define_insn "*mmx_smulv4hi3_highpart"
  [(set (match_operand:V4HI 0 "register_operand" "=y,x,Yw")
	(truncate:V4HI
	  (lshiftrt:V4SI
	    (mult:V4SI
	      (sign_extend:V4SI
		(match_operand:V4HI 1 "register_mmxmem_operand" "%0,0,Yw"))
	      (sign_extend:V4SI
		(match_operand:V4HI 2 "register_mmxmem_operand" "ym,x,Yw")))
	    (const_int 16))))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && ix86_binary_operator_ok (MULT, V4HImode, operands)"
  "@
   pmulhw\t{%2, %0|%0, %2}
   pmulhw\t{%2, %0|%0, %2}
   vpmulhw\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse2_noavx,avx")
   (set_attr "mmx_isa" "native,*,*")
   (set_attr "type" "mmxmul,ssemul,ssemul")
   (set_attr "mode" "DI,TI,TI")])

(define_expand "mmx_umulv4hi3_highpart"
  [(set (match_operand:V4HI 0 "register_operand")
	(truncate:V4HI
	  (lshiftrt:V4SI
	    (mult:V4SI
	      (zero_extend:V4SI
		(match_operand:V4HI 1 "register_mmxmem_operand"))
	      (zero_extend:V4SI
		(match_operand:V4HI 2 "register_mmxmem_operand")))
	    (const_int 16))))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && (TARGET_SSE || TARGET_3DNOW_A)"
  "ix86_fixup_binary_operands_no_copy (MULT, V4HImode, operands);")

(define_insn "*mmx_umulv4hi3_highpart"
  [(set (match_operand:V4HI 0 "register_operand" "=y,x,Yw")
	(truncate:V4HI
	  (lshiftrt:V4SI
	    (mult:V4SI
	      (zero_extend:V4SI
		(match_operand:V4HI 1 "register_mmxmem_operand" "%0,0,Yw"))
	      (zero_extend:V4SI
		(match_operand:V4HI 2 "register_mmxmem_operand" "ym,x,Yw")))
	  (const_int 16))))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && (TARGET_SSE || TARGET_3DNOW_A)
   && ix86_binary_operator_ok (MULT, V4HImode, operands)"
  "@
   pmulhuw\t{%2, %0|%0, %2}
   pmulhuw\t{%2, %0|%0, %2}
   vpmulhuw\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse2_noavx,avx")
   (set_attr "mmx_isa" "native,*,*")
   (set_attr "type" "mmxmul,ssemul,ssemul")
   (set_attr "mode" "DI,TI,TI")])

(define_expand "<s>mulv4hi3_highpart"
  [(set (match_operand:V4HI 0 "register_operand")
	(truncate:V4HI
	  (lshiftrt:V4SI
	    (mult:V4SI
	      (any_extend:V4SI
		(match_operand:V4HI 1 "register_operand"))
	      (any_extend:V4SI
		(match_operand:V4HI 2 "register_operand")))
	    (const_int 16))))]
  "TARGET_MMX_WITH_SSE")

(define_insn "<s>mulv2hi3_highpart"
  [(set (match_operand:V2HI 0 "register_operand" "=x,Yw")
	(truncate:V2HI
	  (lshiftrt:V2SI
	    (mult:V2SI
	      (any_extend:V2SI
		(match_operand:V2HI 1 "register_operand" "%0,Yw"))
	      (any_extend:V2SI
		(match_operand:V2HI 2 "register_operand" "x,Yw")))
	    (const_int 16))))]
  "TARGET_SSE2"
  "@
   pmulh<u>w\t{%2, %0|%0, %2}
   vpmulh<u>w\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "ssemul")
   (set_attr "mode" "TI")])

(define_expand "mmx_pmaddwd"
  [(set (match_operand:V2SI 0 "register_operand")
	(plus:V2SI
	  (mult:V2SI
	    (sign_extend:V2SI
	      (vec_select:V2HI
		(match_operand:V4HI 1 "register_mmxmem_operand")
		(parallel [(const_int 0) (const_int 2)])))
	    (sign_extend:V2SI
	      (vec_select:V2HI
		(match_operand:V4HI 2 "register_mmxmem_operand")
		(parallel [(const_int 0) (const_int 2)]))))
	  (mult:V2SI
	    (sign_extend:V2SI
	      (vec_select:V2HI (match_dup 1)
		(parallel [(const_int 1) (const_int 3)])))
	    (sign_extend:V2SI
	      (vec_select:V2HI (match_dup 2)
		(parallel [(const_int 1) (const_int 3)]))))))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "ix86_fixup_binary_operands_no_copy (MULT, V4HImode, operands);")

(define_insn "*mmx_pmaddwd"
  [(set (match_operand:V2SI 0 "register_operand" "=y,x,Yw")
	(plus:V2SI
	  (mult:V2SI
	    (sign_extend:V2SI
	      (vec_select:V2HI
		(match_operand:V4HI 1 "register_mmxmem_operand" "%0,0,Yw")
		(parallel [(const_int 0) (const_int 2)])))
	    (sign_extend:V2SI
	      (vec_select:V2HI
		(match_operand:V4HI 2 "register_mmxmem_operand" "ym,x,Yw")
		(parallel [(const_int 0) (const_int 2)]))))
	  (mult:V2SI
	    (sign_extend:V2SI
	      (vec_select:V2HI (match_dup 1)
		(parallel [(const_int 1) (const_int 3)])))
	    (sign_extend:V2SI
	      (vec_select:V2HI (match_dup 2)
		(parallel [(const_int 1) (const_int 3)]))))))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && ix86_binary_operator_ok (MULT, V4HImode, operands)"
  "@
   pmaddwd\t{%2, %0|%0, %2}
   pmaddwd\t{%2, %0|%0, %2}
   vpmaddwd\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse2_noavx,avx")
   (set_attr "mmx_isa" "native,*,*")
   (set_attr "type" "mmxmul,sseiadd,sseiadd")
   (set_attr "mode" "DI,TI,TI")])

(define_expand "mmx_pmulhrwv4hi3"
  [(set (match_operand:V4HI 0 "register_operand")
	(truncate:V4HI
	  (lshiftrt:V4SI
	    (plus:V4SI
	      (mult:V4SI
		(sign_extend:V4SI
		  (match_operand:V4HI 1 "nonimmediate_operand"))
		(sign_extend:V4SI
		  (match_operand:V4HI 2 "nonimmediate_operand")))
	      (const_vector:V4SI [(const_int 32768) (const_int 32768)
				  (const_int 32768) (const_int 32768)]))
	    (const_int 16))))]
  "TARGET_3DNOW"
  "ix86_fixup_binary_operands_no_copy (MULT, V4HImode, operands);")

(define_insn "*mmx_pmulhrwv4hi3"
  [(set (match_operand:V4HI 0 "register_operand" "=y")
	(truncate:V4HI
	  (lshiftrt:V4SI
	    (plus:V4SI
	      (mult:V4SI
		(sign_extend:V4SI
		  (match_operand:V4HI 1 "nonimmediate_operand" "%0"))
		(sign_extend:V4SI
		  (match_operand:V4HI 2 "nonimmediate_operand" "ym")))
	      (const_vector:V4SI [(const_int 32768) (const_int 32768)
				  (const_int 32768) (const_int 32768)]))
	    (const_int 16))))]
  "TARGET_3DNOW && ix86_binary_operator_ok (MULT, V4HImode, operands)"
  "pmulhrw\t{%2, %0|%0, %2}"
  [(set_attr "type" "mmxmul")
   (set_attr "prefix_extra" "1")
   (set_attr "mode" "DI")])

(define_expand "sse2_umulv1siv1di3"
  [(set (match_operand:V1DI 0 "register_operand")
	(mult:V1DI
	  (zero_extend:V1DI
	    (vec_select:V1SI
	      (match_operand:V2SI 1 "register_mmxmem_operand")
	      (parallel [(const_int 0)])))
	  (zero_extend:V1DI
	    (vec_select:V1SI
	      (match_operand:V2SI 2 "register_mmxmem_operand")
	      (parallel [(const_int 0)])))))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE) && TARGET_SSE2"
  "ix86_fixup_binary_operands_no_copy (MULT, V2SImode, operands);")

(define_insn "*sse2_umulv1siv1di3"
  [(set (match_operand:V1DI 0 "register_operand" "=y,x,Yv")
	(mult:V1DI
	  (zero_extend:V1DI
	    (vec_select:V1SI
	      (match_operand:V2SI 1 "register_mmxmem_operand" "%0,0,Yv")
	      (parallel [(const_int 0)])))
	  (zero_extend:V1DI
	    (vec_select:V1SI
	      (match_operand:V2SI 2 "register_mmxmem_operand" "ym,x,Yv")
	      (parallel [(const_int 0)])))))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && TARGET_SSE2
   && ix86_binary_operator_ok (MULT, V2SImode, operands)"
  "@
   pmuludq\t{%2, %0|%0, %2}
   pmuludq\t{%2, %0|%0, %2}
   vpmuludq\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse2_noavx,avx")
   (set_attr "mmx_isa" "native,*,*")
   (set_attr "type" "mmxmul,ssemul,ssemul")
   (set_attr "mode" "DI,TI,TI")])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel integral shifts
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_insn "<code><mode>3"
  [(set (match_operand:MMXMODE14 0 "register_operand" "=Yr,*x,Yv")
	(smaxmin:MMXMODE14
	  (match_operand:MMXMODE14 1 "register_operand" "%0,0,Yv")
	  (match_operand:MMXMODE14 2 "register_operand" "Yr,*x,Yv")))]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE"
  "@
   p<maxmin_int><mmxvecsize>\t{%2, %0|%0, %2}
   p<maxmin_int><mmxvecsize>\t{%2, %0|%0, %2}
   vp<maxmin_int><mmxvecsize>\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,noavx,avx")
   (set_attr "type" "sseiadd")
   (set_attr "prefix_extra" "1")
   (set_attr "prefix" "orig,orig,vex")
   (set_attr "mode" "TI")])

(define_expand "mmx_<code>v4hi3"
  [(set (match_operand:V4HI 0 "register_operand")
	(smaxmin:V4HI
	  (match_operand:V4HI 1 "register_mmxmem_operand")
	  (match_operand:V4HI 2 "register_mmxmem_operand")))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && (TARGET_SSE || TARGET_3DNOW_A)"
  "ix86_fixup_binary_operands_no_copy (<CODE>, V4HImode, operands);")

(define_insn "*mmx_<code>v4hi3"
  [(set (match_operand:V4HI 0 "register_operand" "=y,x,Yw")
	(smaxmin:V4HI
	  (match_operand:V4HI 1 "register_mmxmem_operand" "%0,0,Yw")
	  (match_operand:V4HI 2 "register_mmxmem_operand" "ym,x,Yw")))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && (TARGET_SSE || TARGET_3DNOW_A)
   && ix86_binary_operator_ok (<CODE>, V4HImode, operands)"
  "@
   p<maxmin_int>w\t{%2, %0|%0, %2}
   p<maxmin_int>w\t{%2, %0|%0, %2}
   vp<maxmin_int>w\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse2_noavx,avx")
   (set_attr "mmx_isa" "native,*,*")
   (set_attr "type" "mmxadd,sseiadd,sseiadd")
   (set_attr "mode" "DI,TI,TI")])

(define_expand "<code>v4hi3"
  [(set (match_operand:V4HI 0 "register_operand")
	(smaxmin:V4HI
	  (match_operand:V4HI 1 "register_operand")
	  (match_operand:V4HI 2 "register_operand")))]
  "TARGET_MMX_WITH_SSE")

(define_insn "<code><mode>3"
  [(set (match_operand:VI1_16_32 0 "register_operand" "=Yr,*x,Yv")
	(smaxmin:VI1_16_32
	  (match_operand:VI1_16_32 1 "register_operand" "%0,0,Yv")
	  (match_operand:VI1_16_32 2 "register_operand" "Yr,*x,Yv")))]
  "TARGET_SSE4_1"
  "@
   p<maxmin_int>b\t{%2, %0|%0, %2}
   p<maxmin_int>b\t{%2, %0|%0, %2}
   vp<maxmin_int>b\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,noavx,avx")
   (set_attr "type" "sseiadd")
   (set_attr "prefix_extra" "1")
   (set_attr "prefix" "orig,orig,vex")
   (set_attr "mode" "TI")])

(define_insn "<code>v2hi3"
  [(set (match_operand:V2HI 0 "register_operand" "=x,Yw")
	(smaxmin:V2HI
	  (match_operand:V2HI 1 "register_operand" "%0,Yw")
	  (match_operand:V2HI 2 "register_operand" "x,Yw")))]
  "TARGET_SSE2"
  "@
   p<maxmin_int>w\t{%2, %0|%0, %2}
   vp<maxmin_int>w\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sseiadd")
   (set_attr "mode" "TI")])

(define_insn "<code><mode>3"
  [(set (match_operand:MMXMODE24 0 "register_operand" "=Yr,*x,Yv")
	(umaxmin:MMXMODE24
	  (match_operand:MMXMODE24 1 "register_operand" "%0,0,Yv")
	  (match_operand:MMXMODE24 2 "register_operand" "Yr,*x,Yv")))]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE"
  "@
   p<maxmin_int><mmxvecsize>\t{%2, %0|%0, %2}
   p<maxmin_int><mmxvecsize>\t{%2, %0|%0, %2}
   vp<maxmin_int><mmxvecsize>\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,noavx,avx")
   (set_attr "type" "sseiadd")
   (set_attr "prefix_extra" "1")
   (set_attr "prefix" "orig,orig,vex")
   (set_attr "mode" "TI")])

(define_expand "mmx_<code>v8qi3"
  [(set (match_operand:V8QI 0 "register_operand")
	(umaxmin:V8QI
	  (match_operand:V8QI 1 "register_mmxmem_operand")
	  (match_operand:V8QI 2 "register_mmxmem_operand")))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && (TARGET_SSE || TARGET_3DNOW_A)"
  "ix86_fixup_binary_operands_no_copy (<CODE>, V8QImode, operands);")

(define_insn "*mmx_<code>v8qi3"
  [(set (match_operand:V8QI 0 "register_operand" "=y,x,Yw")
	(umaxmin:V8QI
	  (match_operand:V8QI 1 "register_mmxmem_operand" "%0,0,Yw")
	  (match_operand:V8QI 2 "register_mmxmem_operand" "ym,x,Yw")))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && (TARGET_SSE || TARGET_3DNOW_A)
   && ix86_binary_operator_ok (<CODE>, V8QImode, operands)"
  "@
   p<maxmin_int>b\t{%2, %0|%0, %2}
   p<maxmin_int>b\t{%2, %0|%0, %2}
   vp<maxmin_int>b\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse2_noavx,avx")
   (set_attr "mmx_isa" "native,*,*")
   (set_attr "type" "mmxadd,sseiadd,sseiadd")
   (set_attr "mode" "DI,TI,TI")])

(define_expand "<code>v8qi3"
  [(set (match_operand:V8QI 0 "register_operand")
	(umaxmin:V8QI
	  (match_operand:V8QI 1 "register_operand")
	  (match_operand:V8QI 2 "register_operand")))]
  "TARGET_MMX_WITH_SSE")

(define_insn "<code><mode>3"
  [(set (match_operand:VI1_16_32 0 "register_operand" "=x,Yw")
	(umaxmin:VI1_16_32
	  (match_operand:VI1_16_32 1 "register_operand" "%0,Yw")
	  (match_operand:VI1_16_32 2 "register_operand" "x,Yw")))]
  "TARGET_SSE2"
  "@
   p<maxmin_int>b\t{%2, %0|%0, %2}
   vp<maxmin_int>b\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sseiadd")
   (set_attr "mode" "TI")])

(define_insn "<code>v2hi3"
  [(set (match_operand:V2HI 0 "register_operand" "=Yr,*x,Yv")
	(umaxmin:V2HI
	  (match_operand:V2HI 1 "register_operand" "%0,0,Yv")
	  (match_operand:V2HI 2 "register_operand" "Yr,*x,Yv")))]
  "TARGET_SSE4_1"
  "@
   p<maxmin_int>w\t{%2, %0|%0, %2}
   p<maxmin_int>w\t{%2, %0|%0, %2}
   vp<maxmin_int>w\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,noavx,avx")
   (set_attr "type" "sseiadd")
   (set_attr "prefix_extra" "1")
   (set_attr "prefix" "orig,orig,vex")
   (set_attr "mode" "TI")])

(define_insn "ssse3_abs<mode>2"
  [(set (match_operand:MMXMODEI 0 "register_operand" "=y,Yv")
	(abs:MMXMODEI
	  (match_operand:MMXMODEI 1 "register_mmxmem_operand" "ym,Yv")))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE) && TARGET_SSSE3"
  "@
   pabs<mmxvecsize>\t{%1, %0|%0, %1}
   %vpabs<mmxvecsize>\t{%1, %0|%0, %1}"
  [(set_attr "mmx_isa" "native,*")
   (set_attr "type" "sselog1")
   (set_attr "prefix_rep" "0")
   (set_attr "prefix_extra" "1")
   (set (attr "prefix_rex") (symbol_ref "x86_extended_reg_mentioned_p (insn)"))
   (set_attr "mode" "DI,TI")])

(define_expand "abs<mode>2"
  [(set (match_operand:MMXMODEI 0 "register_operand")
	(abs:MMXMODEI
	  (match_operand:MMXMODEI 1 "register_operand")))]
  "TARGET_SSSE3 && TARGET_MMX_WITH_SSE")

(define_insn "abs<mode>2"
  [(set (match_operand:VI_16_32 0 "register_operand" "=Yv")
	(abs:VI_16_32
	  (match_operand:VI_16_32 1 "register_operand" "Yv")))]
  "TARGET_SSSE3"
  "%vpabs<mmxvecsize>\t{%1, %0|%0, %1}"
  [(set_attr "type" "sselog1")
   (set_attr "prefix_rep" "0")
   (set_attr "prefix_extra" "1")
   (set (attr "prefix_rex") (symbol_ref "x86_extended_reg_mentioned_p (insn)"))
   (set_attr "mode" "TI")])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel integral shifts
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_insn "mmx_ashr<mode>3"
  [(set (match_operand:MMXMODE24 0 "register_operand" "=y,x,<Yv_Yw>")
	(ashiftrt:MMXMODE24
	  (match_operand:MMXMODE24 1 "register_operand" "0,0,<Yv_Yw>")
	  (match_operand:DI 2 "nonmemory_operand" "yN,xN,<Yv_Yw>N")))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "@
   psra<mmxvecsize>\t{%2, %0|%0, %2}
   psra<mmxvecsize>\t{%2, %0|%0, %2}
   vpsra<mmxvecsize>\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse2_noavx,avx")
   (set_attr "mmx_isa" "native,*,*")
   (set_attr "type" "mmxshft,sseishft,sseishft")
   (set (attr "length_immediate")
     (if_then_else (match_operand 2 "const_int_operand")
       (const_string "1")
       (const_string "0")))
   (set_attr "mode" "DI,TI,TI")])

(define_insn_and_split "*mmx_ashr<mode>3_1"
  [(set (match_operand:MMXMODE24 0 "register_operand")
	(lt:MMXMODE24
	  (match_operand:MMXMODE24 1 "register_operand")
	  (match_operand:MMXMODE24 2 "const0_operand")))]
  "TARGET_MMX_WITH_SSE && ix86_pre_reload_split ()"
  "#"
  "&& 1"
  [(set (match_dup 0) (ashiftrt:MMXMODE24 (match_dup 1) (match_dup 3)))]
  "operands[3] = gen_int_mode (<mmxscalarsize> - 1, DImode);")

(define_expand "ashr<mode>3"
  [(set (match_operand:MMXMODE24 0 "register_operand")
	(ashiftrt:MMXMODE24
	  (match_operand:MMXMODE24 1 "register_operand")
	  (match_operand:DI 2 "nonmemory_operand")))]
  "TARGET_MMX_WITH_SSE")

(define_insn "mmx_<insn><mode>3"
  [(set (match_operand:MMXMODE248 0 "register_operand" "=y,x,<Yv_Yw>")
	(any_lshift:MMXMODE248
	  (match_operand:MMXMODE248 1 "register_operand" "0,0,<Yv_Yw>")
	  (match_operand:DI 2 "nonmemory_operand" "yN,xN,<Yv_Yw>N")))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "@
   p<vshift><mmxvecsize>\t{%2, %0|%0, %2}
   p<vshift><mmxvecsize>\t{%2, %0|%0, %2}
   vp<vshift><mmxvecsize>\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse2_noavx,avx")
   (set_attr "mmx_isa" "native,*,*")
   (set_attr "type" "mmxshft,sseishft,sseishft")
   (set (attr "length_immediate")
     (if_then_else (match_operand 2 "const_int_operand")
       (const_string "1")
       (const_string "0")))
   (set_attr "mode" "DI,TI,TI")])

(define_split
  [(set (match_operand:MMXMODE248 0 "register_operand")
  	(and:MMXMODE248
	  (lt:MMXMODE248
	    (match_operand:MMXMODE248 1 "register_operand")
	    (match_operand:MMXMODE248 2 "const0_operand"))
	  (match_operand:MMXMODE248 3 "const1_operand")))]
  "TARGET_MMX_WITH_SSE && ix86_pre_reload_split ()"
  [(set (match_dup 0) (lshiftrt:MMXMODE248 (match_dup 1) (match_dup 4)))]
  "operands[4] = gen_int_mode (<mmxscalarsize> - 1, DImode);")

(define_expand "<insn><mode>3"
  [(set (match_operand:MMXMODE24 0 "register_operand")
	(any_lshift:MMXMODE24
	  (match_operand:MMXMODE24 1 "register_operand")
	  (match_operand:DI 2 "nonmemory_operand")))]
  "TARGET_MMX_WITH_SSE")

(define_insn "mmx_<insn>v1si3"
  [(set (match_operand:V1SI 0 "register_operand" "=x,Yw")
	(any_lshift:V1SI
	  (match_operand:V1SI 1 "register_operand" "0,Yw")
	  (match_operand:DI 2 "nonmemory_operand" "xN,YwN")))]
  "TARGET_SSE2"
  "@
   p<vshift>d\t{%2, %0|%0, %2}
   vp<vshift>d\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sseishft")
   (set (attr "length_immediate")
     (if_then_else (match_operand 2 "const_int_operand")
       (const_string "1")
       (const_string "0")))
   (set_attr "mode" "TI")])

(define_insn "<insn>v2hi3"
  [(set (match_operand:V2HI 0 "register_operand" "=x,Yw")
	(any_shift:V2HI
	  (match_operand:V2HI 1 "register_operand" "0,Yw")
	  (match_operand:DI 2 "nonmemory_operand" "xN,YwN")))]
  "TARGET_SSE2"
  "@
   p<vshift>w\t{%2, %0|%0, %2}
   vp<vshift>w\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sseishft")
   (set (attr "length_immediate")
     (if_then_else (match_operand 2 "const_int_operand")
       (const_string "1")
       (const_string "0")))
   (set_attr "mode" "TI")])

(define_insn_and_split "*mmx_ashrv2hi3_1"
  [(set (match_operand:V2HI 0 "register_operand")
	(lt:V2HI
	  (match_operand:V2HI 1 "register_operand")
	  (match_operand:V2HI 2 "const0_operand")))]
  "TARGET_SSE2 && ix86_pre_reload_split ()"
  "#"
  "&& 1"
  [(set (match_dup 0) (ashiftrt:V2HI (match_dup 1) (match_dup 3)))]
  "operands[3] = gen_int_mode (15, DImode);")

(define_split
  [(set (match_operand:V2HI 0 "register_operand")
  	(and:V2HI
	  (lt:V2HI
	    (match_operand:V2HI 1 "register_operand")
	    (match_operand:V2HI 2 "const0_operand"))
	  (match_operand:V2HI 3 "const1_operand")))]
  "TARGET_SSE2 && ix86_pre_reload_split ()"
  [(set (match_dup 0) (lshiftrt:V2HI (match_dup 1) (match_dup 4)))]
  "operands[4] = gen_int_mode (15, DImode);")

(define_expand "<insn>v8qi3"
  [(set (match_operand:V8QI 0 "register_operand")
	(any_shift:V8QI (match_operand:V8QI 1 "register_operand")
			(match_operand:DI 2 "nonmemory_operand")))]
  "TARGET_MMX_WITH_SSE"
{
  ix86_expand_vecop_qihi_partial (<CODE>, operands[0],
				  operands[1], operands[2]);
  DONE;
})

(define_expand "<insn>v4qi3"
  [(set (match_operand:V4QI 0 "register_operand")
	(any_shift:V4QI (match_operand:V4QI 1 "register_operand")
			(match_operand:DI 2 "nonmemory_operand")))]
  "TARGET_SSE2"
{
  ix86_expand_vecop_qihi_partial (<CODE>, operands[0],
				  operands[1], operands[2]);
  DONE;
})

(define_insn_and_split "<insn>v2qi3"
  [(set (match_operand:V2QI 0 "register_operand" "=Q")
	(any_shift:V2QI
	  (match_operand:V2QI 1 "register_operand" "0")
	  (match_operand:QI 2 "nonmemory_operand" "cI")))
   (clobber (reg:CC FLAGS_REG))]
  "!TARGET_PARTIAL_REG_STALL || optimize_size"
  "#"
  "(!TARGET_PARTIAL_REG_STALL || optimize_function_for_size_p (cfun))
   && reload_completed"
  [(parallel
     [(set (zero_extract:HI (match_dup 3) (const_int 8) (const_int 8))
	   (subreg:HI
	     (any_shift:QI
	       (subreg:QI
		 (zero_extract:HI (match_dup 4)
				  (const_int 8)
				  (const_int 8)) 0)
	       (match_dup 2)) 0))
      (clobber (reg:CC FLAGS_REG))])
   (parallel
     [(set (strict_low_part (match_dup 0))
	   (any_shift:QI (match_dup 1) (match_dup 2)))
      (clobber (reg:CC FLAGS_REG))])]
{
  operands[4] = lowpart_subreg (HImode, operands[1], V2QImode);
  operands[3] = lowpart_subreg (HImode, operands[0], V2QImode);
  operands[1] = lowpart_subreg (QImode, operands[1], V2QImode);
  operands[0] = lowpart_subreg (QImode, operands[0], V2QImode);
}
  [(set_attr "type" "multi")
   (set_attr "mode" "QI")])

(define_expand "v<insn>v8qi3"
  [(set (match_operand:V8QI 0 "register_operand")
	(any_shift:V8QI
	  (match_operand:V8QI 1 "register_operand")
	  (match_operand:V8QI 2 "register_operand")))]
  "TARGET_AVX512BW && TARGET_AVX512VL && TARGET_MMX_WITH_SSE"
{
  ix86_expand_vecop_qihi_partial (<CODE>, operands[0],
				  operands[1], operands[2]);
  DONE;
})

(define_expand "v<insn>v4qi3"
  [(set (match_operand:V4QI 0 "register_operand")
	(any_shift:V4QI
	  (match_operand:V4QI 1 "register_operand")
	  (match_operand:V4QI 2 "register_operand")))]
  "TARGET_AVX512BW && TARGET_AVX512VL"
{
  ix86_expand_vecop_qihi_partial (<CODE>, operands[0],
				  operands[1], operands[2]);
  DONE;
})

(define_expand "vec_shl_<mode>"
  [(set (match_operand:V248FI 0 "register_operand")
	(ashift:V1DI
	  (match_operand:V248FI 1 "nonimmediate_operand")
	  (match_operand:DI 2 "nonmemory_operand")))]
  "TARGET_MMX_WITH_SSE"
{
  rtx op0 = gen_reg_rtx (V1DImode);
  rtx op1 = force_reg (<MODE>mode, operands[1]);

  emit_insn (gen_mmx_ashlv1di3
	      (op0, gen_lowpart (V1DImode, op1), operands[2]));
  emit_move_insn (operands[0], gen_lowpart (<MODE>mode, op0));
  DONE;
})

(define_expand "vec_shl_<mode>"
  [(set (match_operand:V24FI_32 0 "register_operand")
	(ashift:V1SI
	  (match_operand:V24FI_32 1 "nonimmediate_operand")
	  (match_operand:DI 2 "nonmemory_operand")))]
  "TARGET_SSE2"
{
  rtx op0 = gen_reg_rtx (V1SImode);
  rtx op1 = force_reg (<MODE>mode, operands[1]);

  emit_insn (gen_mmx_ashlv1si3
	      (op0, gen_lowpart (V1SImode, op1), operands[2]));
  emit_move_insn (operands[0], gen_lowpart (<MODE>mode, op0));
  DONE;
})

(define_expand "vec_shr_<mode>"
  [(set (match_operand:V248FI 0 "register_operand")
	(lshiftrt:V1DI
	  (match_operand:V248FI 1 "nonimmediate_operand")
	  (match_operand:DI 2 "nonmemory_operand")))]
  "TARGET_MMX_WITH_SSE"
{
  rtx op0 = gen_reg_rtx (V1DImode);
  rtx op1 = force_reg (<MODE>mode, operands[1]);

  emit_insn (gen_mmx_lshrv1di3
	      (op0, gen_lowpart (V1DImode, op1), operands[2]));
  emit_move_insn (operands[0], gen_lowpart (<MODE>mode, op0));
  DONE;
})

(define_expand "vec_shr_<mode>"
  [(set (match_operand:V24FI_32 0 "register_operand")
	(lshiftrt:V1SI
	  (match_operand:V24FI_32 1 "nonimmediate_operand")
	  (match_operand:DI 2 "nonmemory_operand")))]
  "TARGET_SSE2"
{
  rtx op0 = gen_reg_rtx (V1SImode);
  rtx op1 = force_reg (<MODE>mode, operands[1]);

  emit_insn (gen_mmx_lshrv1si3
	      (op0, gen_lowpart (V1SImode, op1), operands[2]));
  emit_move_insn (operands[0], gen_lowpart (<MODE>mode, op0));
  DONE;
})

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel integral comparisons
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_expand "mmx_eq<mode>3"
  [(set (match_operand:MMXMODEI 0 "register_operand")
	(eq:MMXMODEI
	  (match_operand:MMXMODEI 1 "register_mmxmem_operand")
	  (match_operand:MMXMODEI 2 "register_mmxmem_operand")))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "ix86_fixup_binary_operands_no_copy (EQ, <MODE>mode, operands);")

(define_insn "*mmx_eq<mode>3"
  [(set (match_operand:MMXMODEI 0 "register_operand" "=y,x,x")
	(eq:MMXMODEI
	  (match_operand:MMXMODEI 1 "register_mmxmem_operand" "%0,0,x")
	  (match_operand:MMXMODEI 2 "register_mmxmem_operand" "ym,x,x")))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && ix86_binary_operator_ok (EQ, <MODE>mode, operands)"
  "@
   pcmpeq<mmxvecsize>\t{%2, %0|%0, %2}
   pcmpeq<mmxvecsize>\t{%2, %0|%0, %2}
   vpcmpeq<mmxvecsize>\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse2_noavx,avx")
   (set_attr "mmx_isa" "native,*,*")
   (set_attr "type" "mmxcmp,ssecmp,ssecmp")
   (set_attr "mode" "DI,TI,TI")])

(define_insn "*eq<mode>3"
  [(set (match_operand:VI_16_32 0 "register_operand" "=x,x")
	(eq:VI_16_32
	  (match_operand:VI_16_32 1 "register_operand" "%0,x")
	  (match_operand:VI_16_32 2 "register_operand" "x,x")))]
  "TARGET_SSE2"
  "@
   pcmpeq<mmxvecsize>\t{%2, %0|%0, %2}
   vpcmpeq<mmxvecsize>\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "ssecmp")
   (set_attr "mode" "TI")])

(define_insn "mmx_gt<mode>3"
  [(set (match_operand:MMXMODEI 0 "register_operand" "=y,x,x")
	(gt:MMXMODEI
	  (match_operand:MMXMODEI 1 "register_operand" "0,0,x")
	  (match_operand:MMXMODEI 2 "register_mmxmem_operand" "ym,x,x")))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "@
   pcmpgt<mmxvecsize>\t{%2, %0|%0, %2}
   pcmpgt<mmxvecsize>\t{%2, %0|%0, %2}
   vpcmpgt<mmxvecsize>\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse2_noavx,avx")
   (set_attr "mmx_isa" "native,*,*")
   (set_attr "type" "mmxcmp,ssecmp,ssecmp")
   (set_attr "mode" "DI,TI,TI")])

(define_insn "*gt<mode>3"
  [(set (match_operand:VI_16_32 0 "register_operand" "=x,x")
	(gt:VI_16_32
	  (match_operand:VI_16_32 1 "register_operand" "0,x")
	  (match_operand:VI_16_32 2 "register_operand" "x,x")))]
  "TARGET_SSE2"
  "@
   pcmpgt<mmxvecsize>\t{%2, %0|%0, %2}
   vpcmpgt<mmxvecsize>\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "ssecmp")
   (set_attr "mode" "TI")])

(define_insn "*xop_maskcmp<mode>3"
  [(set (match_operand:MMXMODEI 0 "register_operand" "=x")
	(match_operator:MMXMODEI 1 "ix86_comparison_int_operator"
	 [(match_operand:MMXMODEI 2 "register_operand" "x")
	  (match_operand:MMXMODEI 3 "register_operand" "x")]))]
  "TARGET_XOP"
  "vpcom%Y1<mmxvecsize>\t{%3, %2, %0|%0, %2, %3}"
  [(set_attr "type" "sse4arg")
   (set_attr "mode" "TI")])

(define_insn "*xop_maskcmp<mode>3"
  [(set (match_operand:VI_16_32 0 "register_operand" "=x")
	(match_operator:VI_16_32 1 "ix86_comparison_int_operator"
	 [(match_operand:VI_16_32 2 "register_operand" "x")
	  (match_operand:VI_16_32 3 "register_operand" "x")]))]
  "TARGET_XOP"
  "vpcom%Y1<mmxvecsize>\t{%3, %2, %0|%0, %2, %3}"
  [(set_attr "type" "sse4arg")
   (set_attr "mode" "TI")])

(define_insn "*xop_maskcmp_uns<mode>3"
  [(set (match_operand:MMXMODEI 0 "register_operand" "=x")
	(match_operator:MMXMODEI 1 "ix86_comparison_uns_operator"
	 [(match_operand:MMXMODEI 2 "register_operand" "x")
	  (match_operand:MMXMODEI 3 "register_operand" "x")]))]
  "TARGET_XOP"
  "vpcom%Y1u<mmxvecsize>\t{%3, %2, %0|%0, %2, %3}"
  [(set_attr "type" "sse4arg")
   (set_attr "mode" "TI")])

(define_insn "*xop_maskcmp_uns<mode>3"
  [(set (match_operand:VI_16_32 0 "register_operand" "=x")
	(match_operator:VI_16_32 1 "ix86_comparison_uns_operator"
	 [(match_operand:VI_16_32 2 "register_operand" "x")
	  (match_operand:VI_16_32 3 "register_operand" "x")]))]
  "TARGET_XOP"
  "vpcom%Y1u<mmxvecsize>\t{%3, %2, %0|%0, %2, %3}"
  [(set_attr "type" "sse4arg")
   (set_attr "mode" "TI")])

(define_expand "vec_cmp<mode><mode>"
  [(set (match_operand:MMXMODEI 0 "register_operand")
	(match_operator:MMXMODEI 1 ""
	  [(match_operand:MMXMODEI 2 "register_operand")
	   (match_operand:MMXMODEI 3 "register_operand")]))]
  "TARGET_MMX_WITH_SSE"
{
  bool ok = ix86_expand_int_vec_cmp (operands);
  gcc_assert (ok);
  DONE;
})

(define_expand "vec_cmp<mode><mode>"
  [(set (match_operand:VI_16_32 0 "register_operand")
	(match_operator:VI_16_32 1 ""
	  [(match_operand:VI_16_32 2 "register_operand")
	   (match_operand:VI_16_32 3 "register_operand")]))]
  "TARGET_SSE2"
{
  bool ok = ix86_expand_int_vec_cmp (operands);
  gcc_assert (ok);
  DONE;
})

(define_expand "vec_cmpu<mode><mode>"
  [(set (match_operand:MMXMODEI 0 "register_operand")
	(match_operator:MMXMODEI 1 ""
	  [(match_operand:MMXMODEI 2 "register_operand")
	   (match_operand:MMXMODEI 3 "register_operand")]))]
  "TARGET_MMX_WITH_SSE"
{
  bool ok = ix86_expand_int_vec_cmp (operands);
  gcc_assert (ok);
  DONE;
})

(define_expand "vec_cmpu<mode><mode>"
  [(set (match_operand:VI_16_32 0 "register_operand")
	(match_operator:VI_16_32 1 ""
	  [(match_operand:VI_16_32 2 "register_operand")
	   (match_operand:VI_16_32 3 "register_operand")]))]
  "TARGET_SSE2"
{
  bool ok = ix86_expand_int_vec_cmp (operands);
  gcc_assert (ok);
  DONE;
})

(define_expand "vcond_mask_<mode><mmxintvecmodelower>"
  [(set (match_operand:MMXMODE124 0 "register_operand")
	(vec_merge:MMXMODE124
	  (match_operand:MMXMODE124 1 "register_operand")
	  (match_operand:MMXMODE124 2 "register_operand")
	  (match_operand:<mmxintvecmode> 3 "register_operand")))]
  "TARGET_MMX_WITH_SSE"
{
  ix86_expand_sse_movcc (operands[0], operands[3],
			 operands[1], operands[2]);
  DONE;
})

(define_expand "vcond_mask_<mode><mode>"
  [(set (match_operand:VI_16_32 0 "register_operand")
	(vec_merge:VI_16_32
	  (match_operand:VI_16_32 1 "register_operand")
	  (match_operand:VI_16_32 2 "register_operand")
	  (match_operand:VI_16_32 3 "register_operand")))]
  "TARGET_SSE2"
{
  ix86_expand_sse_movcc (operands[0], operands[3],
			 operands[1], operands[2]);
  DONE;
})

(define_insn "mmx_pblendvb_v8qi"
  [(set (match_operand:V8QI 0 "register_operand" "=Yr,*x,x")
	(unspec:V8QI
	  [(match_operand:V8QI 1 "register_operand" "0,0,x")
	   (match_operand:V8QI 2 "register_operand" "Yr,*x,x")
	   (match_operand:V8QI 3 "register_operand" "Yz,Yz,x")]
	  UNSPEC_BLENDV))]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE"
  "@
   pblendvb\t{%3, %2, %0|%0, %2, %3}
   pblendvb\t{%3, %2, %0|%0, %2, %3}
   vpblendvb\t{%3, %2, %1, %0|%0, %1, %2, %3}"
  [(set_attr "isa" "noavx,noavx,avx")
   (set_attr "type" "ssemov")
   (set_attr "prefix_extra" "1")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "orig,orig,vex")
   (set_attr "btver2_decode" "vector")
   (set_attr "mode" "TI")])

(define_insn_and_split "*mmx_pblendvb_v8qi_1"
  [(set (match_operand:V8QI 0 "register_operand")
       (unspec:V8QI
	  [(match_operand:V8QI 1 "register_operand")
	   (match_operand:V8QI 2 "register_operand")
	   (eq:V8QI
	     (eq:V8QI
		(match_operand:V8QI 3 "register_operand")
		(match_operand:V8QI 4 "nonmemory_operand"))
	     (match_operand:V8QI 5 "const0_operand"))]
	   UNSPEC_BLENDV))]
  "TARGET_MMX_WITH_SSE && ix86_pre_reload_split ()"
  "#"
  "&& 1"
  [(set (match_dup 6)
	(eq:V8QI (match_dup 3) (match_dup 7)))
   (set (match_dup 0)
	(unspec:V8QI
	  [(match_dup 2)
	   (match_dup 1)
	   (match_dup 6)]
	  UNSPEC_BLENDV))]
{
  operands[6] = gen_reg_rtx (V8QImode);
  operands[7] = force_reg (V8QImode, operands[4]);
})

(define_insn_and_split "*mmx_pblendvb_v8qi_2"
  [(set (match_operand:V8QI 0 "register_operand")
       (unspec:V8QI
	  [(match_operand:V8QI 1 "register_operand")
	   (match_operand:V8QI 2 "register_operand")
	   (subreg:V8QI
	     (eq:MMXMODE24
	       (eq:MMXMODE24
		 (match_operand:MMXMODE24 3 "register_operand")
		 (match_operand:MMXMODE24 4 "nonmemory_operand"))
	     (match_operand:MMXMODE24 5 "const0_operand")) 0)]
	   UNSPEC_BLENDV))]
  "TARGET_MMX_WITH_SSE && ix86_pre_reload_split ()"
  "#"
  "&& 1"
  [(set (match_dup 6)
	(eq:MMXMODE24 (match_dup 3) (match_dup 8)))
   (set (match_dup 0)
	(unspec:V8QI
	  [(match_dup 2)
	   (match_dup 1)
	   (match_dup 7)]
	  UNSPEC_BLENDV))]
{
  operands[6] = gen_reg_rtx (<MODE>mode);
  operands[7] = lowpart_subreg (V8QImode, operands[6], <MODE>mode);
  operands[8] = force_reg (<MODE>mode, operands[4]);
})

(define_insn "mmx_pblendvb_<mode>"
  [(set (match_operand:VI_16_32 0 "register_operand" "=Yr,*x,x")
	(unspec:VI_16_32
	  [(match_operand:VI_16_32 1 "register_operand" "0,0,x")
	   (match_operand:VI_16_32 2 "register_operand" "Yr,*x,x")
	   (match_operand:VI_16_32 3 "register_operand" "Yz,Yz,x")]
	  UNSPEC_BLENDV))]
  "TARGET_SSE4_1"
  "@
   pblendvb\t{%3, %2, %0|%0, %2, %3}
   pblendvb\t{%3, %2, %0|%0, %2, %3}
   vpblendvb\t{%3, %2, %1, %0|%0, %1, %2, %3}"
  [(set_attr "isa" "noavx,noavx,avx")
   (set_attr "type" "ssemov")
   (set_attr "prefix_extra" "1")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "orig,orig,vex")
   (set_attr "btver2_decode" "vector")
   (set_attr "mode" "TI")])

(define_insn_and_split "*mmx_pblendvb_<mode>_1"
  [(set (match_operand:VI_16_32 0 "register_operand")
	(unspec:VI_16_32
	  [(match_operand:VI_16_32 1 "register_operand")
	   (match_operand:VI_16_32 2 "register_operand")
	   (eq:VI_16_32
	     (eq:VI_16_32
		(match_operand:VI_16_32 3 "register_operand")
		(match_operand:VI_16_32 4 "nonmemory_operand"))
	     (match_operand:VI_16_32 5 "const0_operand"))]
	   UNSPEC_BLENDV))]
  "TARGET_SSE2 && ix86_pre_reload_split ()"
  "#"
  "&& 1"
  [(set (match_dup 6)
	(eq:VI_16_32 (match_dup 3) (match_dup 7)))
   (set (match_dup 0)
	(unspec:VI_16_32
	  [(match_dup 2)
	   (match_dup 1)
	   (match_dup 6)]
	  UNSPEC_BLENDV))]
{
  operands[6] = gen_reg_rtx (<MODE>mode);
  operands[7] = force_reg (<MODE>mode, operands[4]);
})

(define_insn_and_split "*mmx_pblendvb_v4qi_2"
  [(set (match_operand:V4QI 0 "register_operand")
       (unspec:V4QI
	  [(match_operand:V4QI 1 "register_operand")
	   (match_operand:V4QI 2 "register_operand")
	   (subreg:V4QI
	     (eq:V2HI
	       (eq:V2HI
		 (match_operand:V2HI 3 "register_operand")
		 (match_operand:V2HI 4 "nonmemory_operand"))
	     (match_operand:V2HI 5 "const0_operand")) 0)]
	   UNSPEC_BLENDV))]
  "TARGET_SSE2 && ix86_pre_reload_split ()"
  "#"
  "&& 1"
  [(set (match_dup 6)
	(eq:V2HI (match_dup 3) (match_dup 8)))
   (set (match_dup 0)
	(unspec:V4QI
	  [(match_dup 2)
	   (match_dup 1)
	   (match_dup 7)]
	  UNSPEC_BLENDV))]
{
  operands[6] = gen_reg_rtx (V2HImode);
  operands[7] = lowpart_subreg (V4QImode, operands[6], V2HImode);
  operands[8] = force_reg (V2HImode, operands[4]);
})

;; XOP parallel XMM conditional moves
(define_insn "*xop_pcmov_<mode>"
  [(set (match_operand:MMXMODE124 0 "register_operand" "=x")
	(if_then_else:MMXMODE124
	  (match_operand:MMXMODE124 3 "register_operand" "x")
	  (match_operand:MMXMODE124 1 "register_operand" "x")
	  (match_operand:MMXMODE124 2 "register_operand" "x")))]
  "TARGET_XOP && TARGET_MMX_WITH_SSE"
  "vpcmov\t{%3, %2, %1, %0|%0, %1, %2, %3}"
  [(set_attr "type" "sse4arg")
   (set_attr "mode" "TI")])

(define_insn "*xop_pcmov_<mode>"
  [(set (match_operand:V4F_64 0 "register_operand" "=x")
	(if_then_else:V4F_64
	  (match_operand:V4F_64 3 "register_operand" "x")
	  (match_operand:V4F_64 1 "register_operand" "x")
	  (match_operand:V4F_64 2 "register_operand" "x")))]
  "TARGET_XOP && TARGET_MMX_WITH_SSE"
  "vpcmov\t{%3, %2, %1, %0|%0, %1, %2, %3}"
  [(set_attr "type" "sse4arg")
   (set_attr "mode" "TI")])

(define_insn "*xop_pcmov_<mode>"
  [(set (match_operand:VI_16_32 0 "register_operand" "=x")
	(if_then_else:VI_16_32
	  (match_operand:VI_16_32 3 "register_operand" "x")
	  (match_operand:VI_16_32 1 "register_operand" "x")
	  (match_operand:VI_16_32 2 "register_operand" "x")))]
  "TARGET_XOP"
  "vpcmov\t{%3, %2, %1, %0|%0, %1, %2, %3}"
  [(set_attr "type" "sse4arg")
   (set_attr "mode" "TI")])

(define_insn "*xop_pcmov_<mode>"
  [(set (match_operand:V2F_32 0 "register_operand" "=x")
	(if_then_else:V2F_32
	  (match_operand:V2F_32 3 "register_operand" "x")
	  (match_operand:V2F_32 1 "register_operand" "x")
	  (match_operand:V2F_32 2 "register_operand" "x")))]
  "TARGET_XOP"
  "vpcmov\t{%3, %2, %1, %0|%0, %1, %2, %3}"
  [(set_attr "type" "sse4arg")
   (set_attr "mode" "TI")])

;; XOP permute instructions
(define_insn "mmx_ppermv64"
  [(set (match_operand:V8QI 0 "register_operand" "=x")
	(unspec:V8QI
	  [(match_operand:V8QI 1 "register_operand" "x")
	   (match_operand:V8QI 2 "register_operand" "x")
	   (match_operand:V16QI 3 "nonimmediate_operand" "xm")]
	  UNSPEC_XOP_PERMUTE))]
  "TARGET_XOP && TARGET_MMX_WITH_SSE"
  "vpperm\t{%3, %2, %1, %0|%0, %1, %2, %3}"
  [(set_attr "type" "sse4arg")
   (set_attr "mode" "TI")])

(define_insn "mmx_ppermv32"
  [(set (match_operand:V4QI 0 "register_operand" "=x")
	(unspec:V4QI
	  [(match_operand:V4QI 1 "register_operand" "x")
	   (match_operand:V4QI 2 "register_operand" "x")
	   (match_operand:V16QI 3 "nonimmediate_operand" "xm")]
	  UNSPEC_XOP_PERMUTE))]
  "TARGET_XOP"
  "vpperm\t{%3, %2, %1, %0|%0, %1, %2, %3}"
  [(set_attr "type" "sse4arg")
   (set_attr "mode" "TI")])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel integral logical operations
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_expand "one_cmpl<mode>2"
  [(set (match_operand:MMXMODEI 0 "register_operand")
	(xor:MMXMODEI
	  (match_operand:MMXMODEI 1 "register_operand")
	  (match_dup 2)))]
  "TARGET_MMX_WITH_SSE"
  "operands[2] = force_reg (<MODE>mode, CONSTM1_RTX (<MODE>mode));")

(define_insn "one_cmpl<mode>2"
  [(set (match_operand:VI_16_32 0 "register_operand" "=?r,&x,&v")
	(not:VI_16_32
	  (match_operand:VI_16_32 1 "register_operand" "0,x,v")))]
  ""
  "#"
  [(set_attr "isa" "*,sse2,avx512vl")
   (set_attr "type" "negnot,sselog1,sselog1")
   (set_attr "mode" "SI,TI,TI")])

(define_split
  [(set (match_operand:VI_16_32 0 "general_reg_operand")
	(not:VI_16_32
	  (match_operand:VI_16_32 1 "general_reg_operand")))]
  "reload_completed"
  [(set (match_dup 0)
	(not:SI (match_dup 1)))]
{
  operands[1] = lowpart_subreg (SImode, operands[1], <MODE>mode);
  operands[0] = lowpart_subreg (SImode, operands[0], <MODE>mode);
})

(define_split
  [(set (match_operand:VI_16_32 0 "sse_reg_operand")
	(not:VI_16_32
	  (match_operand:VI_16_32 1 "sse_reg_operand")))]
  "TARGET_SSE2 && reload_completed"
  [(set (match_dup 0) (match_dup 2))
   (set (match_dup 0)
	(xor:V16QI
	  (match_dup 0) (match_dup 1)))]
{
  operands[2] = CONSTM1_RTX (V16QImode);
  operands[1] = lowpart_subreg (V16QImode, operands[1], <MODE>mode);
  operands[0] = lowpart_subreg (V16QImode, operands[0], <MODE>mode);
})

(define_expand "andn<mode>3"
  [(set (match_operand:MMXMODEI 0 "register_operand")
	(and:MMXMODEI
	  (not:MMXMODEI (match_operand:MMXMODEI 2 "register_operand"))
	  (match_operand:MMXMODEI 1 "register_operand")))]
  "TARGET_MMX_WITH_SSE")

(define_insn "mmx_andnot<mode>3"
  [(set (match_operand:MMXMODEI 0 "register_operand" "=y,x,x,v")
	(and:MMXMODEI
	  (not:MMXMODEI (match_operand:MMXMODEI 1 "register_operand" "0,0,x,v"))
	  (match_operand:MMXMODEI 2 "register_mmxmem_operand" "ym,x,x,v")))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "@
   pandn\t{%2, %0|%0, %2}
   pandn\t{%2, %0|%0, %2}
   vpandn\t{%2, %1, %0|%0, %1, %2}
   vpandnd\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse2_noavx,avx,avx512vl")
   (set_attr "mmx_isa" "native,*,*,*")
   (set_attr "type" "mmxadd,sselog,sselog,sselog")
   (set_attr "mode" "DI,TI,TI,TI")])

(define_insn "*andnot<mode>3"
  [(set (match_operand:VI_16_32 0 "register_operand" "=?&r,?r,x,x,v")
	(and:VI_16_32
	  (not:VI_16_32
	    (match_operand:VI_16_32 1 "register_operand" "0,r,0,x,v"))
	  (match_operand:VI_16_32 2 "register_operand" "r,r,x,x,v")))
   (clobber (reg:CC FLAGS_REG))]
  ""
  "#"
  [(set_attr "isa" "*,bmi,sse2_noavx,avx,avx512vl")
   (set_attr "type" "alu,bitmanip,sselog,sselog,sselog")
   (set_attr "mode" "SI,SI,TI,TI,TI")])

(define_split
  [(set (match_operand:VI_16_32 0 "general_reg_operand")
	(and:VI_16_32
	  (not:VI_16_32 (match_operand:VI_16_32 1 "general_reg_operand"))
	  (match_operand:VI_16_32 2 "general_reg_operand")))
   (clobber (reg:CC FLAGS_REG))]
  "TARGET_BMI && reload_completed"
  [(parallel
     [(set (match_dup 0)
	   (and:SI (not:SI (match_dup 1)) (match_dup 2)))
      (clobber (reg:CC FLAGS_REG))])]
{
  operands[2] = lowpart_subreg (SImode, operands[2], <MODE>mode);
  operands[1] = lowpart_subreg (SImode, operands[1], <MODE>mode);
  operands[0] = lowpart_subreg (SImode, operands[0], <MODE>mode);
})

(define_split
  [(set (match_operand:VI_16_32 0 "general_reg_operand")
	(and:VI_16_32
	  (not:VI_16_32 (match_operand:VI_16_32 1 "general_reg_operand"))
	  (match_operand:VI_16_32 2 "general_reg_operand")))
   (clobber (reg:CC FLAGS_REG))]
  "!TARGET_BMI && reload_completed"
  [(set (match_dup 0)
	(not:SI (match_dup 1)))
   (parallel
     [(set (match_dup 0)
	   (and:SI (match_dup 0) (match_dup 2)))
      (clobber (reg:CC FLAGS_REG))])]
{
  operands[2] = lowpart_subreg (SImode, operands[2], <MODE>mode);
  operands[1] = lowpart_subreg (SImode, operands[1], <MODE>mode);
  operands[0] = lowpart_subreg (SImode, operands[0], <MODE>mode);
})

(define_split
  [(set (match_operand:VI_16_32 0 "sse_reg_operand")
	(and:VI_16_32
	  (not:VI_16_32 (match_operand:VI_16_32 1 "sse_reg_operand"))
	  (match_operand:VI_16_32 2 "sse_reg_operand")))
   (clobber (reg:CC FLAGS_REG))]
  "TARGET_SSE2 && reload_completed"
  [(set (match_dup 0)
	(and:V16QI (not:V16QI (match_dup 1)) (match_dup 2)))]
{
  operands[2] = lowpart_subreg (V16QImode, operands[2], <MODE>mode);
  operands[1] = lowpart_subreg (V16QImode, operands[1], <MODE>mode);
  operands[0] = lowpart_subreg (V16QImode, operands[0], <MODE>mode);
})

(define_expand "mmx_<code><mode>3"
  [(set (match_operand:MMXMODEI 0 "register_operand")
	(any_logic:MMXMODEI
	  (match_operand:MMXMODEI 1 "register_mmxmem_operand")
	  (match_operand:MMXMODEI 2 "register_mmxmem_operand")))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "ix86_fixup_binary_operands_no_copy (<CODE>, <MODE>mode, operands);")

(define_expand "<code><mode>3"
  [(set (match_operand:MMXMODEI 0 "register_operand")
	(any_logic:MMXMODEI
	  (match_operand:MMXMODEI 1 "register_operand")
	  (match_operand:MMXMODEI 2 "register_operand")))]
  "TARGET_MMX_WITH_SSE")

(define_insn "*mmx_<code><mode>3"
  [(set (match_operand:MMXMODEI 0 "register_operand" "=y,x,x,v")
	(any_logic:MMXMODEI
	  (match_operand:MMXMODEI 1 "register_mmxmem_operand" "%0,0,x,v")
	  (match_operand:MMXMODEI 2 "register_mmxmem_operand" "ym,x,x,v")))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && ix86_binary_operator_ok (<CODE>, <MODE>mode, operands)"
  "@
   p<logic>\t{%2, %0|%0, %2}
   p<logic>\t{%2, %0|%0, %2}
   vp<logic>\t{%2, %1, %0|%0, %1, %2}
   vp<logic>d\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse2_noavx,avx,avx512vl")
   (set_attr "mmx_isa" "native,*,*,*")
   (set_attr "type" "mmxadd,sselog,sselog,sselog")
   (set_attr "mode" "DI,TI,TI,TI")])

(define_expand "<code><mode>3"
  [(set (match_operand:VI_16_32 0 "nonimmediate_operand")
	(any_logic:VI_16_32
	  (match_operand:VI_16_32 1 "nonimmediate_operand")
	  (match_operand:VI_16_32 2 "nonimmediate_or_x86_64_const_vector_operand")))]
  ""
  "ix86_expand_binary_operator (<CODE>, <MODE>mode, operands); DONE;")

(define_insn "*<code><mode>3"
  [(set (match_operand:VI_16_32 0 "nonimmediate_operand" "=?r,m,x,x,v")
	(any_logic:VI_16_32
	  (match_operand:VI_16_32 1 "nonimmediate_operand" "%0,0,0,x,v")
	  (match_operand:VI_16_32 2 "nonimmediate_or_x86_64_const_vector_operand" "r,i,x,x,v")))
   (clobber (reg:CC FLAGS_REG))]
  "ix86_binary_operator_ok (<CODE>, <MODE>mode, operands)"
  "#"
  [(set_attr "isa" "*,*,sse2_noavx,avx,avx512vl")
   (set_attr "type" "alu,alu,sselog,sselog,sselog")
   (set_attr "mode" "SI,SI,TI,TI,TI")])

(define_split
  [(set (match_operand:VI_16_32 0 "nonimmediate_gr_operand")
	(any_logic:VI_16_32
	  (match_operand:VI_16_32 1 "nonimmediate_gr_operand")
	  (match_operand:VI_16_32 2 "reg_or_const_vector_operand")))
   (clobber (reg:CC FLAGS_REG))]
  "reload_completed"
  [(parallel
     [(set (match_dup 0)
	   (any_logic:<mmxinsnmode> (match_dup 1) (match_dup 2)))
      (clobber (reg:CC FLAGS_REG))])]
{
  if (!register_operand (operands[2], <MODE>mode))
    {
      HOST_WIDE_INT val = ix86_convert_const_vector_to_integer (operands[2],
								<MODE>mode);
      operands[2] = GEN_INT (val);
    }
  else
    operands[2] = lowpart_subreg (<mmxinsnmode>mode, operands[2], <MODE>mode);
  operands[1] = lowpart_subreg (<mmxinsnmode>mode, operands[1], <MODE>mode);
  operands[0] = lowpart_subreg (<mmxinsnmode>mode, operands[0], <MODE>mode);
})

(define_split
  [(set (match_operand:VI_16_32 0 "sse_reg_operand")
	(any_logic:VI_16_32
	  (match_operand:VI_16_32 1 "sse_reg_operand")
	  (match_operand:VI_16_32 2 "sse_reg_operand")))
   (clobber (reg:CC FLAGS_REG))]
  "TARGET_SSE2 && reload_completed"
  [(set (match_dup 0)
	(any_logic:V16QI (match_dup 1) (match_dup 2)))]
{
  operands[2] = lowpart_subreg (V16QImode, operands[2], <MODE>mode);
  operands[1] = lowpart_subreg (V16QImode, operands[1], <MODE>mode);
  operands[0] = lowpart_subreg (V16QImode, operands[0], <MODE>mode);
})

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Parallel integral element swizzling
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_insn_and_split "mmx_packsswb"
  [(set (match_operand:V8QI 0 "register_operand" "=y,x,Yw")
	(vec_concat:V8QI
	  (ss_truncate:V4QI
	    (match_operand:V4HI 1 "register_operand" "0,0,Yw"))
	  (ss_truncate:V4QI
	    (match_operand:V4HI 2 "register_mmxmem_operand" "ym,x,Yw"))))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "@
   packsswb\t{%2, %0|%0, %2}
   #
   #"
  "&& reload_completed
   && SSE_REGNO_P (REGNO (operands[0]))"
  [(const_int 0)]
  "ix86_split_mmx_pack (operands, SS_TRUNCATE); DONE;"
  [(set_attr "mmx_isa" "native,sse_noavx,avx")
   (set_attr "type" "mmxshft,sselog,sselog")
   (set_attr "mode" "DI,TI,TI")])

;; This instruction does unsigned saturation of signed source
;; and is different from generic us_truncate RTX.
(define_insn_and_split "mmx_packuswb"
  [(set (match_operand:V8QI 0 "register_operand" "=y,x,Yw")
	(unspec:V8QI
	  [(match_operand:V4HI 1 "register_operand" "0,0,Yw")
	   (match_operand:V4HI 2 "register_mmxmem_operand" "ym,x,Yw")]
	  UNSPEC_US_TRUNCATE))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "@
   packuswb\t{%2, %0|%0, %2}
   #
   #"
  "&& reload_completed
   && SSE_REGNO_P (REGNO (operands[0]))"
  [(const_int 0)]
  "ix86_split_mmx_pack (operands, US_TRUNCATE); DONE;"
  [(set_attr "mmx_isa" "native,sse_noavx,avx")
   (set_attr "type" "mmxshft,sselog,sselog")
   (set_attr "mode" "DI,TI,TI")])

(define_insn_and_split "mmx_packssdw"
  [(set (match_operand:V4HI 0 "register_operand" "=y,x,Yw")
	(vec_concat:V4HI
	  (ss_truncate:V2HI
	    (match_operand:V2SI 1 "register_operand" "0,0,Yw"))
	  (ss_truncate:V2HI
	    (match_operand:V2SI 2 "register_mmxmem_operand" "ym,x,Yw"))))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "@
   packssdw\t{%2, %0|%0, %2}
   #
   #"
  "&& reload_completed
   && SSE_REGNO_P (REGNO (operands[0]))"
  [(const_int 0)]
  "ix86_split_mmx_pack (operands, SS_TRUNCATE); DONE;"
  [(set_attr "mmx_isa" "native,sse_noavx,avx")
   (set_attr "type" "mmxshft,sselog,sselog")
   (set_attr "mode" "DI,TI,TI")])

(define_insn_and_split "mmx_packusdw"
  [(set (match_operand:V4HI 0 "register_operand" "=Yr,*x,Yw")
	(unspec:V4HI
	  [(match_operand:V2SI 1 "register_operand" "0,0,Yw")
	   (match_operand:V2SI 2 "register_operand" "Yr,*x,Yw")]
	   UNSPEC_US_TRUNCATE))]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE"
  "#"
  "&& reload_completed"
  [(const_int 0)]
  "ix86_split_mmx_pack (operands, US_TRUNCATE); DONE;"
  [(set_attr "isa" "noavx,noavx,avx")
   (set_attr "type" "sselog")
   (set_attr "mode" "TI")])

(define_insn_and_split "mmx_punpckhbw"
  [(set (match_operand:V8QI 0 "register_operand" "=y,x,Yw")
	(vec_select:V8QI
	  (vec_concat:V16QI
	    (match_operand:V8QI 1 "register_operand" "0,0,Yw")
	    (match_operand:V8QI 2 "register_mmxmem_operand" "ym,x,Yw"))
	  (parallel [(const_int 4) (const_int 12)
		     (const_int 5) (const_int 13)
		     (const_int 6) (const_int 14)
		     (const_int 7) (const_int 15)])))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "@
   punpckhbw\t{%2, %0|%0, %2}
   #
   #"
  "&& reload_completed
   && SSE_REGNO_P (REGNO (operands[0]))"
  [(const_int 0)]
  "ix86_split_mmx_punpck (operands, true); DONE;"
  [(set_attr "mmx_isa" "native,sse_noavx,avx")
   (set_attr "type" "mmxcvt,sselog,sselog")
   (set_attr "mode" "DI,TI,TI")])

(define_insn_and_split "mmx_punpckhbw_low"
  [(set (match_operand:V4QI 0 "register_operand" "=x,Yw")
	(vec_select:V4QI
	  (vec_concat:V8QI
	    (match_operand:V4QI 1 "register_operand" "0,Yw")
	    (match_operand:V4QI 2 "register_operand" "x,Yw"))
	  (parallel [(const_int 2) (const_int 6)
		     (const_int 3) (const_int 7)])))]
  "TARGET_SSE2"
  "#"
  "&& reload_completed"
  [(const_int 0)]
  "ix86_split_mmx_punpck (operands, true); DONE;"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sselog")
   (set_attr "mode" "TI")])

(define_insn_and_split "mmx_punpcklbw"
  [(set (match_operand:V8QI 0 "register_operand" "=y,x,Yw")
	(vec_select:V8QI
	  (vec_concat:V16QI
	    (match_operand:V8QI 1 "register_operand" "0,0,Yw")
	    (match_operand:V8QI 2 "register_mmxmem_operand" "ym,x,Yw"))
	  (parallel [(const_int 0) (const_int 8)
		     (const_int 1) (const_int 9)
		     (const_int 2) (const_int 10)
		     (const_int 3) (const_int 11)])))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "@
   punpcklbw\t{%2, %0|%0, %k2}
   #
   #"
  "&& reload_completed
   && SSE_REGNO_P (REGNO (operands[0]))"
  [(const_int 0)]
  "ix86_split_mmx_punpck (operands, false); DONE;"
  [(set_attr "mmx_isa" "native,sse_noavx,avx")
   (set_attr "type" "mmxcvt,sselog,sselog")
   (set_attr "mode" "DI,TI,TI")])

(define_insn_and_split "mmx_punpcklbw_low"
  [(set (match_operand:V4QI 0 "register_operand" "=x,Yw")
	(vec_select:V4QI
	  (vec_concat:V8QI
	    (match_operand:V4QI 1 "register_operand" "0,Yw")
	    (match_operand:V4QI 2 "register_operand" "x,Yw"))
	  (parallel [(const_int 0) (const_int 4)
		     (const_int 1) (const_int 5)])))]
  "TARGET_SSE2"
  "#"
  "&& reload_completed"
  [(const_int 0)]
  "ix86_split_mmx_punpck (operands, false); DONE;"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sselog")
   (set_attr "mode" "TI")])

(define_insn_and_split "mmx_punpckhwd"
  [(set (match_operand:V4HI 0 "register_operand" "=y,x,Yw")
	(vec_select:V4HI
	  (vec_concat:V8HI
	    (match_operand:V4HI 1 "register_operand" "0,0,Yw")
	    (match_operand:V4HI 2 "register_mmxmem_operand" "ym,x,Yw"))
	  (parallel [(const_int 2) (const_int 6)
		     (const_int 3) (const_int 7)])))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "@
   punpckhwd\t{%2, %0|%0, %2}
   #
   #"
  "&& reload_completed
   && SSE_REGNO_P (REGNO (operands[0]))"
  [(const_int 0)]
  "ix86_split_mmx_punpck (operands, true); DONE;"
  [(set_attr "mmx_isa" "native,sse_noavx,avx")
   (set_attr "type" "mmxcvt,sselog,sselog")
   (set_attr "mode" "DI,TI,TI")])

(define_insn_and_split "mmx_punpcklwd"
  [(set (match_operand:V4HI 0 "register_operand" "=y,x,Yw")
	(vec_select:V4HI
	  (vec_concat:V8HI
	    (match_operand:V4HI 1 "register_operand" "0,0,Yw")
	    (match_operand:V4HI 2 "register_mmxmem_operand" "ym,x,Yw"))
	  (parallel [(const_int 0) (const_int 4)
		     (const_int 1) (const_int 5)])))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "@
   punpcklwd\t{%2, %0|%0, %k2}
   #
   #"
  "&& reload_completed
   && SSE_REGNO_P (REGNO (operands[0]))"
  [(const_int 0)]
  "ix86_split_mmx_punpck (operands, false); DONE;"
  [(set_attr "mmx_isa" "native,sse_noavx,avx")
   (set_attr "type" "mmxcvt,sselog,sselog")
   (set_attr "mode" "DI,TI,TI")])

(define_insn_and_split "mmx_punpckhdq"
  [(set (match_operand:V2SI 0 "register_operand" "=y,x,Yv")
	(vec_select:V2SI
	  (vec_concat:V4SI
	    (match_operand:V2SI 1 "register_operand" "0,0,Yv")
	    (match_operand:V2SI 2 "register_mmxmem_operand" "ym,x,Yv"))
	  (parallel [(const_int 1)
		     (const_int 3)])))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "@
   punpckhdq\t{%2, %0|%0, %2}
   #
   #"
  "&& reload_completed
   && SSE_REGNO_P (REGNO (operands[0]))"
  [(const_int 0)]
  "ix86_split_mmx_punpck (operands, true); DONE;"
  [(set_attr "mmx_isa" "native,sse_noavx,avx")
   (set_attr "type" "mmxcvt,sselog,sselog")
   (set_attr "mode" "DI,TI,TI")])

(define_insn_and_split "mmx_punpckldq"
  [(set (match_operand:V2SI 0 "register_operand" "=y,x,Yv")
	(vec_select:V2SI
	  (vec_concat:V4SI
	    (match_operand:V2SI 1 "register_operand" "0,0,Yv")
	    (match_operand:V2SI 2 "register_mmxmem_operand" "ym,x,Yv"))
	  (parallel [(const_int 0)
		     (const_int 2)])))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "@
   punpckldq\t{%2, %0|%0, %k2}
   #
   #"
  "&& reload_completed
   && SSE_REGNO_P (REGNO (operands[0]))"
  [(const_int 0)]
  "ix86_split_mmx_punpck (operands, false); DONE;"
  [(set_attr "mmx_isa" "native,sse_noavx,avx")
   (set_attr "type" "mmxcvt,sselog,sselog")
   (set_attr "mode" "DI,TI,TI")])

(define_insn "sse4_1_<code>v4qiv4hi2"
  [(set (match_operand:V4HI 0 "register_operand" "=Yr,*x,Yw")
	(any_extend:V4HI
	  (vec_select:V4QI
	    (match_operand:V8QI 1 "register_operand" "Yr,*x,Yw")
	    (parallel [(const_int 0) (const_int 1)
		       (const_int 2) (const_int 3)]))))]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE"
  "%vpmov<extsuffix>bw\t{%1, %0|%0, %1}"
  [(set_attr "isa" "noavx,noavx,avx")
   (set_attr "type" "ssemov")
   (set_attr "prefix_extra" "1")
   (set_attr "prefix" "orig,orig,maybe_evex")
   (set_attr "mode" "TI")])

(define_expand "<insn>v4qiv4hi2"
  [(set (match_operand:V4HI 0 "register_operand")
	(any_extend:V4HI
	  (match_operand:V4QI 1 "register_operand")))]
  "TARGET_MMX_WITH_SSE"
{
  if (!TARGET_SSE4_1)
    {
      ix86_expand_sse_extend (operands[0], operands[1], <u_bool>);
      DONE;
    }

  rtx op1 = force_reg (V4QImode, operands[1]);
  op1 = lowpart_subreg (V8QImode, op1, V4QImode);
  emit_insn (gen_sse4_1_<code>v4qiv4hi2 (operands[0], op1));
  DONE;
})

(define_insn "sse4_1_<code>v2hiv2si2"
  [(set (match_operand:V2SI 0 "register_operand" "=Yr,*x,v")
	(any_extend:V2SI
	  (vec_select:V2HI
	    (match_operand:V4HI 1 "register_operand" "Yr,*x,v")
	    (parallel [(const_int 0) (const_int 1)]))))]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE"
  "%vpmov<extsuffix>wd\t{%1, %0|%0, %1}"
  [(set_attr "isa" "noavx,noavx,avx")
   (set_attr "type" "ssemov")
   (set_attr "prefix_extra" "1")
   (set_attr "prefix" "orig,orig,maybe_evex")
   (set_attr "mode" "TI")])

(define_expand "<insn>v2hiv2si2"
  [(set (match_operand:V2SI 0 "register_operand")
	(any_extend:V2SI
	  (match_operand:V2HI 1 "register_operand")))]
  "TARGET_MMX_WITH_SSE"
{
  if (!TARGET_SSE4_1)
    {
      ix86_expand_sse_extend (operands[0], operands[1], <u_bool>);
      DONE;
    }

  rtx op1 = force_reg (V2HImode, operands[1]);
  op1 = lowpart_subreg (V4HImode, op1, V2HImode);
  emit_insn (gen_sse4_1_<code>v2hiv2si2 (operands[0], op1));
  DONE;
})

(define_insn "sse4_1_<code>v2qiv2si2"
  [(set (match_operand:V2SI 0 "register_operand" "=Yr,*x,v")
	(any_extend:V2SI
	  (vec_select:V2QI
	    (match_operand:V4QI 1 "register_operand" "Yr,*x,v")
	    (parallel [(const_int 0) (const_int 1)]))))]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE"
  "%vpmov<extsuffix>bd\t{%1, %0|%0, %1}"
  [(set_attr "isa" "noavx,noavx,avx")
   (set_attr "type" "ssemov")
   (set_attr "prefix_extra" "1")
   (set_attr "prefix" "orig,orig,maybe_evex")
   (set_attr "mode" "TI")])

(define_expand "<insn>v2qiv2si2"
  [(set (match_operand:V2SI 0 "register_operand")
	(any_extend:V2SI
	  (match_operand:V2QI 1 "register_operand")))]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE"
{
  rtx op1 = force_reg (V2QImode, operands[1]);
  op1 = lowpart_subreg (V4QImode, op1, V2QImode);
  emit_insn (gen_sse4_1_<code>v2qiv2si2 (operands[0], op1));
  DONE;
})

(define_insn "sse4_1_<code>v2qiv2hi2"
  [(set (match_operand:V2HI 0 "register_operand" "=Yr,*x,Yw")
	(any_extend:V2HI
	  (vec_select:V2QI
	    (match_operand:V4QI 1 "register_operand" "Yr,*x,Yw")
	    (parallel [(const_int 0) (const_int 1)]))))]
  "TARGET_SSE4_1"
  "%vpmov<extsuffix>bw\t{%1, %0|%0, %1}"
  [(set_attr "isa" "noavx,noavx,avx")
   (set_attr "type" "ssemov")
   (set_attr "prefix_extra" "1")
   (set_attr "prefix" "orig,orig,maybe_evex")
   (set_attr "mode" "TI")])

(define_expand "<insn>v2qiv2hi2"
  [(set (match_operand:V2HI 0 "register_operand")
	(any_extend:V2HI
	  (match_operand:V2QI 1 "register_operand")))]
  "TARGET_SSE2"
{
  if (!TARGET_SSE4_1)
    {
      ix86_expand_sse_extend (operands[0], operands[1], <u_bool>);
      DONE;
    }

  rtx op1 = force_reg (V2QImode, operands[1]);
  op1 = lowpart_subreg (V4QImode, op1, V2QImode);
  emit_insn (gen_sse4_1_<code>v2qiv2hi2 (operands[0], op1));
  DONE;
})

(define_expand "trunc<mode><mmxhalfmodelower>2"
  [(set (match_operand:<mmxhalfmode> 0 "register_operand")
	(truncate:<mmxhalfmode>
	  (match_operand:VI2_32_64 1 "register_operand")))]
  "TARGET_AVX2"
{
  if (TARGET_AVX512VL && TARGET_AVX512BW)
    emit_insn (gen_avx512vl_trunc<mode><mmxhalfmodelower>2 (operands[0], operands[1]));
  else
    ix86_expand_trunc_with_avx2_noavx512f (operands[0], operands[1], <mmxbytemode>mode);
  DONE;
})

(define_insn "avx512vl_trunc<mode><mmxhalfmodelower>2"
  [(set (match_operand:<mmxhalfmode> 0 "register_operand" "=v")
	(truncate:<mmxhalfmode>
	  (match_operand:VI2_32_64 1 "register_operand" "v")))]
  "TARGET_AVX512VL && TARGET_AVX512BW"
  "vpmovwb\t{%1, %0|%0, %1}"
  [(set_attr "type" "ssemov")
   (set_attr "prefix" "evex")
   (set_attr "mode" "TI")])

(define_mode_iterator V2QI_V2HI [V2QI V2HI])
(define_mode_attr v2qi_quad_v2hi_double
  [(V2QI "V8QI") (V2HI "V4HI")])
(define_expand "truncv2si<mode>2"
  [(set (match_operand:V2QI_V2HI 0 "register_operand")
	(truncate:V2QI_V2HI
	  (match_operand:V2SI 1 "register_operand")))]
  "TARGET_AVX2 && TARGET_MMX_WITH_SSE"
{
  if (TARGET_AVX512VL)
    emit_insn (gen_avx512vl_truncv2si<mode>2 (operands[0], operands[1]));
  else
    ix86_expand_trunc_with_avx2_noavx512f (operands[0], operands[1], <v2qi_quad_v2hi_double>mode);
  DONE;
})

(define_insn "avx512vl_truncv2si<mode>2"
  [(set (match_operand:V2QI_V2HI 0 "register_operand" "=v")
	(truncate:V2QI_V2HI
	  (match_operand:V2SI 1 "register_operand" "v")))]
  "TARGET_AVX512VL && TARGET_MMX_WITH_SSE"
  "vpmovd<mmxvecsize>\t{%1, %0|%0, %1}"
  [(set_attr "type" "ssemov")
   (set_attr "prefix" "evex")
   (set_attr "mode" "TI")])

;; Pack/unpack vector modes
(define_mode_attr mmxpackmode
  [(V4HI "V8QI") (V2SI "V4HI")])

(define_expand "vec_pack_trunc_<mode>"
  [(match_operand:<mmxpackmode> 0 "register_operand")
   (match_operand:MMXMODE24 1 "register_operand")
   (match_operand:MMXMODE24 2 "register_operand")]
  "TARGET_MMX_WITH_SSE"
{
  rtx op1 = gen_lowpart (<mmxpackmode>mode, operands[1]);
  rtx op2 = gen_lowpart (<mmxpackmode>mode, operands[2]);
  ix86_expand_vec_extract_even_odd (operands[0], op1, op2, 0);
  DONE;
})

(define_expand "vec_pack_trunc_v2hi"
  [(match_operand:V4QI 0 "register_operand")
   (match_operand:V2HI 1 "register_operand")
   (match_operand:V2HI 2 "register_operand")]
  "TARGET_SSE2"
{
  rtx op1 = gen_lowpart (V4QImode, operands[1]);
  rtx op2 = gen_lowpart (V4QImode, operands[2]);
  ix86_expand_vec_extract_even_odd (operands[0], op1, op2, 0);
  DONE;
})

(define_mode_attr mmxunpackmode
  [(V8QI "V4HI") (V4HI "V2SI")])

(define_expand "vec_unpacks_lo_<mode>"
  [(match_operand:<mmxunpackmode> 0 "register_operand")
   (match_operand:MMXMODE12 1 "register_operand")]
  "TARGET_MMX_WITH_SSE"
  "ix86_expand_sse_unpack (operands[0], operands[1], false, false); DONE;")

(define_expand "vec_unpacks_hi_<mode>"
  [(match_operand:<mmxunpackmode> 0 "register_operand")
   (match_operand:MMXMODE12 1 "register_operand")]
  "TARGET_MMX_WITH_SSE"
  "ix86_expand_sse_unpack (operands[0], operands[1], false, true); DONE;")

(define_expand "vec_unpacku_lo_<mode>"
  [(match_operand:<mmxunpackmode> 0 "register_operand")
   (match_operand:MMXMODE12 1 "register_operand")]
  "TARGET_MMX_WITH_SSE"
  "ix86_expand_sse_unpack (operands[0], operands[1], true, false); DONE;")

(define_expand "vec_unpacku_hi_<mode>"
  [(match_operand:<mmxunpackmode> 0 "register_operand")
   (match_operand:MMXMODE12 1 "register_operand")]
  "TARGET_MMX_WITH_SSE"
  "ix86_expand_sse_unpack (operands[0], operands[1], true, true); DONE;")

(define_expand "vec_unpacks_lo_v4qi"
  [(match_operand:V2HI 0 "register_operand")
   (match_operand:V4QI 1 "register_operand")]
  "TARGET_SSE2"
  "ix86_expand_sse_unpack (operands[0], operands[1], false, false); DONE;")

(define_expand "vec_unpacks_hi_v4qi"
  [(match_operand:V2HI 0 "register_operand")
   (match_operand:V4QI 1 "register_operand")]
  "TARGET_SSE2"
  "ix86_expand_sse_unpack (operands[0], operands[1], false, true); DONE;")

(define_expand "vec_unpacku_lo_v4qi"
  [(match_operand:V2HI 0 "register_operand")
   (match_operand:V4QI 1 "register_operand")]
  "TARGET_SSE2"
  "ix86_expand_sse_unpack (operands[0], operands[1], true, false); DONE;")

(define_expand "vec_unpacku_hi_v4qi"
  [(match_operand:V2HI 0 "register_operand")
   (match_operand:V4QI 1 "register_operand")]
  "TARGET_SSE2"
  "ix86_expand_sse_unpack (operands[0], operands[1], true, true); DONE;")

(define_insn "*mmx_pinsrd"
  [(set (match_operand:V2SI 0 "register_operand" "=x,Yv")
	(vec_merge:V2SI
	  (vec_duplicate:V2SI
	    (match_operand:SI 2 "nonimmediate_operand" "jrjm,rm"))
	  (match_operand:V2SI 1 "register_operand" "0,Yv")
	  (match_operand:SI 3 "const_int_operand")))]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE
   && ((unsigned) exact_log2 (INTVAL (operands[3]))
       < GET_MODE_NUNITS (V2SImode))"
{
  operands[3] = GEN_INT (exact_log2 (INTVAL (operands[3])));
  switch (which_alternative)
    {
    case 1:
      return "vpinsrd\t{%3, %2, %1, %0|%0, %1, %2, %3}";
    case 0:
      return "pinsrd\t{%3, %2, %0|%0, %2, %3}";
    default:
      gcc_unreachable ();
    }
}
  [(set_attr "isa" "noavx,avx")
   (set_attr "addr" "gpr16,*")
   (set_attr "prefix_extra" "1")
   (set_attr "type" "sselog")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "orig,vex")
   (set_attr "mode" "TI")])

(define_insn "*mmx_pinsrw"
  [(set (match_operand:V4FI_64 0 "register_operand" "=y,x,YW,&x")
	(vec_merge:V4FI_64
	  (vec_duplicate:V4FI_64
	    (match_operand:<mmxscalarmode> 2 "nonimmediate_operand" "rm,rm,rm,x"))
	  (match_operand:V4FI_64 1 "register_operand" "0,0,YW,x")
	  (match_operand:SI 3 "const_int_operand")))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && (TARGET_SSE || TARGET_3DNOW_A)
   && ((unsigned) exact_log2 (INTVAL (operands[3]))
       < GET_MODE_NUNITS (V4HImode))"
{
  operands[3] = GEN_INT (exact_log2 (INTVAL (operands[3])));
  switch (which_alternative)
    {
    case 3:
      return "#";
    case 2:
      if (MEM_P (operands[2]))
	return "vpinsrw\t{%3, %2, %1, %0|%0, %1, %2, %3}";
      else
	return "vpinsrw\t{%3, %k2, %1, %0|%0, %1, %k2, %3}";
    case 1:
    case 0:
      if (MEM_P (operands[2]))
	return "pinsrw\t{%3, %2, %0|%0, %2, %3}";
      else
	return "pinsrw\t{%3, %k2, %0|%0, %k2, %3}";
    default:
      gcc_unreachable ();
    }
}
  [(set_attr "isa" "*,sse2_noavx,avx,sse4")
   (set_attr "mmx_isa" "native,*,*,*")
   (set_attr "type" "mmxcvt,sselog,sselog,sselog")
   (set_attr "length_immediate" "1")
   (set_attr "mode" "DI,TI,TI,TI")])

;; For TARGET_SSE2, implement insert from XMM reg with PSHULFW + PBLENDW.
(define_split
  [(set (match_operand:V4FI_64 0 "sse_reg_operand")
	(vec_merge:V4FI_64
	  (vec_duplicate:V4FI_64
	    (match_operand:<mmxscalarmode> 2 "sse_reg_operand"))
	  (match_operand:V4FI_64 1 "sse_reg_operand")
	  (match_operand:SI 3 "const_int_operand")))]
  "TARGET_MMX_WITH_SSE && TARGET_SSE4_1 && reload_completed
   && ((unsigned) exact_log2 (INTVAL (operands[3]))
       < GET_MODE_NUNITS (<MODE>mode))"
  [(set (match_dup 0)
	(vec_duplicate:V4FI_64 (match_dup 2)))
   (set (match_dup 0)
	(vec_merge:V4FI_64 (match_dup 1) (match_dup 0) (match_dup 3)))]
  "operands[3] = GEN_INT (~INTVAL (operands[3]) & 0xf);")

(define_insn "*mmx_pinsrb"
  [(set (match_operand:V8QI 0 "register_operand" "=x,YW")
	(vec_merge:V8QI
	  (vec_duplicate:V8QI
	    (match_operand:QI 2 "nonimmediate_operand" "jrjm,rm"))
	  (match_operand:V8QI 1 "register_operand" "0,YW")
	  (match_operand:SI 3 "const_int_operand")))]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE
   && ((unsigned) exact_log2 (INTVAL (operands[3]))
       < GET_MODE_NUNITS (V8QImode))"
{
  operands[3] = GEN_INT (exact_log2 (INTVAL (operands[3])));
  switch (which_alternative)
    {
    case 1:
      if (MEM_P (operands[2]))
	return "vpinsrb\t{%3, %2, %1, %0|%0, %1, %2, %3}";
      else
	return "vpinsrb\t{%3, %k2, %1, %0|%0, %1, %k2, %3}";
    case 0:
      if (MEM_P (operands[2]))
	return "pinsrb\t{%3, %2, %0|%0, %2, %3}";
      else
	return "pinsrb\t{%3, %k2, %0|%0, %k2, %3}";
    default:
      gcc_unreachable ();
    }
}
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sselog")
   (set_attr "addr" "gpr16,*")
   (set_attr "prefix_extra" "1")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "orig,vex")
   (set_attr "mode" "TI")])

(define_insn "*mmx_pextrw"
  [(set (match_operand:HI 0 "register_sse4nonimm_operand" "=r,r,jm,m")
	(vec_select:HI
	  (match_operand:V4HI 1 "register_operand" "y,YW,YW,YW")
	  (parallel [(match_operand:SI 2 "const_0_to_3_operand")])))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && (TARGET_SSE || TARGET_3DNOW_A)"
  "@
   pextrw\t{%2, %1, %k0|%k0, %1, %2}
   %vpextrw\t{%2, %1, %k0|%k0, %1, %2}
   pextrw\t{%2, %1, %0|%0, %1, %2}
   vpextrw\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse2,sse4_noavx,avx")
   (set_attr "addr" "*,*,gpr16,*")
   (set_attr "mmx_isa" "native,*,*,*")
   (set_attr "type" "mmxcvt,sselog1,sselog1,sselog1")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "orig,maybe_vex,maybe_vex,maybe_evex")
   (set_attr "mode" "DI,TI,TI,TI")])

(define_insn "*mmx_pextrw<mode>"
  [(set (match_operand:<mmxscalarmode> 0 "register_sse4nonimm_operand" "=?r,?r,jm,m,x,Yw")
	(vec_select:<mmxscalarmode>
	  (match_operand:V4F_64 1 "register_operand" "y,YW,YW,YW,0,YW")
	  (parallel [(match_operand:SI 2 "const_0_to_3_operand")])))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && (TARGET_SSE || TARGET_3DNOW_A)"
{
  switch (which_alternative)
    {
    case 0:
    case 1:
     return "%vpextrw\t{%2, %1, %k0|%k0, %1, %2}";
    case 2:
    case 3:
     return "%vpextrw\t{%2, %1, %0|%0, %1, %2}";
    case 4:
      operands[2] = GEN_INT (INTVAL (operands[2]) * 2);
      return "psrldq\t{%2, %0|%0, %2}";
    case 5:
      operands[2] = GEN_INT (INTVAL (operands[2]) * 2);
      return "vpsrldq\t{%2, %1, %0|%0, %1, %2}";

    default:
      gcc_unreachable ();
    }
}
  [(set_attr "isa" "*,sse2,sse4_noavx,avx,noavx,avx")
   (set_attr "addr" "*,*,gpr16,*,*,*")
   (set_attr "mmx_isa" "native,*,*,*,*,*")
   (set_attr "type" "mmxcvt,sselog1,sselog1,sselog1,sseishft1,sseishft1")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "orig,maybe_vex,maybe_vex,maybe_evex,orig,maybe_evex")
   (set_attr "mode" "DI,TI,TI,TI,TI,TI")])

(define_insn "*mmx_pextrw_zext"
  [(set (match_operand:SWI48 0 "register_operand" "=r,r")
	(zero_extend:SWI48
	  (vec_select:HI
	    (match_operand:V4HI 1 "register_operand" "y,YW")
	    (parallel [(match_operand:SI 2 "const_0_to_3_operand")]))))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && (TARGET_SSE || TARGET_3DNOW_A)"
  "@
   pextrw\t{%2, %1, %k0|%k0, %1, %2}
   %vpextrw\t{%2, %1, %k0|%k0, %1, %2}"
  [(set_attr "isa" "*,sse2")
   (set_attr "mmx_isa" "native,*")
   (set_attr "type" "mmxcvt,sselog1")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "orig,maybe_vex")
   (set_attr "mode" "DI,TI")])

(define_insn "*mmx_pextrb"
  [(set (match_operand:QI 0 "nonimmediate_operand" "=jr,jm,r,m")
	(vec_select:QI
	  (match_operand:V8QI 1 "register_operand" "YW,YW,YW,YW")
	  (parallel [(match_operand:SI 2 "const_0_to_7_operand")])))]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE"
  "@
   pextrb\t{%2, %1, %k0|%k0, %1, %2}
   pextrb\t{%2, %1, %0|%0, %1, %2}
   vpextrb\t{%2, %1, %k0|%k0, %1, %2}
   vpextrb\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,noavx,avx,avx")
   (set_attr "addr" "*,gpr16,*,*")
   (set_attr "type" "sselog1")
   (set_attr "prefix_extra" "1")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "maybe_vex")
   (set_attr "mode" "TI")])

(define_insn "*mmx_pextrb_zext"
  [(set (match_operand:SWI248 0 "register_operand" "=jr,r")
	(zero_extend:SWI248
	  (vec_select:QI
	    (match_operand:V8QI 1 "register_operand" "YW,YW")
	    (parallel [(match_operand:SI 2 "const_0_to_7_operand")]))))]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE"
  "%vpextrb\t{%2, %1, %k0|%k0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sselog1")
   (set_attr "prefix_extra" "1")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "maybe_vex")
   (set_attr "mode" "TI")])

(define_insn "mmx_pshufbv8qi3"
  [(set (match_operand:V8QI 0 "register_operand" "=x,Yw")
	(unspec:V8QI
	  [(match_operand:V8QI 1 "register_operand" "0,Yw")
	   (match_operand:V16QI 2 "vector_operand" "xja,Ywm")]
	  UNSPEC_PSHUFB))]
  "TARGET_SSSE3 && TARGET_MMX_WITH_SSE"
  "@
   pshufb\t{%2, %0|%0, %2}
   vpshufb\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "addr" "gpr16,*")
   (set_attr "type" "sselog1")
   (set_attr "prefix_extra" "1")
   (set_attr "prefix" "orig,maybe_evex")
   (set_attr "btver2_decode" "vector")
   (set_attr "mode" "TI")])

(define_insn "mmx_pshufbv4qi3"
  [(set (match_operand:V4QI 0 "register_operand" "=x,Yw")
	(unspec:V4QI
	  [(match_operand:V4QI 1 "register_operand" "0,Yw")
	   (match_operand:V16QI 2 "vector_operand" "xja,Ywm")]
	  UNSPEC_PSHUFB))]
  "TARGET_SSSE3"
  "@
   pshufb\t{%2, %0|%0, %2}
   vpshufb\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "addr" "gpr16,*")
   (set_attr "type" "sselog1")
   (set_attr "prefix_extra" "1")
   (set_attr "prefix" "orig,maybe_evex")
   (set_attr "btver2_decode" "vector")
   (set_attr "mode" "TI")])

(define_expand "mmx_pshufw"
  [(match_operand:V4HI 0 "register_operand")
   (match_operand:V4HI 1 "register_mmxmem_operand")
   (match_operand:SI 2 "const_int_operand")]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && (TARGET_SSE || TARGET_3DNOW_A)"
{
  int mask = INTVAL (operands[2]);
  emit_insn (gen_mmx_pshufwv4hi_1 (operands[0], operands[1],
				   GEN_INT ((mask >> 0) & 3),
				   GEN_INT ((mask >> 2) & 3),
				   GEN_INT ((mask >> 4) & 3),
				   GEN_INT ((mask >> 6) & 3)));
  DONE;
})

(define_insn "mmx_pshufw<mode>_1"
  [(set (match_operand:V4FI_64 0 "register_operand" "=y,Yw")
	(vec_select:V4FI_64
	  (match_operand:V4FI_64 1 "register_mmxmem_operand" "ym,Yw")
	  (parallel [(match_operand 2 "const_0_to_3_operand")
		     (match_operand 3 "const_0_to_3_operand")
		     (match_operand 4 "const_0_to_3_operand")
		     (match_operand 5 "const_0_to_3_operand")])))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && (TARGET_SSE || TARGET_3DNOW_A)"
{
  int mask = 0;
  mask |= INTVAL (operands[2]) << 0;
  mask |= INTVAL (operands[3]) << 2;
  mask |= INTVAL (operands[4]) << 4;
  mask |= INTVAL (operands[5]) << 6;
  operands[2] = GEN_INT (mask);

  switch (which_alternative)
    {
    case 0:
      return "pshufw\t{%2, %1, %0|%0, %1, %2}";
    case 1:
      return "%vpshuflw\t{%2, %1, %0|%0, %1, %2}";
    default:
      gcc_unreachable ();
    }
}
  [(set_attr "isa" "*,sse2")
   (set_attr "mmx_isa" "native,*")
   (set_attr "type" "mmxcvt,sselog1")
   (set_attr "length_immediate" "1")
   (set_attr "mode" "DI,TI")])

(define_insn "*mmx_pshufd_1"
  [(set (match_operand:V2SI 0 "register_operand" "=Yv")
	(vec_select:V2SI
	  (match_operand:V2SI 1 "register_operand" "Yv")
	  (parallel [(match_operand 2 "const_0_to_1_operand")
		     (match_operand 3 "const_0_to_1_operand")])))]
  "TARGET_MMX_WITH_SSE"
{
  int mask = 0;
  mask |= INTVAL (operands[2]) << 0;
  mask |= INTVAL (operands[3]) << 2;
  mask |= 2 << 4;
  mask |= 3 << 6;
  operands[2] = GEN_INT (mask);

  return "%vpshufd\t{%2, %1, %0|%0, %1, %2}";
}
  [(set_attr "type" "sselog1")
   (set_attr "prefix_data16" "1")
   (set_attr "length_immediate" "1")
   (set_attr "mode" "TI")])

(define_insn "*mmx_pblendw64"
  [(set (match_operand:V4FI_64 0 "register_operand" "=Yr,*x,x")
	(vec_merge:V4FI_64
	  (match_operand:V4FI_64 2 "register_operand" "Yr,*x,x")
	  (match_operand:V4FI_64 1 "register_operand" "0,0,x")
	  (match_operand:SI 3 "const_0_to_15_operand")))]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE"
  "@
   pblendw\t{%3, %2, %0|%0, %2, %3}
   pblendw\t{%3, %2, %0|%0, %2, %3}
   vpblendw\t{%3, %2, %1, %0|%0, %1, %2, %3}"
  [(set_attr "isa" "noavx,noavx,avx")
   (set_attr "type" "ssemov")
   (set_attr "prefix_extra" "1")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "orig,orig,vex")
   (set_attr "mode" "TI")])

(define_insn "*mmx_pblendw32"
  [(set (match_operand:V2FI_32 0 "register_operand" "=Yr,*x,x")
	(vec_merge:V2FI_32
	  (match_operand:V2FI_32 2 "register_operand" "Yr,*x,x")
	  (match_operand:V2FI_32 1 "register_operand" "0,0,x")
	  (match_operand:SI 3 "const_0_to_7_operand")))]
  "TARGET_SSE4_1"
  "@
   pblendw\t{%3, %2, %0|%0, %2, %3}
   pblendw\t{%3, %2, %0|%0, %2, %3}
   vpblendw\t{%3, %2, %1, %0|%0, %1, %2, %3}"
  [(set_attr "isa" "noavx,noavx,avx")
   (set_attr "type" "ssemov")
   (set_attr "prefix_extra" "1")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "orig,orig,vex")
   (set_attr "mode" "TI")])

;; Optimize V2SImode load from memory, swapping the elements and
;; storing back into the memory into DImode rotate of the memory by 32.
(define_split
  [(set (match_operand:V2SI 0 "memory_operand")
	(vec_select:V2SI (match_dup 0)
	  (parallel [(const_int 1) (const_int 0)])))]
  "TARGET_64BIT && (TARGET_READ_MODIFY_WRITE || optimize_insn_for_size_p ())"
  [(set (match_dup 0)
	(rotate:DI (match_dup 0) (const_int 32)))]
  "operands[0] = adjust_address (operands[0], DImode, 0);")

(define_insn "mmx_pswapdv2si2"
  [(set (match_operand:V2SI 0 "register_operand" "=y,Yv")
	(vec_select:V2SI
	  (match_operand:V2SI 1 "register_mmxmem_operand" "ym,Yv")
	  (parallel [(const_int 1) (const_int 0)])))]
  "TARGET_3DNOW_A"
  "@
   pswapd\t{%1, %0|%0, %1}
   %vpshufd\t{$0xe1, %1, %0|%0, %1, 0xe1}";
  [(set_attr "isa" "*,sse2")
   (set_attr "mmx_isa" "native,*")
   (set_attr "type" "mmxcvt,sselog1")
   (set_attr "prefix_extra" "1,*")
   (set_attr "length_immediate" "*,1")
   (set_attr "mode" "DI,TI")])

(define_insn "*vec_dupv4hi"
  [(set (match_operand:V4HI 0 "register_operand" "=y,Yw")
	(vec_duplicate:V4HI
	  (truncate:HI
	    (match_operand:SI 1 "register_operand" "0,Yw"))))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && (TARGET_SSE || TARGET_3DNOW_A)"
  "@
   pshufw\t{$0, %0, %0|%0, %0, 0}
   %vpshuflw\t{$0, %1, %0|%0, %1, 0}"
  [(set_attr "isa" "*,sse2")
   (set_attr "mmx_isa" "native,*")
   (set_attr "type" "mmxcvt,sselog1")
   (set_attr "length_immediate" "1")
   (set_attr "mode" "DI,TI")])

(define_insn "*vec_dup<mode>"
  [(set (match_operand:V4FI_64 0 "register_operand" "=Yw")
	(vec_duplicate:V4FI_64
	  (match_operand:<mmxscalarmode> 1 "register_operand" "Yw")))]
  "TARGET_MMX_WITH_SSE"
  "%vpshuflw\t{$0, %1, %0|%0, %1, 0}"
  [(set_attr "isa" "sse2")
   (set_attr "type" "sselog1")
   (set_attr "length_immediate" "1")
   (set_attr "mode" "TI")])

(define_insn "*vec_dupv2si"
  [(set (match_operand:V2SI 0 "register_operand" "=y,Yv")
	(vec_duplicate:V2SI
	  (match_operand:SI 1 "register_operand" "0,Yv")))]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
  "@
   punpckldq\t%0, %0
   %vpshufd\t{$0xe0, %1, %0|%0, %1, 0xe0}"
  [(set_attr "isa" "*,sse2")
   (set_attr "mmx_isa" "native,*")
   (set_attr "type" "mmxcvt,sselog1")
   (set_attr "prefix_data16" "*,1")
   (set_attr "length_immediate" "*,1")
   (set_attr "mode" "DI,TI")])

(define_insn "*mmx_concatv2si"
  [(set (match_operand:V2SI 0 "register_operand"     "=y,y")
	(vec_concat:V2SI
	  (match_operand:SI 1 "nonimmediate_operand" " 0,rm")
	  (match_operand:SI 2 "nonimm_or_0_operand"  "ym,C")))]
  "TARGET_MMX && !TARGET_SSE"
  "@
   punpckldq\t{%2, %0|%0, %2}
   movd\t{%1, %0|%0, %1}"
  [(set_attr "type" "mmxcvt,mmxmov")
   (set_attr "mode" "DI")])

(define_expand "vec_setv2si"
  [(match_operand:V2SI 0 "register_operand")
   (match_operand:SI 1 "register_operand")
   (match_operand 2 "vec_setm_mmx_operand")]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
{
  if (CONST_INT_P (operands[2]))
    ix86_expand_vector_set (TARGET_MMX_WITH_SSE, operands[0], operands[1],
			    INTVAL (operands[2]));
  else
    ix86_expand_vector_set_var (operands[0], operands[1], operands[2]);
  DONE;
})

;; Avoid combining registers from different units in a single alternative,
;; see comment above inline_secondary_memory_needed function in i386.cc
(define_insn_and_split "*vec_extractv2si_0"
  [(set (match_operand:SI 0 "nonimmediate_operand"     "=x,m,y, m,r,r")
	(vec_select:SI
	  (match_operand:V2SI 1 "nonimmediate_operand" "xm,x,ym,y,m,x")
	  (parallel [(const_int 0)])))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && !(MEM_P (operands[0]) && MEM_P (operands[1]))"
  "#"
  "&& reload_completed"
  [(set (match_dup 0) (match_dup 1))]
  "operands[1] = gen_lowpart (SImode, operands[1]);"
  [(set_attr "isa" "*,*,*,*,*,sse2")
   (set_attr "mmx_isa" "*,*,native,native,*,*")
   (set (attr "preferred_for_speed")
     (cond [(eq_attr "alternative" "5")
	      (symbol_ref "TARGET_INTER_UNIT_MOVES_FROM_VEC")
	   ]
	   (symbol_ref "true")))])

(define_insn "*vec_extractv2si_0_zext_sse4"
  [(set (match_operand:DI 0 "register_operand" "=r,x")
	(zero_extend:DI
	  (vec_select:SI
	    (match_operand:V2SI 1 "register_operand" "x,x")
	    (parallel [(const_int 0)]))))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE) && TARGET_SSE4_1"
  "#"
  [(set_attr "isa" "x64,*")
   (set (attr "preferred_for_speed")
     (cond [(eq_attr "alternative" "0")
	      (symbol_ref "TARGET_INTER_UNIT_MOVES_FROM_VEC")
	   ]
	   (symbol_ref "true")))])

(define_insn "*vec_extractv2si_0_zext"
  [(set (match_operand:DI 0 "register_operand" "=r")
	(zero_extend:DI
	  (vec_select:SI
	    (match_operand:V2SI 1 "register_operand" "x")
	    (parallel [(const_int 0)]))))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && TARGET_64BIT && TARGET_SSE2 && TARGET_INTER_UNIT_MOVES_FROM_VEC"
  "#")

(define_split
  [(set (match_operand:DI 0 "register_operand")
	(zero_extend:DI
	  (vec_select:SI
	    (match_operand:V2SI 1 "register_operand")
	    (parallel [(const_int 0)]))))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && TARGET_SSE2 && reload_completed"
  [(set (match_dup 0) (zero_extend:DI (match_dup 1)))]
  "operands[1] = gen_lowpart (SImode, operands[1]);")

;; Avoid combining registers from different units in a single alternative,
;; see comment above inline_secondary_memory_needed function in i386.cc
(define_insn "*vec_extractv2si_1"
  [(set (match_operand:SI 0 "nonimmediate_operand"     "=y,jrjm,rm,x,x,y,x,r")
	(vec_select:SI
	  (match_operand:V2SI 1 "nonimmediate_operand" " 0,x,   x ,x,0,o,o,o")
	  (parallel [(const_int 1)])))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && !(MEM_P (operands[0]) && MEM_P (operands[1]))"
  "@
   punpckhdq\t%0, %0
   pextrd\t{$1, %1, %0|%0, %1, 1}
   vpextrd\t{$1, %1, %0|%0, %1, 1}
   %vpshufd\t{$0xe5, %1, %0|%0, %1, 0xe5}
   shufps\t{$0xe5, %0, %0|%0, %0, 0xe5}
   #
   #
   #"
  [(set_attr "isa" "*,sse4_noavx,avx,sse2,noavx,*,*,*")
   (set (attr "addr")
	(if_then_else (eq_attr "alternative" "1")
		      (const_string "gpr16")
		      (const_string "*")))
   (set_attr "mmx_isa" "native,*,*,*,*,native,*,*")
   (set_attr "type" "mmxcvt,ssemov,ssemov,sseshuf1,sseshuf1,mmxmov,ssemov,imov")
   (set (attr "length_immediate")
     (if_then_else (eq_attr "alternative" "1,2,3,4")
		   (const_string "1")
		   (const_string "*")))
   (set_attr "prefix" "orig,orig,maybe_evex,maybe_vex,orig,orig,orig,orig")
   (set_attr "mode" "DI,TI,TI,TI,V4SF,SI,SI,SI")])

(define_split
  [(set (match_operand:SI 0 "register_operand")
	(vec_select:SI
	  (match_operand:V2SI 1 "memory_operand")
	  (parallel [(const_int 1)])))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE) && reload_completed"
  [(set (match_dup 0) (match_dup 1))]
  "operands[1] = adjust_address (operands[1], SImode, 4);")

(define_insn "*vec_extractv2si_1_zext"
  [(set (match_operand:DI 0 "register_operand" "=jr,r")
	(zero_extend:DI
	  (vec_select:SI
	    (match_operand:V2SI 1 "register_operand" "x,x")
	    (parallel [(const_int 1)]))))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && TARGET_64BIT && TARGET_SSE4_1"
  "%vpextrd\t{$1, %1, %k0|%k0, %1, 1}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sselog1")
   (set_attr "prefix_extra" "1")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "maybe_vex")
   (set_attr "mode" "TI")])

(define_insn_and_split "*vec_extractv2si_zext_mem"
  [(set (match_operand:DI 0 "register_operand" "=y,x,r")
	(zero_extend:DI
	  (vec_select:SI
	    (match_operand:V2SI 1 "memory_operand" "o,o,o")
	    (parallel [(match_operand:SI 2 "const_0_to_1_operand")]))))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE) && TARGET_64BIT"
  "#"
  "&& reload_completed"
  [(set (match_dup 0) (zero_extend:DI (match_dup 1)))]
{
  operands[1] = adjust_address (operands[1], SImode, INTVAL (operands[2]) * 4);
}
  [(set_attr "isa" "*,sse2,*")
   (set_attr "mmx_isa" "native,*,*")])

(define_expand "vec_extractv2sisi"
  [(match_operand:SI 0 "register_operand")
   (match_operand:V2SI 1 "register_operand")
   (match_operand 2 "const_int_operand")]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
{
  ix86_expand_vector_extract (TARGET_MMX_WITH_SSE, operands[0],
			      operands[1], INTVAL (operands[2]));
  DONE;
})

(define_expand "vec_initv2sisi"
  [(match_operand:V2SI 0 "register_operand")
   (match_operand 1)]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE) && TARGET_SSE"
{
  ix86_expand_vector_init (TARGET_MMX_WITH_SSE, operands[0],
			   operands[1]);
  DONE;
})

(define_expand "vec_set<mode>"
  [(match_operand:V4FI_64 0 "register_operand")
   (match_operand:<mmxscalarmode> 1 "register_operand")
   (match_operand 2 "vec_setm_mmx_operand")]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
{
  if (CONST_INT_P (operands[2]))
    ix86_expand_vector_set (TARGET_MMX_WITH_SSE, operands[0], operands[1],
			    INTVAL (operands[2]));
  else
    ix86_expand_vector_set_var (operands[0], operands[1], operands[2]);
  DONE;
})

(define_expand "vec_extract<mode><mmxscalarmodelower>"
  [(match_operand:<mmxscalarmode> 0 "register_operand")
   (match_operand:V4FI_64 1 "register_operand")
   (match_operand 2 "const_int_operand")]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
{
  ix86_expand_vector_extract (TARGET_MMX_WITH_SSE, operands[0],
			      operands[1], INTVAL (operands[2]));
  DONE;
})

(define_expand "vec_initv4hihi"
  [(match_operand:V4HI 0 "register_operand")
   (match_operand 1)]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE) && TARGET_SSE"
{
  ix86_expand_vector_init (TARGET_MMX_WITH_SSE, operands[0],
			   operands[1]);
  DONE;
})

(define_expand "vec_init<mode><mmxscalarmodelower>"
  [(match_operand:V4F_64 0 "register_operand")
   (match_operand 1)]
  "TARGET_MMX_WITH_SSE"
{
  ix86_expand_vector_init (TARGET_MMX_WITH_SSE, operands[0],
			   operands[1]);
  DONE;
})

(define_expand "vec_setv8qi"
  [(match_operand:V8QI 0 "register_operand")
   (match_operand:QI 1 "register_operand")
   (match_operand 2 "vec_setm_mmx_operand")]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE"
{
  if (CONST_INT_P (operands[2]))
    ix86_expand_vector_set (TARGET_MMX_WITH_SSE, operands[0], operands[1],
			    INTVAL (operands[2]));
  else
    ix86_expand_vector_set_var (operands[0], operands[1], operands[2]);
  DONE;
})

(define_expand "vec_extractv8qiqi"
  [(match_operand:QI 0 "register_operand")
   (match_operand:V8QI 1 "register_operand")
   (match_operand 2 "const_int_operand")]
  "TARGET_SSE4_1 && TARGET_MMX_WITH_SSE"
{
  ix86_expand_vector_extract (TARGET_MMX_WITH_SSE, operands[0],
			      operands[1], INTVAL (operands[2]));
  DONE;
})

(define_expand "vec_initv8qiqi"
  [(match_operand:V8QI 0 "register_operand")
   (match_operand 1)]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE) && TARGET_SSE"
{
  ix86_expand_vector_init (TARGET_MMX_WITH_SSE, operands[0],
			   operands[1]);
  DONE;
})

(define_insn "*pinsrw"
  [(set (match_operand:V2FI_32 0 "register_operand" "=x,YW,&x")
	(vec_merge:V2FI_32
	  (vec_duplicate:V2FI_32
	    (match_operand:<mmxscalarmode> 2 "nonimmediate_operand" "rm,rm,x"))
	  (match_operand:V2FI_32 1 "register_operand" "0,YW,x")
	  (match_operand:SI 3 "const_int_operand")))]
  "TARGET_SSE2
   && ((unsigned) exact_log2 (INTVAL (operands[3]))
       < GET_MODE_NUNITS (V2HImode))"
{
  operands[3] = GEN_INT (exact_log2 (INTVAL (operands[3])));
  switch (which_alternative)
    {
    case 2:
      return "#";
    case 1:
      if (MEM_P (operands[2]))
	return "vpinsrw\t{%3, %2, %1, %0|%0, %1, %2, %3}";
      else
	return "vpinsrw\t{%3, %k2, %1, %0|%0, %1, %k2, %3}";
    case 0:
      if (MEM_P (operands[2]))
	return "pinsrw\t{%3, %2, %0|%0, %2, %3}";
      else
	return "pinsrw\t{%3, %k2, %0|%0, %k2, %3}";
    default:
      gcc_unreachable ();
    }
}
  [(set_attr "isa" "noavx,avx,sse4")
   (set_attr "type" "sselog")
   (set_attr "length_immediate" "1")
   (set_attr "mode" "TI")])

;; For TARGET_SSE2, implement insert from XMM reg with PSHULFW + PBLENDW.
(define_split
  [(set (match_operand:V2FI_32 0 "sse_reg_operand")
	(vec_merge:V2FI_32
	  (vec_duplicate:V2FI_32
	    (match_operand:<mmxscalarmode> 2 "sse_reg_operand"))
	  (match_operand:V2FI_32 1 "sse_reg_operand")
	  (match_operand:SI 3 "const_int_operand")))]
  "TARGET_SSE4_1 && reload_completed
   && ((unsigned) exact_log2 (INTVAL (operands[3]))
       < GET_MODE_NUNITS (<MODE>mode))"
  [(set (match_dup 0)
	(vec_duplicate:V2FI_32 (match_dup 2)))
   (set (match_dup 0)
	(vec_merge:V2FI_32 (match_dup 1) (match_dup 0) (match_dup 3)))]
  "operands[3] = GEN_INT (~INTVAL (operands[3]) & 0x3);")


(define_insn "*pinsrb"
  [(set (match_operand:V4QI 0 "register_operand" "=x,YW")
	(vec_merge:V4QI
	  (vec_duplicate:V4QI
	    (match_operand:QI 2 "nonimmediate_operand" "jrjm,rm"))
	  (match_operand:V4QI 1 "register_operand" "0,YW")
	  (match_operand:SI 3 "const_int_operand")))]
  "TARGET_SSE4_1
   && ((unsigned) exact_log2 (INTVAL (operands[3]))
       < GET_MODE_NUNITS (V4QImode))"
{
  operands[3] = GEN_INT (exact_log2 (INTVAL (operands[3])));
  switch (which_alternative)
    {
    case 1:
      if (MEM_P (operands[2]))
	return "vpinsrb\t{%3, %2, %1, %0|%0, %1, %2, %3}";
      else
	return "vpinsrb\t{%3, %k2, %1, %0|%0, %1, %k2, %3}";
    case 0:
      if (MEM_P (operands[2]))
	return "pinsrb\t{%3, %2, %0|%0, %2, %3}";
      else
	return "pinsrb\t{%3, %k2, %0|%0, %k2, %3}";
    default:
      gcc_unreachable ();
    }
}
  [(set_attr "isa" "noavx,avx")
   (set_attr "addr" "gpr16,*")
   (set_attr "type" "sselog")
   (set_attr "prefix_extra" "1")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "orig,vex")
   (set_attr "mode" "TI")])

(define_insn "*pextrw"
  [(set (match_operand:HI 0 "register_sse4nonimm_operand" "=r,jm,m")
	(vec_select:HI
	  (match_operand:V2HI 1 "register_operand" "YW,YW,YW")
	  (parallel [(match_operand:SI 2 "const_0_to_1_operand")])))]
  "TARGET_SSE2"
  "@
   %vpextrw\t{%2, %1, %k0|%k0, %1, %2}
   pextrw\t{%2, %1, %0|%0, %1, %2}
   vpextrw\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse4_noavx,avx")
   (set_attr "addr" "*,gpr16,*")
   (set_attr "type" "sselog1")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "maybe_vex")
   (set_attr "mode" "TI")])

(define_insn "*pextrw<mode>"
  [(set (match_operand:<mmxscalarmode> 0 "register_sse4nonimm_operand" "=?r,jm,m,x,Yw")
	(vec_select:<mmxscalarmode>
	  (match_operand:V2F_32 1 "register_operand" "YW,YW,YW,0,YW")
	  (parallel [(match_operand:SI 2 "const_0_to_1_operand")])))]
  "TARGET_SSE2"
{
  switch (which_alternative)
    {
    case 0:
     return "%vpextrw\t{%2, %1, %k0|%k0, %1, %2}";
    case 1:
     return "pextrw\t{%2, %1, %0|%0, %1, %2}";
    case 2:
     return "vpextrw\t{%2, %1, %0|%0, %1, %2}";
    case 3:
      operands[2] = GEN_INT (INTVAL (operands[2]) * 2);
      return "psrldq\t{%2, %0|%0, %2}";
    case 4:
      operands[2] = GEN_INT (INTVAL (operands[2]) * 2);
      return "vpsrldq\t{%2, %1, %0|%0, %1, %2}";

    default:
      gcc_unreachable ();
    }
}
  [(set_attr "isa" "*,sse4_noavx,avx,noavx,avx")
   (set_attr "addr" "*,gpr16,*,*,*")
   (set_attr "type" "sselog1,sselog1,sselog1,sseishft1,sseishft1")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "maybe_vex,orig,maybe_evex,orig,maybe_evex")
   (set_attr "mode" "TI")])

(define_insn "*pextrw_zext"
  [(set (match_operand:SWI48 0 "register_operand" "=r")
	(zero_extend:SWI48
	  (vec_select:HI
	    (match_operand:V2HI 1 "register_operand" "YW")
	    (parallel [(match_operand:SI 2 "const_0_to_1_operand")]))))]
  "TARGET_SSE2"
  "%vpextrw\t{%2, %1, %k0|%k0, %1, %2}"
  [(set_attr "type" "sselog1")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "maybe_vex")
   (set_attr "mode" "TI")])

(define_insn "*pextrb"
  [(set (match_operand:QI 0 "nonimmediate_operand" "=jr,jm,r,m")
	(vec_select:QI
	  (match_operand:V4QI 1 "register_operand" "YW,YW,YW,YW")
	  (parallel [(match_operand:SI 2 "const_0_to_3_operand")])))]
  "TARGET_SSE4_1"
  "@
   pextrb\t{%2, %1, %k0|%k0, %1, %2}
   pextrb\t{%2, %1, %0|%0, %1, %2}
   vpextrb\t{%2, %1, %k0|%k0, %1, %2}
   vpextrb\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,noavx,avx,avx")
   (set_attr "addr" "*,gpr16,*,*")
   (set_attr "type" "sselog1")
   (set_attr "prefix_extra" "1")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "maybe_vex")
   (set_attr "mode" "TI")])

(define_insn "*pextrb_zext"
  [(set (match_operand:SWI248 0 "register_operand" "=jr,r")
	(zero_extend:SWI248
	  (vec_select:QI
	    (match_operand:V4QI 1 "register_operand" "YW,YW")
	    (parallel [(match_operand:SI 2 "const_0_to_3_operand")]))))]
  "TARGET_SSE4_1"
  "%vpextrb\t{%2, %1, %k0|%k0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sselog1")
   (set_attr "prefix_extra" "1")
   (set_attr "length_immediate" "1")
   (set_attr "prefix" "maybe_vex")
   (set_attr "mode" "TI")])

(define_expand "vec_set<mode>"
  [(match_operand:V2FI_32 0 "register_operand")
   (match_operand:<mmxscalarmode> 1 "register_operand")
   (match_operand 2 "vec_setm_sse41_operand")]
  "TARGET_SSE2"
{
  if (CONST_INT_P (operands[2]))
    ix86_expand_vector_set (false, operands[0], operands[1],
			    INTVAL (operands[2]));
  else
    ix86_expand_vector_set_var (operands[0], operands[1], operands[2]);
  DONE;
})

(define_expand "vec_extract<mode><mmxscalarmodelower>"
  [(match_operand:<mmxscalarmode> 0 "register_operand")
   (match_operand:V2FI_32 1 "register_operand")
   (match_operand 2 "const_int_operand")]
  "TARGET_SSE2"
{
  ix86_expand_vector_extract (false, operands[0],
			      operands[1], INTVAL (operands[2]));
  DONE;
})

(define_expand "vec_setv4qi"
  [(match_operand:V4QI 0 "register_operand")
   (match_operand:QI 1 "register_operand")
   (match_operand 2 "vec_setm_mmx_operand")]
  "TARGET_SSE4_1"
{
  if (CONST_INT_P (operands[2]))
    ix86_expand_vector_set (false, operands[0], operands[1],
			    INTVAL (operands[2]));
  else
    ix86_expand_vector_set_var (operands[0], operands[1], operands[2]);
  DONE;
})

(define_expand "vec_extractv4qiqi"
  [(match_operand:QI 0 "register_operand")
   (match_operand:V4QI 1 "register_operand")
   (match_operand 2 "const_int_operand")]
  "TARGET_SSE4_1"
{
  ix86_expand_vector_extract (false, operands[0],
			      operands[1], INTVAL (operands[2]));
  DONE;
})

(define_insn_and_split "*punpckwd"
  [(set (match_operand:V2FI_32 0 "register_operand" "=x,Yw")
	(vec_select:V2FI_32
	  (vec_concat:<mmxdoublevecmode>
	    (match_operand:V2FI_32 1 "register_operand" "0,Yw")
	    (match_operand:V2FI_32 2 "register_operand" "x,Yw"))
	  (parallel [(match_operand 3 "const_0_to_3_operand")
		     (match_operand 4 "const_0_to_3_operand")])))]
  "TARGET_SSE2"
  "#"
  "&& reload_completed"
  [(set (match_dup 5)
	(vec_select:<mmxxmmmode>
	  (match_dup 5)
	  (parallel [(match_dup 3) (match_dup 4)
		     (const_int 2) (const_int 3)
		     (const_int 4) (const_int 5)
		     (const_int 6) (const_int 7)])))]
{
  rtx dest = lowpart_subreg (<mmxxmmmode>mode, operands[0], <MODE>mode);
  rtx op1 = lowpart_subreg (<mmxxmmmode>mode, operands[1], <MODE>mode);
  rtx op2 = lowpart_subreg (<mmxxmmmode>mode, operands[2], <MODE>mode);

  emit_insn (gen_vec_interleave_low<mmxxmmmodelower> (dest, op1, op2));

  static const int map[4] = { 0, 2, 1, 3 };

  int sel0 = map[INTVAL (operands[3])];
  int sel1 = map[INTVAL (operands[4])];

  if (sel0 == 0 && sel1 == 1)
    DONE;

  operands[3] = GEN_INT (sel0);
  operands[4] = GEN_INT (sel1);
  operands[5] = dest;
}
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sselog")
   (set_attr "mode" "TI")])

(define_insn "*pshufw<mode>_1"
  [(set (match_operand:V2FI_32 0 "register_operand" "=Yw")
	(vec_select:V2FI_32
	  (match_operand:V2FI_32 1 "register_operand" "Yw")
	  (parallel [(match_operand 2 "const_0_to_1_operand")
		     (match_operand 3 "const_0_to_1_operand")])))]
  "TARGET_SSE2"
{
  int mask = 0;
  mask |= INTVAL (operands[2]) << 0;
  mask |= INTVAL (operands[3]) << 2;
  mask |= 2 << 4;
  mask |= 3 << 6;
  operands[2] = GEN_INT (mask);

  return "%vpshuflw\t{%2, %1, %0|%0, %1, %2}";
}
  [(set_attr "type" "sselog1")
   (set_attr "length_immediate" "1")
   (set_attr "mode" "TI")])

(define_insn "*vec_dupv2hi"
  [(set (match_operand:V2HI 0 "register_operand" "=Yw")
	(vec_duplicate:V2HI
	  (truncate:HI
	    (match_operand:SI 1 "register_operand" "Yw"))))]
  "TARGET_SSE2"
  "%vpshuflw\t{$0, %1, %0|%0, %1, 0}"
  [(set_attr "type" "sselog1")
   (set_attr "length_immediate" "1")
   (set_attr "mode" "TI")])

(define_insn "*vec_dup<mode>"
  [(set (match_operand:V2FI_32 0 "register_operand" "=Yw")
	(vec_duplicate:V2FI_32
	  (match_operand:<mmxscalarmode> 1 "register_operand" "Yw")))]
  "TARGET_SSE2"
  "%vpshuflw\t{$0, %1, %0|%0, %1, 0}"
  [(set_attr "type" "sselog1")
   (set_attr "length_immediate" "1")
   (set_attr "mode" "TI")])

(define_expand "vec_init<mode><mmxscalarmodelower>"
  [(match_operand:V2FI_32 0 "register_operand")
   (match_operand 1)]
  "TARGET_SSE2"
{
  ix86_expand_vector_init (TARGET_MMX_WITH_SSE, operands[0],
			   operands[1]);
  DONE;
})

(define_expand "vec_initv4qiqi"
  [(match_operand:V2HI 0 "register_operand")
   (match_operand 1)]
  "TARGET_SSE2"
{
  ix86_expand_vector_init (TARGET_MMX_WITH_SSE, operands[0],
			   operands[1]);
  DONE;
})

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; Miscellaneous
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define_expand "mmx_uavg<mode>3"
  [(set (match_operand:MMXMODE12 0 "register_operand")
	(truncate:MMXMODE12
	  (lshiftrt:<mmxdoublemode>
	    (plus:<mmxdoublemode>
	      (plus:<mmxdoublemode>
		(zero_extend:<mmxdoublemode>
		  (match_operand:MMXMODE12 1 "register_mmxmem_operand"))
		(zero_extend:<mmxdoublemode>
		  (match_operand:MMXMODE12 2 "register_mmxmem_operand")))
	      (match_dup 3))
	    (const_int 1))))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && (TARGET_SSE || TARGET_3DNOW)"
{
  operands[3] = CONST1_RTX(<mmxdoublemode>mode);
  ix86_fixup_binary_operands_no_copy (PLUS, <MODE>mode, operands);
})

(define_insn "*mmx_uavgv8qi3"
  [(set (match_operand:V8QI 0 "register_operand" "=y,x,Yw")
	(truncate:V8QI
	  (lshiftrt:V8HI
	    (plus:V8HI
	      (plus:V8HI
		(zero_extend:V8HI
		  (match_operand:V8QI 1 "register_mmxmem_operand" "%0,0,Yw"))
		(zero_extend:V8HI
		  (match_operand:V8QI 2 "register_mmxmem_operand" "ym,x,Yw")))
	      (const_vector:V8HI [(const_int 1) (const_int 1)
				  (const_int 1) (const_int 1)
				  (const_int 1) (const_int 1)
				  (const_int 1) (const_int 1)]))
	    (const_int 1))))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && (TARGET_SSE || TARGET_3DNOW)
   && !(MEM_P (operands[1]) && MEM_P (operands[2]))"
{
  switch (which_alternative)
    {
    case 2:
      return "vpavgb\t{%2, %1, %0|%0, %1, %2}";
    case 1:
    case 0:
      /* These two instructions have the same operation, but their encoding
	 is different.  Prefer the one that is de facto standard.  */
      if (TARGET_SSE || TARGET_3DNOW_A)
	return "pavgb\t{%2, %0|%0, %2}";
      else
	return "pavgusb\t{%2, %0|%0, %2}";
      default:
	gcc_unreachable ();
    }
}
  [(set_attr "isa" "*,sse2_noavx,avx")
   (set_attr "mmx_isa" "native,*,*")
   (set_attr "type" "mmxshft,sseiadd,sseiadd")
   (set (attr "prefix_extra")
     (if_then_else
       (not (ior (match_test "TARGET_SSE")
		 (match_test "TARGET_3DNOW_A")))
       (const_string "1")
       (const_string "*")))
   (set_attr "mode" "DI,TI,TI")])

(define_insn "*mmx_uavgv4hi3"
  [(set (match_operand:V4HI 0 "register_operand" "=y,x,Yw")
	(truncate:V4HI
	  (lshiftrt:V4SI
	    (plus:V4SI
	      (plus:V4SI
		(zero_extend:V4SI
		  (match_operand:V4HI 1 "register_mmxmem_operand" "%0,0,Yw"))
		(zero_extend:V4SI
		  (match_operand:V4HI 2 "register_mmxmem_operand" "ym,x,Yw")))
	      (const_vector:V4SI [(const_int 1) (const_int 1)
				  (const_int 1) (const_int 1)]))
	    (const_int 1))))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && (TARGET_SSE || TARGET_3DNOW_A)
   && !(MEM_P (operands[1]) && MEM_P (operands[2]))"
  "@
   pavgw\t{%2, %0|%0, %2}
   pavgw\t{%2, %0|%0, %2}
   vpavgw\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse2_noavx,avx")
   (set_attr "mmx_isa" "native,*,*")
   (set_attr "type" "mmxshft,sseiadd,sseiadd")
   (set_attr "mode" "DI,TI,TI")])

(define_expand "uavg<mode>3_ceil"
  [(set (match_operand:MMXMODE12 0 "register_operand")
	(truncate:MMXMODE12
	  (lshiftrt:<mmxdoublemode>
	    (plus:<mmxdoublemode>
	      (plus:<mmxdoublemode>
		(zero_extend:<mmxdoublemode>
		  (match_operand:MMXMODE12 1 "register_operand"))
		(zero_extend:<mmxdoublemode>
		  (match_operand:MMXMODE12 2 "register_operand")))
	      (match_dup 3))
	    (const_int 1))))]
  "TARGET_MMX_WITH_SSE"
  "operands[3] = CONST1_RTX(<mmxdoublemode>mode);")

(define_insn "uavgv4qi3_ceil"
  [(set (match_operand:V4QI 0 "register_operand" "=x,Yw")
	(truncate:V4QI
	  (lshiftrt:V4HI
	    (plus:V4HI
	      (plus:V4HI
		(zero_extend:V4HI
		  (match_operand:V4QI 1 "register_operand" "%0,Yw"))
		(zero_extend:V4HI
		  (match_operand:V4QI 2 "register_operand" "x,Yw")))
	      (const_vector:V4HI [(const_int 1) (const_int 1)
				  (const_int 1) (const_int 1)]))
	    (const_int 1))))]
  "TARGET_SSE2"
  "@
   pavgb\t{%2, %0|%0, %2}
   vpavgb\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sseiadd")
   (set_attr "mode" "TI")])

(define_insn "uavgv2qi3_ceil"
  [(set (match_operand:V2QI 0 "register_operand" "=x,Yw")
	(truncate:V2QI
	  (lshiftrt:V2HI
	    (plus:V2HI
	      (plus:V2HI
		(zero_extend:V2HI
		  (match_operand:V2QI 1 "register_operand" "%0,Yw"))
		(zero_extend:V2HI
		  (match_operand:V2QI 2 "register_operand" "x,Yw")))
	      (const_vector:V2HI [(const_int 1) (const_int 1)]))
	    (const_int 1))))]
  "TARGET_SSE2"
  "@
   pavgb\t{%2, %0|%0, %2}
   vpavgb\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sseiadd")
   (set_attr "mode" "TI")])

(define_insn "uavgv2hi3_ceil"
  [(set (match_operand:V2HI 0 "register_operand" "=x,Yw")
	(truncate:V2HI
	  (lshiftrt:V2SI
	    (plus:V2SI
	      (plus:V2SI
		(zero_extend:V2SI
		  (match_operand:V2HI 1 "register_operand" "%0,Yw"))
		(zero_extend:V2SI
		  (match_operand:V2HI 2 "register_operand" "x,Yw")))
	      (const_vector:V2SI [(const_int 1) (const_int 1)]))
	    (const_int 1))))]
  "TARGET_SSE2"
  "@
   pavgw\t{%2, %0|%0, %2}
   vpavgw\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "noavx,avx")
   (set_attr "type" "sseiadd")
   (set_attr "mode" "TI")])

(define_expand "mmx_psadbw"
  [(set (match_operand:V1DI 0 "register_operand")
	(unspec:V1DI [(match_operand:V8QI 1 "register_mmxmem_operand")
		      (match_operand:V8QI 2 "register_mmxmem_operand")]
		     UNSPEC_PSADBW))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE) && (TARGET_SSE || TARGET_3DNOW_A)"
  "ix86_fixup_binary_operands_no_copy (PLUS, V8QImode, operands);")

(define_insn "*mmx_psadbw"
  [(set (match_operand:V1DI 0 "register_operand" "=y,x,Yw")
	(unspec:V1DI [(match_operand:V8QI 1 "register_mmxmem_operand" "%0,0,Yw")
		      (match_operand:V8QI 2 "register_mmxmem_operand" "ym,x,Yw")]
		     UNSPEC_PSADBW))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE) && (TARGET_SSE || TARGET_3DNOW_A)
   && ix86_binary_operator_ok (PLUS, V8QImode, operands)"
  "@
   psadbw\t{%2, %0|%0, %2}
   psadbw\t{%2, %0|%0, %2}
   vpsadbw\t{%2, %1, %0|%0, %1, %2}"
  [(set_attr "isa" "*,sse2_noavx,avx")
   (set_attr "mmx_isa" "native,*,*")
   (set_attr "type" "mmxshft,sseiadd,sseiadd")
   (set_attr "mode" "DI,TI,TI")])

(define_expand "reduc_<code>_scal_<mode>"
 [(any_logic:MMXMODE12
    (match_operand:<mmxscalarmode> 0 "register_operand")
    (match_operand:MMXMODE12 1 "register_operand"))]
 "TARGET_MMX_WITH_SSE"
{
  rtx tmp = gen_reg_rtx (<MODE>mode);
  ix86_expand_reduc (gen_<code><mode>3, tmp, operands[1]);
  emit_insn (gen_vec_extract<mode><mmxscalarmodelower> (operands[0],
						       tmp, const0_rtx));
  DONE;
})

(define_expand "reduc_<code>_scal_v4qi"
 [(any_logic:V4QI
    (match_operand:QI 0 "register_operand")
    (match_operand:V4QI 1 "register_operand"))]
 "TARGET_SSE2"
{
  rtx tmp = gen_reg_rtx (V4QImode);
  ix86_expand_reduc (gen_<code>v4qi3, tmp, operands[1]);
  emit_insn (gen_vec_extractv4qiqi (operands[0], tmp, const0_rtx));
  DONE;
})

(define_expand "reduc_plus_scal_v8qi"
 [(plus:V8QI
    (match_operand:QI 0 "register_operand")
    (match_operand:V8QI 1 "register_operand"))]
 "TARGET_MMX_WITH_SSE"
{
  rtx tmp = gen_reg_rtx (V8QImode);
  emit_move_insn (tmp, CONST0_RTX (V8QImode));
  rtx tmp2 = gen_reg_rtx (V1DImode);
  emit_insn (gen_mmx_psadbw (tmp2, operands[1], tmp));
  tmp2 = gen_lowpart (V8QImode, tmp2);
  emit_insn (gen_vec_extractv8qiqi (operands[0], tmp2, const0_rtx));
  DONE;
})

(define_expand "reduc_plus_scal_v4hi"
 [(plus:V4HI
    (match_operand:HI 0 "register_operand")
    (match_operand:V4HI 1 "register_operand"))]
 "TARGET_MMX_WITH_SSE"
{
  rtx tmp = gen_reg_rtx (V4HImode);
  ix86_expand_reduc (gen_addv4hi3, tmp, operands[1]);
  emit_insn (gen_vec_extractv4hihi (operands[0], tmp, const0_rtx));
  DONE;
})

(define_expand "reduc_<code>_scal_v4hi"
  [(smaxmin:V4HI
     (match_operand:HI 0 "register_operand")
     (match_operand:V4HI 1 "register_operand"))]
  "TARGET_MMX_WITH_SSE"
{
  rtx tmp = gen_reg_rtx (V4HImode);
  ix86_expand_reduc (gen_<code>v4hi3, tmp, operands[1]);
  emit_insn (gen_vec_extractv4hihi (operands[0], tmp, const0_rtx));
  DONE;
})

(define_expand "reduc_<code>_scal_v4qi"
  [(smaxmin:V4QI
     (match_operand:QI 0 "register_operand")
     (match_operand:V4QI 1 "register_operand"))]
  "TARGET_SSE4_1"
{
  rtx tmp = gen_reg_rtx (V4QImode);
  ix86_expand_reduc (gen_<code>v4qi3, tmp, operands[1]);
  emit_insn (gen_vec_extractv4qiqi (operands[0], tmp, const0_rtx));
  DONE;
})

(define_expand "reduc_<code>_scal_v4hi"
  [(umaxmin:V4HI
     (match_operand:HI 0 "register_operand")
     (match_operand:V4HI 1 "register_operand"))]
  "TARGET_MMX_WITH_SSE && TARGET_SSE4_1"
{
  rtx tmp = gen_reg_rtx (V4HImode);
  ix86_expand_reduc (gen_<code>v4hi3, tmp, operands[1]);
  emit_insn (gen_vec_extractv4hihi (operands[0], tmp, const0_rtx));
  DONE;
})

(define_expand "reduc_<code>_scal_v4qi"
  [(umaxmin:V4QI
     (match_operand:QI 0 "register_operand")
     (match_operand:V4QI 1 "register_operand"))]
  "TARGET_SSE4_1"
{
  rtx tmp = gen_reg_rtx (V4QImode);
  ix86_expand_reduc (gen_<code>v4qi3, tmp, operands[1]);
  emit_insn (gen_vec_extractv4qiqi (operands[0], tmp, const0_rtx));
  DONE;
})

(define_expand "reduc_plus_scal_v4qi"
 [(plus:V4QI
    (match_operand:QI 0 "register_operand")
    (match_operand:V4QI 1 "register_operand"))]
 "TARGET_SSE2"
{
  rtx op1 = gen_reg_rtx (V16QImode);
  emit_insn (gen_vec_setv4si_0 (lowpart_subreg (V4SImode, op1, V16QImode),
				CONST0_RTX (V4SImode),
				lowpart_subreg (SImode,
						operands[1],
						V4QImode)));
  rtx tmp = gen_reg_rtx (V16QImode);
  emit_move_insn (tmp, CONST0_RTX (V16QImode));
  rtx tmp2 = gen_reg_rtx (V2DImode);
  emit_insn (gen_sse2_psadbw (tmp2, op1, tmp));
  tmp2 = gen_lowpart (V16QImode, tmp2);
  emit_insn (gen_vec_extractv16qiqi (operands[0], tmp2, const0_rtx));
  DONE;
})

(define_expand "usadv8qi"
  [(match_operand:V2SI 0 "register_operand")
   (match_operand:V8QI 1 "register_operand")
   (match_operand:V8QI 2 "register_operand")
   (match_operand:V2SI 3 "register_operand")]
  "TARGET_MMX_WITH_SSE"
{
  rtx t1 = gen_reg_rtx (V1DImode);
  rtx t2 = gen_reg_rtx (V2SImode);
  emit_insn (gen_mmx_psadbw (t1, operands[1], operands[2]));
  convert_move (t2, t1, 0);
  emit_insn (gen_addv2si3 (operands[0], t2, operands[3]));
  DONE;
})

(define_expand "usdot_prodv2siv8qi"
  [(match_operand:V2SI 0 "register_operand")
   (match_operand:V8QI 1 "register_operand")
   (match_operand:V8QI 2 "register_operand")
   (match_operand:V2SI 3 "register_operand")]
  "TARGET_MMX_WITH_SSE && TARGET_SSE4_1"
{
  operands[1] = force_reg (V8QImode, operands[1]);
  operands[2] = force_reg (V8QImode, operands[2]);
  operands[3] = force_reg (V2SImode, operands[3]);

  if ((TARGET_AVX512VNNI && TARGET_AVX512VL)
     || TARGET_AVXVNNI)
    {
      rtx op1 = lowpart_subreg (V16QImode, operands[1], V8QImode);
      rtx op2 = lowpart_subreg (V16QImode, operands[2], V8QImode);
      rtx op3 = lowpart_subreg (V4SImode, operands[3], V2SImode);
      rtx op0 = gen_reg_rtx (V4SImode);

      emit_insn (gen_usdot_prodv4siv16qi (op0, op1, op2, op3));
      emit_move_insn (operands[0], lowpart_subreg (V2SImode, op0, V4SImode));
     }
   else
     {
      rtx op1 = gen_reg_rtx (V8HImode);
      rtx op2 = gen_reg_rtx (V8HImode);
      rtx op3 = gen_reg_rtx (V4SImode);
      rtx op0 = gen_reg_rtx (V4SImode);
      rtx op0_1 = gen_reg_rtx (V4SImode);

      emit_move_insn (op3, CONST0_RTX (V4SImode));
      emit_insn (gen_zero_extendv8qiv8hi2 (op1, operands[1]));
      emit_insn (gen_extendv8qiv8hi2 (op2, operands[2]));
      emit_insn (gen_sdot_prodv4siv8hi (op0, op1, op2, op3));

      /* vec_perm (op0, 2, 3, 0, 1);  */
      emit_insn (gen_sse2_pshufd (op0_1, op0, GEN_INT (78)));
      emit_insn (gen_addv4si3 (op0, op0, op0_1));
      emit_insn (gen_addv2si3 (operands[0], operands[3],
			       lowpart_subreg (V2SImode, op0, V4SImode)));
     }
    DONE;
})

(define_expand "sdot_prodv2siv8qi"
  [(match_operand:V2SI 0 "register_operand")
   (match_operand:V8QI 1 "register_operand")
   (match_operand:V8QI 2 "register_operand")
   (match_operand:V2SI 3 "register_operand")]
  "TARGET_MMX_WITH_SSE && TARGET_SSE4_1"
{
  operands[1] = force_reg (V8QImode, operands[1]);
  operands[2] = force_reg (V8QImode, operands[2]);
  operands[3] = force_reg (V2SImode, operands[3]);

  if (TARGET_AVXVNNIINT8)
    {
      rtx op1 = lowpart_subreg (V16QImode, operands[1], V8QImode);
      rtx op2 = lowpart_subreg (V16QImode, operands[2], V8QImode);
      rtx op3 = lowpart_subreg (V4SImode, operands[3], V2SImode);
      rtx op0 = gen_reg_rtx (V4SImode);

      emit_insn (gen_sdot_prodv4siv16qi (op0, op1, op2, op3));
      emit_move_insn (operands[0], lowpart_subreg (V2SImode, op0, V4SImode));
    }
  else
    {
      rtx op1 = gen_reg_rtx (V8HImode);
      rtx op2 = gen_reg_rtx (V8HImode);
      rtx op3 = gen_reg_rtx (V4SImode);
      rtx op0 = gen_reg_rtx (V4SImode);
      rtx op0_1 = gen_reg_rtx (V4SImode);

      emit_move_insn (op3, CONST0_RTX (V4SImode));
      emit_insn (gen_extendv8qiv8hi2 (op1, operands[1]));
      emit_insn (gen_extendv8qiv8hi2 (op2, operands[2]));
      emit_insn (gen_sdot_prodv4siv8hi (op0, op1, op2, op3));

      /* vec_perm (op0, 2, 3, 0, 1);  */
      emit_insn (gen_sse2_pshufd (op0_1, op0, GEN_INT (78)));
      emit_insn (gen_addv4si3 (op0, op0, op0_1));
      emit_insn (gen_addv2si3 (operands[0], operands[3],
			       lowpart_subreg (V2SImode, op0, V4SImode)));
    }
  DONE;

})

(define_expand "udot_prodv2siv8qi"
  [(match_operand:V2SI 0 "register_operand")
   (match_operand:V8QI 1 "register_operand")
   (match_operand:V8QI 2 "register_operand")
   (match_operand:V2SI 3 "register_operand")]
  "TARGET_MMX_WITH_SSE && TARGET_SSE4_1"
{
  operands[1] = force_reg (V8QImode, operands[1]);
  operands[2] = force_reg (V8QImode, operands[2]);
  operands[3] = force_reg (V2SImode, operands[3]);

  if (TARGET_AVXVNNIINT8)
    {
      rtx op1 = lowpart_subreg (V16QImode, operands[1], V8QImode);
      rtx op2 = lowpart_subreg (V16QImode, operands[2], V8QImode);
      rtx op3 = lowpart_subreg (V4SImode, operands[3], V2SImode);
      rtx op0 = gen_reg_rtx (V4SImode);

      emit_insn (gen_udot_prodv4siv16qi (op0, op1, op2, op3));
      emit_move_insn (operands[0], lowpart_subreg (V2SImode, op0, V4SImode));
    }
  else
    {
      rtx op1 = gen_reg_rtx (V8HImode);
      rtx op2 = gen_reg_rtx (V8HImode);
      rtx op3 = gen_reg_rtx (V4SImode);
      rtx op0 = gen_reg_rtx (V4SImode);
      rtx op0_1 = gen_reg_rtx (V4SImode);

      emit_move_insn (op3, CONST0_RTX (V4SImode));
      emit_insn (gen_zero_extendv8qiv8hi2 (op1, operands[1]));
      emit_insn (gen_zero_extendv8qiv8hi2 (op2, operands[2]));
      emit_insn (gen_sdot_prodv4siv8hi (op0, op1, op2, op3));

      /* vec_perm (op0, 2, 3, 0, 1);  */
      emit_insn (gen_sse2_pshufd (op0_1, op0, GEN_INT (78)));
      emit_insn (gen_addv4si3 (op0, op0, op0_1));
      emit_insn (gen_addv2si3 (operands[0], operands[3],
			       lowpart_subreg (V2SImode, op0, V4SImode)));
    }
  DONE;

})

(define_expand "usdot_prodv2siv4hi"
  [(match_operand:V2SI 0 "register_operand")
   (match_operand:V4HI 1 "register_operand")
   (match_operand:V4HI 2 "register_operand")
   (match_operand:V2SI 3 "register_operand")]
  "TARGET_AVXVNNIINT16 && TARGET_MMX_WITH_SSE"
{
  operands[1] = force_reg (V4HImode, operands[1]);
  operands[2] = force_reg (V4HImode, operands[2]);
  operands[3] = force_reg (V2SImode, operands[3]);

  rtx op1 = lowpart_subreg (V8HImode, operands[1], V4HImode);
  rtx op2 = lowpart_subreg (V8HImode, operands[2], V4HImode);
  rtx op3 = lowpart_subreg (V4SImode, operands[3], V2SImode);
  rtx op0 = gen_reg_rtx (V4SImode);

  emit_insn (gen_usdot_prodv4siv8hi (op0, op1, op2, op3));
  emit_move_insn (operands[0], lowpart_subreg (V2SImode, op0, V4SImode));
  DONE;
})

(define_expand "udot_prodv2siv4hi"
  [(match_operand:V2SI 0 "register_operand")
   (match_operand:V4HI 1 "register_operand")
   (match_operand:V4HI 2 "register_operand")
   (match_operand:V2SI 3 "register_operand")]
  "TARGET_AVXVNNIINT16 && TARGET_MMX_WITH_SSE"
{
  operands[1] = force_reg (V4HImode, operands[1]);
  operands[2] = force_reg (V4HImode, operands[2]);
  operands[3] = force_reg (V2SImode, operands[3]);

  rtx op1 = lowpart_subreg (V8HImode, operands[1], V4HImode);
  rtx op2 = lowpart_subreg (V8HImode, operands[2], V4HImode);
  rtx op3 = lowpart_subreg (V4SImode, operands[3], V2SImode);
  rtx op0 = gen_reg_rtx (V4SImode);

  emit_insn (gen_udot_prodv4siv8hi (op0, op1, op2, op3));
  emit_move_insn (operands[0], lowpart_subreg (V2SImode, op0, V4SImode));
  DONE;
})

(define_expand "sdot_prodv2siv4hi"
  [(match_operand:V2SI 0 "register_operand")
   (match_operand:V4HI 1 "register_operand")
   (match_operand:V4HI 2 "register_operand")
   (match_operand:V2SI 3 "register_operand")]
  "TARGET_MMX_WITH_SSE"
{
  operands[1] = force_reg (V4HImode, operands[1]);
  operands[2] = force_reg (V4HImode, operands[2]);
  operands[3] = force_reg (V2SImode, operands[3]);

  rtx op1 = lowpart_subreg (V8HImode, operands[1], V4HImode);
  rtx op2 = lowpart_subreg (V8HImode, operands[2], V4HImode);
  rtx op3 = lowpart_subreg (V4SImode, operands[3], V2SImode);
  rtx op0 = gen_reg_rtx (V4SImode);

  emit_insn (gen_sdot_prodv4siv8hi (op0, op1, op2, op3));
  emit_move_insn (operands[0], lowpart_subreg (V2SImode, op0, V4SImode));
  DONE;
})

(define_insn_and_split "mmx_pmovmskb"
  [(set (match_operand:SI 0 "register_operand" "=r,r,jr")
	(unspec:SI [(match_operand:V8QI 1 "register_operand" "y,x,x")]
		   UNSPEC_MOVMSK))]
  "(TARGET_MMX || TARGET_MMX_WITH_SSE)
   && (TARGET_SSE || TARGET_3DNOW_A)"
  "@
   pmovmskb\t{%1, %0|%0, %1}
   #
   #"
  "TARGET_SSE2 && reload_completed
   && SSE_REGNO_P (REGNO (operands[1]))"
  [(set (match_dup 0)
	(unspec:SI [(match_dup 1)] UNSPEC_MOVMSK))
   (set (match_dup 0)
	(zero_extend:SI (match_dup 2)))]
{
  /* Generate SSE pmovmskb and zero-extend from QImode to SImode.  */
  operands[1] = lowpart_subreg (V16QImode, operands[1],
				GET_MODE (operands[1]));
  operands[2] = lowpart_subreg (QImode, operands[0],
				GET_MODE (operands[0]));
}
  [(set_attr "mmx_isa" "native,sse_noavx,avx")
   (set_attr "type" "mmxcvt,ssemov,ssemov")
   (set_attr "mode" "DI,TI,TI")])

(define_expand "mmx_maskmovq"
  [(set (match_operand:V8QI 0 "memory_operand")
	(unspec:V8QI [(match_operand:V8QI 1 "register_operand")
		      (match_operand:V8QI 2 "register_operand")
		      (match_dup 0)]
		     UNSPEC_MASKMOV))]
  "TARGET_SSE || TARGET_3DNOW_A")

(define_insn "*mmx_maskmovq"
  [(set (mem:V8QI (match_operand:P 0 "register_operand" "D"))
	(unspec:V8QI [(match_operand:V8QI 1 "register_operand" "y")
		      (match_operand:V8QI 2 "register_operand" "y")
		      (mem:V8QI (match_dup 0))]
		     UNSPEC_MASKMOV))]
  "TARGET_SSE || TARGET_3DNOW_A"
  ;; @@@ check ordering of operands in intel/nonintel syntax
  "maskmovq\t{%2, %1|%1, %2}"
  [(set_attr "type" "mmxcvt")
   (set_attr "znver1_decode" "vector")
   (set_attr "mode" "DI")])

(define_int_iterator EMMS
  [(UNSPECV_EMMS "TARGET_MMX")
   (UNSPECV_FEMMS "TARGET_3DNOW")])

(define_int_attr emms
  [(UNSPECV_EMMS "emms")
   (UNSPECV_FEMMS "femms")])

(define_expand "mmx_<emms>"
  [(parallel
    [(unspec_volatile [(const_int 0)] EMMS)
      (clobber (reg:XF ST0_REG))
      (clobber (reg:XF ST1_REG))
      (clobber (reg:XF ST2_REG))
      (clobber (reg:XF ST3_REG))
      (clobber (reg:XF ST4_REG))
      (clobber (reg:XF ST5_REG))
      (clobber (reg:XF ST6_REG))
      (clobber (reg:XF ST7_REG))
      (clobber (reg:DI MM0_REG))
      (clobber (reg:DI MM1_REG))
      (clobber (reg:DI MM2_REG))
      (clobber (reg:DI MM3_REG))
      (clobber (reg:DI MM4_REG))
      (clobber (reg:DI MM5_REG))
      (clobber (reg:DI MM6_REG))
      (clobber (reg:DI MM7_REG))])]
  "TARGET_MMX || TARGET_MMX_WITH_SSE"
{
   if (!TARGET_MMX)
     {
       emit_insn (gen_nop ());
       DONE;
     }
})

(define_insn "*mmx_<emms>"
  [(unspec_volatile [(const_int 0)] EMMS)
   (clobber (reg:XF ST0_REG))
   (clobber (reg:XF ST1_REG))
   (clobber (reg:XF ST2_REG))
   (clobber (reg:XF ST3_REG))
   (clobber (reg:XF ST4_REG))
   (clobber (reg:XF ST5_REG))
   (clobber (reg:XF ST6_REG))
   (clobber (reg:XF ST7_REG))
   (clobber (reg:DI MM0_REG))
   (clobber (reg:DI MM1_REG))
   (clobber (reg:DI MM2_REG))
   (clobber (reg:DI MM3_REG))
   (clobber (reg:DI MM4_REG))
   (clobber (reg:DI MM5_REG))
   (clobber (reg:DI MM6_REG))
   (clobber (reg:DI MM7_REG))]
  ""
  "<emms>"
  [(set_attr "type" "mmx")
   (set_attr "modrm" "0")
   (set_attr "memory" "none")])

(define_insn "popcount<mode>2"
  [(set (match_operand:VI1_16_32_64 0 "register_operand" "=v")
	(popcount:VI1_16_32_64
	  (match_operand:VI1_16_32_64 1 "register_operand" "v")))]
  "TARGET_AVX512VL && TARGET_AVX512BITALG"
  "vpopcntb\t{%1, %0|%0, %1}")

(define_insn "popcount<mode>2"
  [(set (match_operand:VI2_32_64 0 "register_operand" "=v")
	(popcount:VI2_32_64
	  (match_operand:VI2_32_64 1 "register_operand" "v")))]
  "TARGET_AVX512VL && TARGET_AVX512BITALG"
  "vpopcntw\t{%1, %0|%0, %1}")

(define_insn "popcountv2si2"
  [(set (match_operand:V2SI 0 "register_operand" "=v")
	(popcount:V2SI
	  (match_operand:V2SI 1 "register_operand" "v")))]
  "TARGET_AVX512VPOPCNTDQ && TARGET_AVX512VL && TARGET_MMX_WITH_SSE"
  "vpopcntd\t{%1, %0|%0, %1}")
