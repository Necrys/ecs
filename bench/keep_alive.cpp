// The out-of-line escape used by bench::keep on MSVC. Declared in harness.hpp and
// defined here so that it cannot be inlined or reasoned about: the compiler sees
// only a call with a volatile pointer, which is enough to prevent the value that
// produced it from being discarded.
//
// This file is deliberately trivial. It exists so that harness.hpp needs no
// assembly on MSVC.
#if defined( _MSC_VER )

// The declaration lives in harness.hpp; including it keeps the two from
// drifting apart, which is also what the missing-prototype check asks for.
#include "harness.hpp"

extern "C" void bench_keep_alive( void const volatile* value ) {
  // The write is the whole point: it forces the caller's value into a volatile
  // location the compiler cannot reason about. The read-back keeps the variable
  // observably used, which clang otherwise warns about.
  static void const volatile* volatile sink = nullptr;
  sink = value;
  ( void )sink;
}

#endif
