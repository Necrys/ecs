#pragma once

#include "registry.h"
#include "types.h"

#include <cstdint>
#include <limits>

namespace ecs {
class entity {
public:
  static const eid_t bad_id = std::numeric_limits< eid_t >::max();

  entity( eid_t id, registry* owner );
  entity( const entity& e );

  // Copying a handle is cheap and must stay available; moving one cannot throw
  // and says so, so that containers holding entities move instead of copying.
  entity( entity&& e ) noexcept;

  entity& operator=( const entity& e );
  entity& operator=( entity&& e ) noexcept;

  // Declared rather than left implicit, because the other four are: a type that
  // says how it is copied and moved should also say how it dies, even when the
  // answer is "the same way as always".
  ~entity() = default;

  [[nodiscard]] eid_t id() const;

  // The registry the handle came from. Const on the handle, not on the
  // registry: the handle does not own it and may hand it out unchanged.
  [[nodiscard]] registry* owner() const;

  /* components management */
  template < typename T, typename... Ts >
  T& add( Ts&&... ts );

  template < typename T >
  T* get();

  template < typename T >
  const T* get() const;

  template < typename T, typename... Ts >
  T& set( Ts&&... ts );

  template < typename T >
  void remove();

  registry* m_owner;
  eid_t m_id;
};
}

#include "entity.hpp"
