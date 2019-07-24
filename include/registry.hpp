#include "registry.h"
#include "entity.h"
#include "system.h"
#include "events.h"

#include <exception>
#include <memory>

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

template < typename S, typename... Args >
system_configure< system_wrapper< S > > registry::add_system( Args&& ...args ) {
  const auto sid = type_collection< systems >::type_id< S >();
  if ( m_systems.size() <= sid ) {
    m_systems.resize( sid + 1 );
  }

  if ( !m_systems[ sid ] ) {
    m_systems[ sid ] = std::make_shared< system_wrapper< S, Args... > >( std::forward< Args >( args )... );
  }

  return system_configure< system_wrapper< S > >( static_cast< system_wrapper< S >& >( *m_systems[ sid ] ), *this );
}

template < typename S >
system_wrapper< S >& registry::get_system() {
  const auto sid = type_collection< systems >::type_id< S >();
  return static_cast< system_wrapper< S >& >( *m_systems[ sid ] );
}

template < typename S, typename E >
void registry::register_system( system_wrapper< S >& system ) {
  const auto sid = type_collection< systems >::type_id< S >();
  auto& h = get_or_create_handler< E >();
  h.subscribe( [ &system ]( const E& e ){
    system.get().handle_event( e );
  }, sid );
}

template < typename S, typename E, typename... Rs,
  class = typename std::enable_if< ( sizeof...( Rs ) > 0 ) >::type >
void registry::register_system( system_wrapper< S >& system ) {
  const auto sid = type_collection< systems >::type_id< S >();
  auto& h = get_or_create_handler< E >();
  h.subscribe( [ &system ]( const E& e ){
    system.get().handle_event( e );
  }, sid );

  register_system< S, Rs... >( system );
}

template < typename S >
void registry::add_system_dependencies( const size_t sid ) {
  const auto dep_sid = type_collection< systems >::type_id< S >();
  m_systemDependenciesMatrix[ sid ].emplace( dep_sid );
  m_rebuildDependencyTree = true;
}

template < typename S, typename... Rs,
  class = typename std::enable_if< ( sizeof...( Rs ) > 0 ) >::type >
void registry::add_system_dependencies( const size_t sid ) {
  const auto dep_sid = type_collection< systems >::type_id< S >();
  m_systemDependenciesMatrix[ sid ].emplace( dep_sid );
  add_system_dependencies< Rs... >( sid );
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

template < typename E >
event_handler< E >& registry::get_or_create_handler() {
  const auto eid = type_collection< events >::type_id< E >();
  if ( m_handlers.size() <= eid ) {
    m_handlers.resize( eid + 1 );
  }

  if ( !m_handlers[ eid ] ) {
    m_handlers[ eid ] = std::unique_ptr< event_handler< E > >( new event_handler< E >( *this ) );
  }

  return static_cast< event_handler< E >& >( *m_handlers[ eid ] );
}

template < typename E, typename... Args >
void registry::push_event( Args&& ...args ) {
  auto eid = type_collection< events >::type_id< E >();
  if ( m_handlers.size() <= eid || !m_handlers[ eid ] ) {
    return;
  }

  static_cast< event_handler< E >& >( *m_handlers[ eid ] ).enqueue( std::forward< Args >( args )... );
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

}
