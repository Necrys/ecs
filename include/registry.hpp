#include "registry.h"
#include "entity.h"

#include <exception>

namespace ecs {

registry::registry():
  m_entityIdCounter( 0 ) {
}

entity registry::allocate() {
  eid_t id;
  if ( m_freeEntityIds.empty() ) {
    if ( m_entityIdCounter == entity::bad_id ) {
      throw std::runtime_error( "No free entity available" );
    }
    id = m_entityIdCounter++;
  } else {
    id = m_freeEntityIds.front();
    m_freeEntityIds.pop_front();
  }

  return entity( id, this );
}

void registry::deallocate( const entity& e ) {
  deallocate( e.id() );
}

void registry::deallocate( const eid_t id ) {
  const auto& it = std::find( m_freeEntityIds.begin(), m_freeEntityIds.end(), id );
  if ( it != m_freeEntityIds.end() ) {
    return;
  }

  m_components.remove_all_components( id );
  m_freeEntityIds.push_back( id );
}

components_storage& registry::components() {
  return m_components;
}

}
