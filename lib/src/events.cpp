// The non-template half of the event machinery: the base that lets a queue be held
// without knowing the event type, and the one query the handlers make of the
// registry.
//
// The base destructor is defined here, out of line, because a class whose virtual
// members are all inline has no key function: clang then emits the vtable as a weak
// symbol in every translation unit that uses the class, and says so under
// -Wweak-vtables. This definition is that key function.
//
// registry.h is included here rather than in events.h: it needs events.h, so the
// dependency can only run in this direction, and this is the translation unit where
// the incomplete type becomes complete.
#include "events.h"

#include "registry.h"

namespace ecs {

event_handler_base::~event_handler_base() = default;

namespace detail {

bool system_is_active( const registry& r, const size_t sid ) {
  return r.is_system_active( sid );
}

} // namespace detail

} // namespace ecs
