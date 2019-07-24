#pragma once

#include "registry.h"
#include "types.h"

#include <cstdint>
#include <limits>

namespace ecs {
  class entity {
  public:
    static const eid_t bad_id = std::numeric_limits< eid_t >::max();

    entity( const eid_t id, registry* owner );
    entity( const entity& e );
    entity( entity&& e );

    entity& operator=( const entity& e );
    entity& operator=( entity&& e );

    eid_t id() const;

    registry* owner();

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

  public:
    registry* m_owner;
    eid_t     m_id;
  };
}

#include "entity.hpp"
