#pragma once

// Private header of aion_gs_geomath: include it only from the library's .cpp files (never from a public header, the pragmas stay in effect
// for the rest of the translation unit).
//
// Java float/double arithmetic is strict IEEE 754 without contraction: `a * b + c` is a multiply followed by an add, never a fused
// multiply-add. Every method with such an expression is defined out of line in a .cpp file that includes this header, so the result does not
// depend on the flags of the translation units that include the public headers.
//
// MSVC x64 (the project compiler): /fp:precise is the default and does not contract since Visual Studio 2022 (contraction needs /fp:contract
// or /fp:fast; neither is set, see docs). SSE2 has no extended precision. The pragma below documents and enforces this for the library's code.
// GCC defaults to -ffp-contract=fast outside ISO mode, so a GCC build must also pass -ffp-contract=off to every target that includes the
// inline headers (the pragma only covers these .cpp files).

#if defined(_M_FP_FAST) || (defined(__FAST_MATH__) && !defined(_MSC_VER))
#error "aion_gs_geomath needs strict IEEE 754 float semantics: do not compile it with /fp:fast or -ffast-math"
#endif

#if defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#elif defined(_MSC_VER)
#pragma fp_contract(off)
#elif defined(__GNUC__)
#pragma GCC optimize("fp-contract=off")
#endif
