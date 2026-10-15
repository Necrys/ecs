// The harness's own translation unit: the out-of-line escape used by bench::keep on
// MSVC, and the state the harness keeps between calls.
//
// bench::keep is declared in harness.hpp and defined here so that it cannot be
// inlined or reasoned about: the compiler sees only a call with a volatile pointer,
// which is enough to prevent the value that produced it from being discarded.
//
// The two accessors below are declared in harness.hpp and defined here because a
// mutable object defined in a header has vague linkage, which clang reports under
// -Wunique-object-duplication - and it is right to: every benchmark binary would
// carry its own copy of an object the header says is shared.
#include "harness.hpp"

namespace bench {

uint64_t& last_cycles() {
  static uint64_t value = 0;
  return value;
}

uint64_t& checksum() {
  static uint64_t value = 0;
  return value;
}

} // namespace bench

#if defined( _MSC_VER )

extern "C" void bench_keep_alive( void const volatile* value ) {
  // The write is the whole point: it forces the caller's value into a volatile
  // location the compiler cannot reason about. The read-back keeps the variable
  // observably used, which clang otherwise warns about.
  static void const volatile* volatile sink = nullptr;
  sink = value;
  ( void )sink;
}

#endif
