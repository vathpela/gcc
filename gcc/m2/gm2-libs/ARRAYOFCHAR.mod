(* ARRAYOFCHAR.def provides output procedures for the ARRAY OF CHAR datatype.

Copyright (C) 2025 Free Software Foundation, Inc.
Contributed by Gaius Mulley <gaiusmod2@gmail.com>.

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
<http://www.gnu.org/licenses/>.  *)

IMPLEMENTATION MODULE ARRAYOFCHAR ;

FROM FIO IMPORT WriteChar, WriteLine ;
IMPORT StrLib ;


(*
   Write - writes a string to file f.
*)

PROCEDURE Write (f: File; str: ARRAY OF CHAR) ;
VAR
   len, i: CARDINAL ;
BEGIN
   len := StrLib.StrLen (str) ;
   i := 0 ;
   WHILE i < len DO
      WriteChar (f, str[i]) ;
      INC (i)
   END
END Write ;


PROCEDURE WriteLn (f: File) ;
BEGIN
   WriteLine (f)
END WriteLn ;


END ARRAYOFCHAR.
