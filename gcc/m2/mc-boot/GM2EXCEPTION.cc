/* do not edit automatically generated by mc from M2EXCEPTION.  */
/* M2EXCEPTION.mod implement M2Exception and IsM2Exception.

Copyright (C) 2001-2025 Free Software Foundation, Inc.
Contributed by Gaius Mulley <gaius.mulley@southwales.ac.uk>.

This file is part of GNU Modula-2.

GNU Modula-2 is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GNU Modula-2 is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

Under Section 7 of GPL version 3, you are granted additional
permissions described in the GCC Runtime Library Exception, version
3.1, as published by the Free Software Foundation.

You should have received a copy of the GNU General Public License and
a copy of the GCC Runtime Library Exception along with this program;
see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
<http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include <stdbool.h>
#   if !defined (PROC_D)
#      define PROC_D
       typedef void (*PROC_t) (void);
       typedef struct { PROC_t proc; } PROC;
#   endif

#define _M2EXCEPTION_C

#include "GM2EXCEPTION.h"
#   include "GSYSTEM.h"
#   include "GRTExceptions.h"

extern "C" M2EXCEPTION_M2Exceptions M2EXCEPTION_M2Exception (void);
extern "C" bool M2EXCEPTION_IsM2Exception (void);

extern "C" M2EXCEPTION_M2Exceptions M2EXCEPTION_M2Exception (void)
{
  RTExceptions_EHBlock e;
  unsigned int n;

  /* If the program or coroutine is in the exception state then return the enumeration
   value representing the exception cause.  If it is not in the exception state then
   raises an exException exception.  */
  e = RTExceptions_GetExceptionBlock ();
  n = RTExceptions_GetNumber (e);
  if (n == (UINT_MAX))
    {
      RTExceptions_Raise ( ((unsigned int) (M2EXCEPTION_exException)), const_cast<void*> (static_cast<const void*>("../../gcc/m2/gm2-libs/M2EXCEPTION.mod")), 47, 6, const_cast<void*> (static_cast<const void*>("M2Exception")), const_cast<void*> (static_cast<const void*>("current coroutine is not in the exceptional execution state")));
      return M2EXCEPTION_exException;
    }
  else
    {
      return (M2EXCEPTION_M2Exceptions) (n);
    }
  /* static analysis guarentees a RETURN statement will be used before here.  */
  __builtin_unreachable ();
}

extern "C" bool M2EXCEPTION_IsM2Exception (void)
{
  RTExceptions_EHBlock e;

  /* Returns TRUE if the program or coroutine is in the exception state.
   Returns FALSE if the program or coroutine is not in the exception state.  */
  e = RTExceptions_GetExceptionBlock ();
  return (RTExceptions_GetNumber (e)) != (UINT_MAX);
  /* static analysis guarentees a RETURN statement will be used before here.  */
  __builtin_unreachable ();
}

extern "C" void _M2_M2EXCEPTION_init (__attribute__((unused)) int argc, __attribute__((unused)) char *argv[], __attribute__((unused)) char *envp[])
{
  RTExceptions_SetExceptionBlock (RTExceptions_InitExceptionBlock ());
}

extern "C" void _M2_M2EXCEPTION_fini (__attribute__((unused)) int argc, __attribute__((unused)) char *argv[], __attribute__((unused)) char *envp[])
{
}
