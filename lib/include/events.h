#pragma once

#include <functional>

namespace ecs {

struct events {};
class registry;

namespace detail {

// Whether the system that subscribed is still there and enabled. Declared here and
// defined in lib/src/events.cpp, where registry.h is available: the handler has to
// ask the registry, and going through a free function keeps this header free of
// registry.h - which it could not include anyway, since registry.h includes this
// one. The call sits in a template, so the dependency stays where it belongs, at the
// instantiation rather than at the include.
bool system_is_active( const registry& r, size_t sid );

} // namespace detail

class event_handler_base {
public:
  // Out of line for the same reason as the other polymorphic bases: one home for
  // the vtable rather than a weak copy per translation unit.
  virtual ~event_handler_base();

  // The rest of the set, declared so that the class states how it is copied and
  // moved as well as how it dies: the rule of five, applied to a base that is never
  // copied by itself but is part of a handler that may be.
  event_handler_base() = default;
  event_handler_base( const event_handler_base& ) = default;
  event_handler_base( event_handler_base&& ) = default;
  event_handler_base& operator=( const event_handler_base& ) = default;
  event_handler_base& operator=( event_handler_base&& ) = default;

  virtual void handle_all() = 0;
};

template < typename E >
struct system_event_handler_callback {
  std::function< void( const E& ) > f;
  size_t sid;
};

template < typename E >
class event_handler : public event_handler_base {
public:
  event_handler( registry& r );

  template < typename F >
  void subscribe( F&& f, size_t sid );

  template < typename... Args >
  void enqueue( Args&&... args );

  void handle_all() override;

private:
  std::vector< system_event_handler_callback< E > > m_callbacks;
  std::vector< E > m_events[ 2 ];
  size_t m_current{ 0 };
  registry& m_registry;
};

}

#include "events.hpp"
