#pragma once

//
// util/Annotations.h
// -------------------
// Central stash for gentle, compiler-agnostic attribute shims.  The project is
// split between hardware and simulator builds, and static analysis tends to
// gripe about public APIs that only light up in one universe.  Instead of
// sprinkling tool-specific pragmas everywhere, we provide a handful of macros
// so headers can opt-in to intent-revealing attributes ("yes, this symbol is
// intentionally unused in this translation unit") without locking us into a
// particular compiler.
//
// Novelty < Utility.  These helpers keep the code teaching-friendly while still
// letting us placate linters.

#if defined(__has_cpp_attribute)
  #if __has_cpp_attribute(maybe_unused)
    #define SEEDBOX_MAYBE_UNUSED [[maybe_unused]]
  #else
    #define SEEDBOX_MAYBE_UNUSED
  #endif
#else
  #define SEEDBOX_MAYBE_UNUSED
#endif

