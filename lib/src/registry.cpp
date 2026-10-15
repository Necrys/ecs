#include "registry.h"

#include <algorithm>

namespace ecs {

registry::registry() :
  m_entityIdCounter( 0 ),
  m_rebuildDependencyTree( false ) {
}

entity registry::allocate() {
  eid_t id = entity::bad_id;
  if ( m_freeEntityIdQueue.empty() ) {
    if ( m_entityIdCounter == entity::bad_id ) {
      throw std::runtime_error( "No free entity available" );
    }
    id = m_entityIdCounter++;
  } else {
    id = m_freeEntityIdQueue.front();
    m_freeEntityIdQueue.pop_front();
    m_freeEntityIds.erase( id );
  }

  return entity{ id, this };
}

void registry::deallocate( const entity& e ) {
  deallocate( e.id() );
}

void registry::deallocate( const eid_t id ) {
  // The set holds exactly the ids in the queue, so a single lookup answers both
  // "already released" and the membership the queue needs.
  const auto inserted = m_freeEntityIds.insert( id );
  if ( !inserted.second ) {
    return;
  }

  m_components.remove_all_components( id );
  m_freeEntityIdQueue.push_back( id );
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
    for ( ; it != toUpdate.end(); ++it ) {
      if ( *it && ( *it )->ready() ) {
        break;
      }
    }

    if ( it == toUpdate.end() ) {
      // possible loop, end processing
      return;
    }

    if ( ( *it )->enabled() ) {
      ( *it )->update();
    }

    toUpdate.erase( it );
  }

  for ( auto& s : m_systems ) {
    s->reset_update();
  }
}

bool registry::rebuild_system_dependency_tree() {
  for ( size_t sid = 0; sid < m_systems.size(); ++sid ) {
    // Loop bounds, so the index is in range by construction.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    if ( !m_systems[ sid ] ) {
      continue;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    m_systems[ sid ]->clear_dependencies();

    if ( sid >= m_systemDependenciesMatrix.size() ) {
      break;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    const auto& deps = m_systemDependenciesMatrix[ sid ];
    for ( auto it = deps.cbegin(); it != deps.cend(); ++it ) {
      // A dependency that was never added as a system has an id the vector has
      // never grown to; reading it would be out of bounds. Ignoring it matches
      // how a dependency cycle is treated today - D10 tracks making both loud.
      if ( *it >= m_systems.size() ) {
        continue;
      }

      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      m_systems[ sid ]->add_dependency( m_systems[ *it ] );
    }
  }

  m_rebuildDependencyTree = false;

  return true;
}

bool registry::is_system_active( const size_t sid ) const {
  if ( sid >= m_systems.size() ) {
    return false;
  }

  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  const auto s = m_systems[ sid ];
  return s && s->enabled();
}

}
