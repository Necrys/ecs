#pragma once

#include "storage.h"

#include <list>

namespace ecs {
  class entity;

  class registry {
  public:
    registry();

    registry( const registry& ) = delete;
    registry( registry&& ) = delete;
    registry& operator= ( const registry& ) = delete;
    registry& operator= ( registry&& ) = delete;

    // entity management
    entity allocate();
    void deallocate( const entity& e );
    void deallocate( const eid_t id );

    components_storage& components();

  private:
    std::list< eid_t > m_freeEntityIds;
    components_storage m_components;
    eid_t              m_entityIdCounter;
  };
}

#include "registry.hpp"
