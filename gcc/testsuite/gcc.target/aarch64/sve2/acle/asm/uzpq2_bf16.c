/* { dg-do assemble { target aarch64_asm_sve2p1_ok } } */
/* { dg-do compile { target { ! aarch64_asm_sve2p1_ok } } } */
/* { dg-skip-if "" { *-*-* } { "-DSTREAMING_COMPATIBLE" } { "" } } */
/* { dg-final { check-function-bodies "**" "" "-DCHECK_ASM" } } */

#include "test_sve_acle.h"

#pragma GCC target "+sve2p1"

/*
** uzpq2_bf16_tied1:
**	uzpq2	z0\.h, z0\.h, z1\.h
**	ret
*/
TEST_UNIFORM_Z (uzpq2_bf16_tied1, svbfloat16_t,
		z0 = svuzpq2_bf16 (z0, z1),
		z0 = svuzpq2 (z0, z1))

/*
** uzpq2_bf16_tied2:
**	uzpq2	z0\.h, z1\.h, z0\.h
**	ret
*/
TEST_UNIFORM_Z (uzpq2_bf16_tied2, svbfloat16_t,
		z0 = svuzpq2_bf16 (z1, z0),
		z0 = svuzpq2 (z1, z0))

/*
** uzpq2_bf16_untied:
**	uzpq2	z0\.h, z1\.h, z2\.h
**	ret
*/
TEST_UNIFORM_Z (uzpq2_bf16_untied, svbfloat16_t,
		z0 = svuzpq2_bf16 (z1, z2),
		z0 = svuzpq2 (z1, z2))
