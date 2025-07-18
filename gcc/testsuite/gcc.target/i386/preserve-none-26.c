/* { dg-do compile { target { *-*-linux* && { ! ia32 } } } } */
/* { dg-options "-O2 -fno-pic -mtune=generic -msse2 -mno-apxf -mtune-ctrl=prologue_using_move,epilogue_using_move" } */
/* Keep labels and directives ('.cfi_startproc', '.cfi_endproc').  */
/* { dg-final { check-function-bodies "**" "" "" { target "*-*-*" } {^\t?\.}  } } */

/*
**entry:
**.LFB[0-9]+:
**	.cfi_startproc
**	movq	%r15, %rcx
**	movq	%r14, %rdx
**	movq	%r13, %rsi
**	movq	%r12, %rdi
**	jmp	continuation
**	.cfi_endproc
**...
*/

extern void continuation (void *, void *, void *, void *)
  __attribute__ ((no_callee_saved_registers));

__attribute__ ((preserve_none))
void
entry (void *a, void *b, void *c, void *d)
{
  continuation(a, b, c, d);
}
