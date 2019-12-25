#include "registry.h"

#include <algorithm>

namespace ecs {

registry::registry():
  m_entityIdCounter( 0 ),
  m_rebuildDependencyTree( false ) {
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

void registry::update() {
  for ( auto& h : m_handlers ) {
    if ( h ) {
      h->handle_all();
    }
  }

  if ( m_rebuildDependencyTree ) {
    rebuild_system_dependency_tree();
  }

  std::vector< std::shared_ptr< system_base > > toUpdate( m_systems.begin(), m_systems.end() );
  while ( !toUpdate.empty() ) {
    auto it = toUpdate.begin();
    for (; it != toUpdate.end(); ++it ) {
      if ( *it && (*it)->ready() ) {
        break;
      }
    }

    if ( it == toUpdate.end() ) {
      // possible loop, end processing
      return;
    }

    if ( (*it)->enabled() ) {
      (*it)->update();
    }

    toUpdate.erase( it );
  }

  for ( auto& s: m_systems ) {
    s->reset_update();
  }
}

bool registry::rebuild_system_dependency_tree() {
  for ( size_t sid = 0; sid < m_systems.size(); ++sid ) {
    if ( !m_systems[ sid ] ) {
      continue;
    }

    m_systems[ sid ]->clear_dependencies();

    if ( sid >= m_systemDependenciesMatrix.size() ) {
      break;
    }

    const auto& deps = m_systemDependenciesMatrix[ sid ];
    for ( auto it = deps.cbegin(); it != deps.cend(); ++it ) {
      m_systems[ sid ]->add_dependency( m_systems[ *it ] );
    }
  }

  m_rebuildDependencyTree = false;

  return true;
}

}
