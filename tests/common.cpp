// The one part of the test helpers that belongs in a translation unit rather than a
// header.
//
// A test registers systems and then asserts the order they ran in. The registry
// constructs its systems itself, so there is nowhere to pass that state in - it has
// to be shared. Defined here it is an ordinary object with a single home; defined in
// the header it would be a mutable object with vague linkage, which clang reports
// under -Wunique-object-duplication, and rightly so for anything linked into a
// shared library.
#include "common.h"

update_log& updates() {
  static update_log log;
  return log;
}
